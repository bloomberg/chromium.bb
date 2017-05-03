// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include <stdint.h>
#include <unordered_set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/common/features.h"
#include "chrome/grit/browser_resources.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
const base::Feature kCaptivePortalInterstitial{
    "CaptivePortalInterstitial", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCaptivePortalCertificateList{
    "CaptivePortalCertificateList", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kSSLCommonNameMismatchHandling{
    "SSLCommonNameMismatchHandling", base::FEATURE_ENABLED_BY_DEFAULT};

// Default delay in milliseconds before displaying the SSL interstitial.
// This can be changed in tests.
// - If there is a name mismatch and a suggested URL available result arrives
//   during this time, the user is redirected to the suggester URL.
// - If a "captive portal detected" result arrives during this time,
//   a captive portal interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
const int64_t kInterstitialDelayInMilliseconds = 3000;

const char kHistogram[] = "interstitial.ssl_error_handler";

// Adds a message to console after navigation commits and then, deletes itself.
// Also deletes itself if the navigation is stopped.
class CommonNameMismatchRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommonNameMismatchRedirectObserver> {
 public:
  ~CommonNameMismatchRedirectObserver() override {}

  static void AddToConsoleAfterNavigation(
      content::WebContents* web_contents,
      const std::string& request_url_hostname,
      const std::string& suggested_url_hostname) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new CommonNameMismatchRedirectObserver(
            web_contents, request_url_hostname, suggested_url_hostname)));
  }

 private:
  CommonNameMismatchRedirectObserver(content::WebContents* web_contents,
                                     const std::string& request_url_hostname,
                                     const std::string& suggested_url_hostname)
      : WebContentsObserver(web_contents),
        web_contents_(web_contents),
        request_url_hostname_(request_url_hostname),
        suggested_url_hostname_(suggested_url_hostname) {}

  // WebContentsObserver:
  void NavigationStopped() override {
    // Deletes |this|.
    web_contents_->RemoveUserData(UserDataKey());
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& /* load_details */) override {
    web_contents_->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_INFO,
        base::StringPrintf(
            "Redirecting navigation %s -> %s because the server presented a "
            "certificate valid for %s but not for %s. To disable such "
            "redirects launch Chrome with the following flag: "
            "--disable-features=SSLCommonNameMismatchHandling",
            request_url_hostname_.c_str(), suggested_url_hostname_.c_str(),
            suggested_url_hostname_.c_str(), request_url_hostname_.c_str()));
    web_contents_->RemoveUserData(UserDataKey());
  }

  void WebContentsDestroyed() override {
    web_contents_->RemoveUserData(UserDataKey());
  }

  content::WebContents* web_contents_;
  const std::string request_url_hostname_;
  const std::string suggested_url_hostname_;

  DISALLOW_COPY_AND_ASSIGN(CommonNameMismatchRedirectObserver);
};

void RecordUMA(SSLErrorHandler::UMAEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kHistogram, event,
                            SSLErrorHandler::SSL_ERROR_HANDLER_EVENT_COUNT);
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
bool IsCaptivePortalInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalInterstitial);
}

// Reads the SSL error assistant configuration from the resource bundle.
std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
ReadErrorAssistantProtoFromResourceBundle() {
  auto proto = base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(proto);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  base::StringPiece data =
      bundle.GetRawDataResource(IDR_SSL_ERROR_ASSISTANT_PB);
  google::protobuf::io::ArrayInputStream stream(data.data(), data.size());
  return proto->ParseFromZeroCopyStream(&stream) ? std::move(proto) : nullptr;
}

std::unique_ptr<std::unordered_set<std::string>> LoadCaptivePortalCertHashes(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto hashes = base::MakeUnique<std::unordered_set<std::string>>();
  for (const chrome_browser_ssl::CaptivePortalCert& cert :
       proto.captive_portal_cert()) {
    hashes.get()->insert(cert.sha256_hash());
  }
  return hashes;
}
#endif

