// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/safe_browsing/ping_manager.h"
#include "ios/chrome/browser/safe_browsing/ui_manager.h"
#include "ios/web/public/web_thread.h"
#include "net/cookies/cookie_monster.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

namespace {

const base::FilePath::CharType kIOSChromeSafeBrowsingBaseFilename[] =
    FILE_PATH_LITERAL("Safe Browsing");

// Filename suffix for the cookie database.
const base::FilePath::CharType kCookiesFile[] = FILE_PATH_LITERAL(" Cookies");

// The default URL prefix where browser fetches chunk updates, hashes,
// and reports safe browsing hits and malware details.
const char kSbDefaultURLPrefix[] =
    "https://safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are issues establishing a connection
// with the server at the primary URL.
const char kSbBackupConnectErrorURLPrefix[] =
    "https://alt1-safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are HTTP-specific issues with the
// server at the primary URL.
const char kSbBackupHttpErrorURLPrefix[] =
    "https://alt2-safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are local network specific issues.
const char kSbBackupNetworkErrorURLPrefix[] =
    "https://alt3-safebrowsing.google.com/safebrowsing";

}  // namespace

class SafeBrowsingURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit SafeBrowsingURLRequestContextGetter(
      scoped_refptr<net::URLRequestContextGetter> system_context_getter);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Shuts down any pending requests using the getter, and sets |shut_down_| to
  // true.
  void ServiceShuttingDown();

 protected:
  ~SafeBrowsingURLRequestContextGetter() override;

 private:
  bool shut_down_;

  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;

  scoped_ptr<net::URLRequestContext> safe_browsing_request_context_;

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
};

SafeBrowsingURLRequestContextGetter::SafeBrowsingURLRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> system_context_getter)
    : shut_down_(false),
      system_context_getter_(system_context_getter),
      network_task_runner_(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)) {}

net::URLRequestContext*
SafeBrowsingURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // Check if the service has been shut down.
  if (shut_down_)
    return nullptr;

  if (!safe_browsing_request_context_) {
    safe_browsing_request_context_.reset(new net::URLRequestContext());
    // May be NULL in unit tests.
    if (system_context_getter_) {
      safe_browsing_request_context_->CopyFrom(
          system_context_getter_->GetURLRequestContext());
    }

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            base::FilePath(SafeBrowsingService::GetBaseFilename().value() +
                           kCookiesFile),
            network_task_runner_,
            web::WebThread::GetBlockingPool()->GetSequencedTaskRunner(
                web::WebThread::GetBlockingPool()->GetSequenceToken()),
            false, nullptr));

    safe_browsing_request_context_->set_cookie_store(
        new net::CookieMonster(sqlite_store.get(), nullptr));
  }

  return safe_browsing_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SafeBrowsingURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void SafeBrowsingURLRequestContextGetter::ServiceShuttingDown() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  shut_down_ = true;
  URLRequestContextGetter::NotifyContextShuttingDown();
  safe_browsing_request_context_.reset();
}

SafeBrowsingURLRequestContextGetter::~SafeBrowsingURLRequestContextGetter() {}

// static
SafeBrowsingServiceFactory* SafeBrowsingService::factory_ = nullptr;

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  SafeBrowsingService* CreateSafeBrowsingService() override {
    return new SafeBrowsingService();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactoryImpl);
};

static base::LazyInstance<SafeBrowsingServiceFactoryImpl>::Leaky
    g_safe_browsing_service_factory_impl = LAZY_INSTANCE_INITIALIZER;

// static
base::FilePath SafeBrowsingService::GetBaseFilename() {
  base::FilePath path;
  bool result = PathService::Get(ios::DIR_USER_DATA, &path);
  DCHECK(result);
  return path.Append(kIOSChromeSafeBrowsingBaseFilename);
}

// static
SafeBrowsingService* SafeBrowsingService::CreateSafeBrowsingService() {
  if (!factory_)
    factory_ = g_safe_browsing_service_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingService();
}

SafeBrowsingService::SafeBrowsingService()
    : ping_manager_(nullptr), enabled_(false), enabled_by_prefs_(false) {}

SafeBrowsingService::~SafeBrowsingService() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingService::Initialize() {
  url_request_context_getter_ = new SafeBrowsingURLRequestContextGetter(
      GetApplicationContext()->GetSystemURLRequestContext());

  ui_manager_ = CreateUIManager();

  // Track the safe browsing preference of existing browser states.
  // The SafeBrowsingService will be started if any existing browser state has
  // the preference enabled. It will also listen for updates to the preferences.
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  DCHECK(browser_state_manager);

  std::vector<ios::ChromeBrowserState*> browser_states =
      browser_state_manager->GetLoadedBrowserStates();
  DCHECK_GT(browser_states.size(), 0u);
  for (ios::ChromeBrowserState* browser_state : browser_states) {
    if (browser_state->IsOffTheRecord())
      continue;
    AddPrefService(browser_state->GetPrefs());
  }
}

void SafeBrowsingService::ShutDown() {
  // Deletes the PrefChangeRegistrars, whose dtors also unregister |this| as an
  // observer of the preferences.
  STLDeleteValues(&prefs_map_);

  Stop(true);

  // Since URLRequestContextGetters are refcounted, can't count on clearing
  // |url_request_context_getter_| to delete it, so need to shut it down first,
  // which will cancel any requests that are currently using it, and prevent
  // new requests from using it as well.
  web::WebThread::PostNonNestableTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingURLRequestContextGetter::ServiceShuttingDown,
                 url_request_context_getter_));

  // Release the URLRequestContextGetter after passing it to the IOThread.  It
  // has to be released now rather than in the destructor because it can only
  // be deleted on the IOThread, and the SafeBrowsingService outlives the IO
  // thread.
  url_request_context_getter_ = nullptr;
}