bool IsSSLCommonNameMismatchHandlingEnabled() {
  return base::FeatureList::IsEnabled(kSSLCommonNameMismatchHandling);
}

// Configuration for SSLErrorHandler.
class ConfigSingleton : public base::NonThreadSafe {
 public:
  ConfigSingleton();

  base::TimeDelta interstitial_delay() const;
  SSLErrorHandler::TimerStartedCallback* timer_started_callback() const;
  base::Clock* clock() const;
  network_time::NetworkTimeTracker* network_time_tracker() const;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Returns true if any of the SHA256 hashes in |ssl_info| is of a captive
  // portal certificate. The set of captive portal hashes is loaded on first
  // use.
  bool IsKnownCaptivePortalCert(const net::SSLInfo& ssl_info);
#endif

  // Testing methods:
  void ResetForTesting();
  void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  void SetTimerStartedCallbackForTesting(
      SSLErrorHandler::TimerStartedCallback* callback);
  void SetClockForTesting(base::Clock* clock);
  void SetNetworkTimeTrackerForTesting(
      network_time::NetworkTimeTracker* tracker);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          error_assistant_proto);
#endif

 private:
  base::TimeDelta interstitial_delay_;

  // Callback to call when the interstitial timer is started. Used for
  // testing.
  SSLErrorHandler::TimerStartedCallback* timer_started_callback_ = nullptr;

  // The clock to use when deciding which error type to display. Used for
  // testing.
  base::Clock* testing_clock_ = nullptr;

  network_time::NetworkTimeTracker* network_time_tracker_ = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Error assistant configuration.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
      error_assistant_proto_;

  // SPKI hashes belonging to certs treated as captive portals. Null until the
  // first time IsKnownCaptivePortalCert() or SetErrorAssistantProto()
  // is called.
  std::unique_ptr<std::unordered_set<std::string>> captive_portal_spki_hashes_;
#endif
};

ConfigSingleton::ConfigSingleton()
    : interstitial_delay_(
          base::TimeDelta::FromMilliseconds(kInterstitialDelayInMilliseconds)) {
}

base::TimeDelta ConfigSingleton::interstitial_delay() const {
  return interstitial_delay_;
}

SSLErrorHandler::TimerStartedCallback* ConfigSingleton::timer_started_callback()
    const {
  return timer_started_callback_;
}

network_time::NetworkTimeTracker* ConfigSingleton::network_time_tracker()
    const {
  return network_time_tracker_ ? network_time_tracker_
                               : g_browser_process->network_time_tracker();
}

base::Clock* ConfigSingleton::clock() const {
  return testing_clock_;
}

void ConfigSingleton::ResetForTesting() {
  interstitial_delay_ =
      base::TimeDelta::FromMilliseconds(kInterstitialDelayInMilliseconds);
  timer_started_callback_ = nullptr;
  network_time_tracker_ = nullptr;
  testing_clock_ = nullptr;
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  error_assistant_proto_.reset();
  captive_portal_spki_hashes_.reset();
#endif
}

void ConfigSingleton::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  interstitial_delay_ = delay;
}

void ConfigSingleton::SetTimerStartedCallbackForTesting(
    SSLErrorHandler::TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  timer_started_callback_ = callback;
}

void ConfigSingleton::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ConfigSingleton::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  network_time_tracker_ = tracker;
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
void ConfigSingleton::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(proto);
  // Ignore versions that are not new.
  if (error_assistant_proto_ &&
      proto->version_id() <= error_assistant_proto_->version_id()) {
    return;
  }
  error_assistant_proto_ = std::move(proto);
  captive_portal_spki_hashes_ =
      LoadCaptivePortalCertHashes(*error_assistant_proto_);
}

bool ConfigSingleton::IsKnownCaptivePortalCert(const net::SSLInfo& ssl_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!captive_portal_spki_hashes_) {
    error_assistant_proto_ = ReadErrorAssistantProtoFromResourceBundle();
    CHECK(error_assistant_proto_);
    captive_portal_spki_hashes_ =
        LoadCaptivePortalCertHashes(*error_assistant_proto_);
  }

  for (const net::HashValue& hash_value : ssl_info.public_key_hashes) {
    if (hash_value.tag != net::HASH_VALUE_SHA256) {
      continue;
    }
    if (captive_portal_spki_hashes_->find(hash_value.ToString()) !=
        captive_portal_spki_hashes_->end()) {
      return true;
    }
  }
  return false;
}
#endif

class SSLErrorHandlerDelegateImpl : public SSLErrorHandler::Delegate {
 public:
  SSLErrorHandlerDelegateImpl(
      content::WebContents* web_contents,
      const net::SSLInfo& ssl_info,
      Profile* const profile,
      int cert_error,
      int options_mask,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback)
      : web_contents_(web_contents),
        ssl_info_(ssl_info),
        profile_(profile),
        cert_error_(cert_error),
        options_mask_(options_mask),
        request_url_(request_url),
        ssl_cert_reporter_(std::move(ssl_cert_reporter)),
        callback_(callback) {}
  ~SSLErrorHandlerDelegateImpl() override;

  // SSLErrorHandler::Delegate methods:
  void CheckForCaptivePortal() override;
  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override;
  void CheckSuggestedUrl(
      const GURL& suggested_url,
      const CommonNameMismatchHandler::CheckUrlCallback& callback) override;
  void NavigateToSuggestedURL(const GURL& suggested_url) override;
  bool IsErrorOverridable() const override;
  void ShowCaptivePortalInterstitial(const GURL& landing_url) override;
  void ShowSSLInterstitial() override;
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override;

 private:
  content::WebContents* web_contents_;
  const net::SSLInfo& ssl_info_;
  Profile* const profile_;
  const int cert_error_;
  const int options_mask_;
  const GURL request_url_;
  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  const base::Callback<void(content::CertificateRequestResultType)> callback_;
};

SSLErrorHandlerDelegateImpl::~SSLErrorHandlerDelegateImpl() {
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
}

void SSLErrorHandlerDelegateImpl::CheckForCaptivePortal() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  captive_portal_service->DetectCaptivePortal();
#else
  NOTREACHED();
#endif
}

bool SSLErrorHandlerDelegateImpl::GetSuggestedUrl(
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandlerDelegateImpl::CheckSuggestedUrl(
    const GURL& suggested_url,
    const CommonNameMismatchHandler::CheckUrlCallback& callback) {
  scoped_refptr<net::URLRequestContextGetter> request_context(
      profile_->GetRequestContext());
  common_name_mismatch_handler_.reset(
      new CommonNameMismatchHandler(request_url_, request_context));

  common_name_mismatch_handler_->CheckSuggestedUrl(suggested_url, callback);
}

void SSLErrorHandlerDelegateImpl::NavigateToSuggestedURL(
    const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

bool SSLErrorHandlerDelegateImpl::IsErrorOverridable() const {
  return SSLBlockingPage::IsOverridable(options_mask_, profile_);
}

void SSLErrorHandlerDelegateImpl::ShowCaptivePortalInterstitial(
    const GURL& landing_url) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Show captive portal blocking page. The interstitial owns the blocking page.
  (new CaptivePortalBlockingPage(web_contents_, request_url_, landing_url,
                                 std::move(ssl_cert_reporter_), ssl_info_,
                                 callback_))
      ->Show();
#else
  NOTREACHED();
#endif
}

void SSLErrorHandlerDelegateImpl::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  (SSLBlockingPage::Create(web_contents_, cert_error_, ssl_info_, request_url_,
                           options_mask_, base::Time::NowFromSystemTime(),
                           std::move(ssl_cert_reporter_), callback_))
      ->Show();
}