// Binhash verification is only enabled for UMA users for now.
bool SafeBrowsingService::DownloadBinHashNeeded() const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  return false;
}

net::URLRequestContextGetter* SafeBrowsingService::url_request_context() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return url_request_context_getter_.get();
}

const scoped_refptr<SafeBrowsingUIManager>& SafeBrowsingService::ui_manager()
    const {
  return ui_manager_;
}

SafeBrowsingPingManager* SafeBrowsingService::ping_manager() const {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  return ping_manager_;
}

SBThreatType SafeBrowsingService::CheckResponseFromProxyRequestHeaders(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  SBThreatType result = SB_THREAT_TYPE_SAFE;

  if (!headers.get())
    return result;

  // If safebrowsing service is disabled, don't do any check.
  if (!enabled())
    return result;

  if (headers->HasHeader("X-Phishing-Url"))
    result = SB_THREAT_TYPE_URL_PHISHING;
  else if (headers->HasHeader("X-Malware-Url"))
    result = SB_THREAT_TYPE_URL_MALWARE;

  return result;
}

SafeBrowsingUIManager* SafeBrowsingService::CreateUIManager() {
  return new SafeBrowsingUIManager(this);
}

SafeBrowsingProtocolConfig SafeBrowsingService::GetProtocolConfig() const {
  SafeBrowsingProtocolConfig config;
#if defined(GOOGLE_CHROME_BUILD)
  config.client_name = "googlechrome";
#else
  config.client_name = "chromium";
#endif

  // Mark client string to allow server to differentiate mobile.
  config.client_name.append("-i");

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  config.disable_auto_update =
      cmdline->HasSwitch(switches::kSbDisableAutoUpdate) ||
      cmdline->HasSwitch(switches::kDisableBackgroundNetworking);
  config.url_prefix = kSbDefaultURLPrefix;
  config.backup_connect_error_url_prefix = kSbBackupConnectErrorURLPrefix;
  config.backup_http_error_url_prefix = kSbBackupHttpErrorURLPrefix;
  config.backup_network_error_url_prefix = kSbBackupNetworkErrorURLPrefix;

  return config;
}

void SafeBrowsingService::StartOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  if (enabled_)
    return;
  enabled_ = true;

  SafeBrowsingProtocolConfig config = GetProtocolConfig();

  DCHECK(!ping_manager_);
  ping_manager_ =
      SafeBrowsingPingManager::Create(url_request_context_getter, config);
}

void SafeBrowsingService::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  ui_manager_->StopOnIOThread(shutdown);

  if (enabled_) {
    enabled_ = false;

    delete ping_manager_;
    ping_manager_ = nullptr;
  }
}

void SafeBrowsingService::Start() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                           base::Bind(&SafeBrowsingService::StartOnIOThread,
                                      this, url_request_context_getter_));
}

void SafeBrowsingService::Stop(bool shutdown) {
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingService::StopOnIOThread, this, shutdown));
}

void SafeBrowsingService::AddPrefService(PrefService* pref_service) {
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  PrefChangeRegistrar* registrar = new PrefChangeRegistrar();
  registrar->Init(pref_service);
  registrar->Add(
      prefs::kSafeBrowsingEnabled,
      base::Bind(&SafeBrowsingService::RefreshState, base::Unretained(this)));
  // ClientSideDetectionService will need to be refresh the models
  // renderers have if extended-reporting changes.
  registrar->Add(
      prefs::kSafeBrowsingExtendedReportingEnabled,
      base::Bind(&SafeBrowsingService::RefreshState, base::Unretained(this)));
  prefs_map_[pref_service] = registrar;
  RefreshState();

  // Record the current pref state.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.General",
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.Pref.Extended",
      pref_service->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
}

void SafeBrowsingService::RemovePrefService(PrefService* pref_service) {
  if (prefs_map_.find(pref_service) != prefs_map_.end()) {
    delete prefs_map_[pref_service];
    prefs_map_.erase(pref_service);
    RefreshState();
  } else {
    NOTREACHED();
  }
}

scoped_ptr<SafeBrowsingService::StateSubscription>
SafeBrowsingService::RegisterStateCallback(
    const base::Callback<void(void)>& callback) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return state_callback_list_.Add(callback);
}

void SafeBrowsingService::RefreshState() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  // Check if any browser state requires the service to be active.
  bool enable = false;
  std::map<PrefService*, PrefChangeRegistrar*>::iterator iter;
  for (iter = prefs_map_.begin(); iter != prefs_map_.end(); ++iter) {
    if (iter->first->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      enable = true;
      break;
    }
  }

  enabled_by_prefs_ = enable;

  if (enable)
    Start();
  else
    Stop(false);

  state_callback_list_.Notify();
}

void SafeBrowsingService::SendDownloadRecoveryReport(
    const std::string& report) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingService::OnSendDownloadRecoveryReport, this,
                 report));
}

void SafeBrowsingService::OnSendDownloadRecoveryReport(
    const std::string& report) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  if (ping_manager())
    ping_manager()->ReportThreatDetails(report);
}

}  // namespace safe_browsing