void SSLErrorHandlerDelegateImpl::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  // Show bad clock page. The interstitial owns the blocking page.
  (new BadClockBlockingPage(web_contents_, cert_error_, ssl_info_, request_url_,
                            now, clock_state, std::move(ssl_cert_reporter_),
                            callback_))
      ->Show();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SSLErrorHandler);
DEFINE_WEB_CONTENTS_USER_DATA_KEY(CommonNameMismatchRedirectObserver);

static base::LazyInstance<ConfigSingleton>::Leaky g_config =
    LAZY_INSTANCE_INITIALIZER;

void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  DCHECK(!FromWebContents(web_contents));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  SSLErrorHandler* error_handler = new SSLErrorHandler(
      std::unique_ptr<SSLErrorHandler::Delegate>(
          new SSLErrorHandlerDelegateImpl(
              web_contents, ssl_info, profile, cert_error, options_mask,
              request_url, std::move(ssl_cert_reporter), callback)),
      web_contents, profile, cert_error, ssl_info, request_url, callback);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(error_handler));
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::ResetConfigForTesting() {
  g_config.Pointer()->ResetForTesting();
}

// static
void SSLErrorHandler::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  g_config.Pointer()->SetInterstitialDelayForTesting(delay);
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(
    TimerStartedCallback* callback) {
  g_config.Pointer()->SetTimerStartedCallbackForTesting(callback);
}

// static
void SSLErrorHandler::SetClockForTesting(base::Clock* testing_clock) {
  g_config.Pointer()->SetClockForTesting(testing_clock);
}

// static
void SSLErrorHandler::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  g_config.Pointer()->SetNetworkTimeTrackerForTesting(tracker);
}

// static
std::string SSLErrorHandler::GetHistogramNameForTesting() {
  return kHistogram;
}

bool SSLErrorHandler::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

void SSLErrorHandler::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  g_config.Pointer()->SetErrorAssistantProto(std::move(config_proto));
#endif
}

SSLErrorHandler::SSLErrorHandler(
    std::unique_ptr<Delegate> delegate,
    content::WebContents* web_contents,
    Profile* profile,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      web_contents_(web_contents),
      profile_(profile),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      callback_(callback),
      weak_ptr_factory_(this) {}

SSLErrorHandler::~SSLErrorHandler() {
}

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    HandleCertDateInvalidError();
    return;
  }

  const net::CertStatus non_name_mismatch_errors =
      ssl_info_.cert_status ^ net::CERT_STATUS_COMMON_NAME_INVALID;
  const bool only_error_is_name_mismatch =
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      (!net::IsCertStatusError(non_name_mismatch_errors) ||
       net::IsCertStatusMinorError(ssl_info_.cert_status));

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Check known captive portal certificate list if the only error is
  // name-mismatch. If there are multiple errors, it indicates that the captive
  // portal landing page itself will have SSL errors, and so it's not a very
  // helpful place to direct the user to go.
  if (base::FeatureList::IsEnabled(kCaptivePortalCertificateList) &&
      only_error_is_name_mismatch &&
      g_config.Pointer()->IsKnownCaptivePortalCert(ssl_info_)) {
    RecordUMA(CAPTIVE_PORTAL_CERT_FOUND);
    ShowCaptivePortalInterstitial(
        GURL(captive_portal::CaptivePortalDetector::kDefaultURL));
    return;
  }
#endif

  if (IsSSLCommonNameMismatchHandlingEnabled() &&
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      delegate_->IsErrorOverridable()) {
    std::vector<std::string> dns_names;
    ssl_info_.cert->GetSubjectAltName(&dns_names, nullptr);
    GURL suggested_url;
    if (!dns_names.empty() &&
        delegate_->GetSuggestedUrl(dns_names, &suggested_url)) {
      RecordUMA(WWW_MISMATCH_FOUND_IN_SAN);

      // Show the SSL interstitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
      // the only error. Need not check for captive portal in this case.
      // (See the comment below).
      if (!only_error_is_name_mismatch) {
        ShowSSLInterstitial();
        return;
      }
      delegate_->CheckSuggestedUrl(
          suggested_url,
          base::Bind(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                     weak_ptr_factory_.GetWeakPtr()));
      timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                   &SSLErrorHandler::ShowSSLInterstitial);

      if (g_config.Pointer()->timer_started_callback())
        g_config.Pointer()->timer_started_callback()->Run(web_contents_);

      // Do not check for a captive portal in this case, because a captive
      // portal most likely cannot serve a valid certificate which passes the
      // similarity check.
      return;
    }
  }

  // Always listen to captive portal notifications, otherwise build fails
  // because profile_ isn't used. This is a no-op on platforms where
  // captive portal detection is disabled.
  registrar_.Add(this, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents_);
  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info_);
  }

  if (IsCaptivePortalInterstitialEnabled()) {
    delegate_->CheckForCaptivePortal();
    timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                 &SSLErrorHandler::ShowSSLInterstitial);
    if (g_config.Pointer()->timer_started_callback())
      g_config.Pointer()->timer_started_callback()->Run(web_contents_);
    return;
  }
#endif
  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE
                : SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowCaptivePortalInterstitial(landing_url);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this". It also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowSSLInterstitial();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  RecordUMA(SHOW_BAD_CLOCK);
  delegate_->ShowBadClockInterstitial(now, clock_state);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    CommonNameMismatchHandler::SuggestedUrlCheckResult result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    CommonNameMismatchRedirectObserver::AddToConsoleAfterNavigation(
        web_contents(), request_url_.host(), suggested_url.host());
    delegate_->NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}

void SSLErrorHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  DCHECK_EQ(chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT, type);

  timer_.Stop();
  CaptivePortalService::Results* results =
      content::Details<CaptivePortalService::Results>(details).ptr();
  if (results->result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL)
    ShowCaptivePortalInterstitial(results->landing_url);
  else
    ShowSSLInterstitial();
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Destroy the error handler on all new navigations. This ensures that the
  // handler is properly recreated when a hanging page is navigated to an SSL
  // error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
// Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_)
        .Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
  delegate_.reset();
  // Deletes |this| and also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::HandleCertDateInvalidError() {
  const base::TimeTicks now = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(),
               base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                          base::Unretained(this), now));
  // Try kicking off a time fetch to get an up-to-date estimate of the
  // true time. This will only have an effect if network time is
  // unavailable or if there is not already a query in progress.
  //
  // Pass a weak pointer as the callback; if the timer fires before the
  // fetch completes and shows an interstitial, this SSLErrorHandler
  // will be deleted.
  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  if (!tracker->StartTimeFetch(
          base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                     weak_ptr_factory_.GetWeakPtr(), now))) {
    HandleCertDateInvalidErrorImpl(now);
    return;
  }

  if (g_config.Pointer()->timer_started_callback())
    g_config.Pointer()->timer_started_callback()->Run(web_contents_);
}

void SSLErrorHandler::HandleCertDateInvalidErrorImpl(
    base::TimeTicks started_handling_error) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "interstitial.ssl_error_handler.cert_date_error_delay",
      base::TimeTicks::Now() - started_handling_error,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(4),
      50);

  timer_.Stop();
  base::Clock* testing_clock = g_config.Pointer()->clock();
  const base::Time now =
      testing_clock ? testing_clock->Now() : base::Time::NowFromSystemTime();

  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  ssl_errors::ClockState clock_state = ssl_errors::GetClockState(now, tracker);
  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
      clock_state == ssl_errors::CLOCK_STATE_PAST) {
    ShowBadClockInterstitial(now, clock_state);
    return;  // |this| is deleted after showing the interstitial.
  }
  ShowSSLInterstitial();
}
