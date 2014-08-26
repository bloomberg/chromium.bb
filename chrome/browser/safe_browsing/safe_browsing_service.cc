// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_tracker.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"
#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_analyzer.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/metrics_service.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_crypto_delegate.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

#if defined(OS_ANDROID)
#include <string>
#include "base/metrics/field_trial.h"
#endif

using content::BrowserThread;

namespace {

// Filename suffix for the cookie database.
const base::FilePath::CharType kCookiesFile[] = FILE_PATH_LITERAL(" Cookies");

// The default URL prefix where browser fetches chunk updates, hashes,
// and reports safe browsing hits and malware details.
const char* const kSbDefaultURLPrefix =
    "https://safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are issues establishing a connection
// with the server at the primary URL.
const char* const kSbBackupConnectErrorURLPrefix =
    "https://alt1-safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are HTTP-specific issues with the
// server at the primary URL.
const char* const kSbBackupHttpErrorURLPrefix =
    "https://alt2-safebrowsing.google.com/safebrowsing";

// The backup URL prefix used when there are local network specific issues.
const char* const kSbBackupNetworkErrorURLPrefix =
    "https://alt3-safebrowsing.google.com/safebrowsing";

base::FilePath CookieFilePath() {
  return base::FilePath(
      SafeBrowsingService::GetBaseFilename().value() + kCookiesFile);
}

#if defined(FULL_SAFE_BROWSING)
// Returns true if the incident reporting service is enabled via a field trial.
bool IsIncidentReportingServiceEnabled() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      "SafeBrowsingIncidentReportingService");
  return group_name == "Enabled";
}
#endif  // defined(FULL_SAFE_BROWSING)

}  // namespace

class SafeBrowsingURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit SafeBrowsingURLRequestContextGetter(
      SafeBrowsingService* sb_service_);

  // Implementation for net::UrlRequestContextGetter.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~SafeBrowsingURLRequestContextGetter();

 private:
  SafeBrowsingService* const sb_service_;  // Owned by BrowserProcess.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SafeBrowsingURLRequestContextGetter> leak_tracker_;
};

SafeBrowsingURLRequestContextGetter::SafeBrowsingURLRequestContextGetter(
    SafeBrowsingService* sb_service)
    : sb_service_(sb_service),
      network_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)) {
}

SafeBrowsingURLRequestContextGetter::~SafeBrowsingURLRequestContextGetter() {}

net::URLRequestContext*
SafeBrowsingURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(sb_service_->url_request_context_.get());

  return sb_service_->url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SafeBrowsingURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

// static
SafeBrowsingServiceFactory* SafeBrowsingService::factory_ = NULL;

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    return new SafeBrowsingService();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactoryImpl);
};

static base::LazyInstance<SafeBrowsingServiceFactoryImpl>::Leaky
    g_safe_browsing_service_factory_impl = LAZY_INSTANCE_INITIALIZER;

// static
base::FilePath SafeBrowsingService::GetCookieFilePathForTesting() {
  return CookieFilePath();
}

// static
base::FilePath SafeBrowsingService::GetBaseFilename() {
  base::FilePath path;
  bool result = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  return path.Append(chrome::kSafeBrowsingBaseFilename);
}


// static
SafeBrowsingService* SafeBrowsingService::CreateSafeBrowsingService() {
  if (!factory_)
    factory_ = g_safe_browsing_service_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingService();
}

#if defined(OS_ANDROID) && defined(FULL_SAFE_BROWSING)
// static
bool SafeBrowsingService::IsEnabledByFieldTrial() {
  const std::string experiment_name =
      base::FieldTrialList::FindFullName("SafeBrowsingAndroid");
  return experiment_name == "Enabled";
}
#endif

SafeBrowsingService::SafeBrowsingService()
    : protocol_manager_(NULL),
      ping_manager_(NULL),
      enabled_(false) {
}

SafeBrowsingService::~SafeBrowsingService() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingService::Initialize() {
  startup_metric_utils::ScopedSlowStartupUMA
      scoped_timer("Startup.SlowStartupSafeBrowsingServiceInitialize");

  url_request_context_getter_ =
      new SafeBrowsingURLRequestContextGetter(this);

  ui_manager_ = CreateUIManager();

  database_manager_ = CreateDatabaseManager();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SafeBrowsingService::InitURLRequestContextOnIOThread, this,
          make_scoped_refptr(g_browser_process->system_request_context())));

#if defined(FULL_SAFE_BROWSING)
#if !defined(OS_ANDROID)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClientSidePhishingDetection)) {
    csd_service_.reset(safe_browsing::ClientSideDetectionService::Create(
        url_request_context_getter_.get()));
  }
  download_service_.reset(new safe_browsing::DownloadProtectionService(
      this, url_request_context_getter_.get()));
#endif

  if (IsIncidentReportingServiceEnabled()) {
    incident_service_.reset(new safe_browsing::IncidentReportingService(
        this, url_request_context_getter_));
  }
#endif

  // Track the safe browsing preference of existing profiles.
  // The SafeBrowsingService will be started if any existing profile has the
  // preference enabled. It will also listen for updates to the preferences.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
    for (size_t i = 0; i < profiles.size(); ++i) {
      if (profiles[i]->IsOffTheRecord())
        continue;
      AddPrefService(profiles[i]->GetPrefs());
    }
  }

  // Track profile creation and destruction.
  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                       content::NotificationService::AllSources());
  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                       content::NotificationService::AllSources());

#if defined(FULL_SAFE_BROWSING)
  // Register all the delayed analysis to the incident reporting service.
  RegisterAllDelayedAnalysis();
#endif
}

void SafeBrowsingService::ShutDown() {
  // Deletes the PrefChangeRegistrars, whose dtors also unregister |this| as an
  // observer of the preferences.
  STLDeleteValues(&prefs_map_);

  // Remove Profile creation/destruction observers.
  prefs_registrar_.RemoveAll();

  Stop(true);
  // The IO thread is going away, so make sure the ClientSideDetectionService
  // dtor executes now since it may call the dtor of URLFetcher which relies
  // on it.
  csd_service_.reset();
  download_service_.reset();
  incident_service_.reset();

  url_request_context_getter_ = NULL;
  BrowserThread::PostNonNestableTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingService::DestroyURLRequestContextOnIOThread,
                 this));
}

// Binhash verification is only enabled for UMA users for now.
bool SafeBrowsingService::DownloadBinHashNeeded() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(FULL_SAFE_BROWSING)
  return (database_manager_->download_protection_enabled() &&
          ui_manager_->CanReportStats()) ||
      (download_protection_service() &&
       download_protection_service()->enabled());
#else
  return false;
#endif
}

net::URLRequestContextGetter* SafeBrowsingService::url_request_context() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return url_request_context_getter_.get();
}

const scoped_refptr<SafeBrowsingUIManager>&
SafeBrowsingService::ui_manager() const {
  return ui_manager_;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
SafeBrowsingService::database_manager() const {
  return database_manager_;
}

SafeBrowsingProtocolManager* SafeBrowsingService::protocol_manager() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return protocol_manager_;
}

SafeBrowsingPingManager* SafeBrowsingService::ping_manager() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ping_manager_;
}

scoped_ptr<TrackedPreferenceValidationDelegate>
SafeBrowsingService::CreatePreferenceValidationDelegate(
    Profile* profile) const {
#if defined(FULL_SAFE_BROWSING)
  if (incident_service_)
    return incident_service_->CreatePreferenceValidationDelegate(profile);
#endif
  return scoped_ptr<TrackedPreferenceValidationDelegate>();
}

void SafeBrowsingService::RegisterDelayedAnalysisCallback(
    const safe_browsing::DelayedAnalysisCallback& callback) {
#if defined(FULL_SAFE_BROWSING)
  if (incident_service_)
    incident_service_->RegisterDelayedAnalysisCallback(callback);
#endif
}

SafeBrowsingUIManager* SafeBrowsingService::CreateUIManager() {
  return new SafeBrowsingUIManager(this);
}

SafeBrowsingDatabaseManager* SafeBrowsingService::CreateDatabaseManager() {
#if defined(FULL_SAFE_BROWSING)
  return new SafeBrowsingDatabaseManager(this);
#else
  return NULL;
#endif
}

void SafeBrowsingService::RegisterAllDelayedAnalysis() {
  safe_browsing::RegisterBinaryIntegrityAnalysis();
  safe_browsing::RegisterBlacklistLoadAnalysis();
}

void SafeBrowsingService::InitURLRequestContextOnIOThread(
    net::URLRequestContextGetter* system_url_request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!url_request_context_.get());

  scoped_refptr<net::CookieStore> cookie_store(
      content::CreateCookieStore(
          content::CookieStoreConfig(
              CookieFilePath(),
              content::CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
              NULL,
              NULL)));

  url_request_context_.reset(new net::URLRequestContext);
  // |system_url_request_context_getter| may be NULL during tests.
  if (system_url_request_context_getter) {
    url_request_context_->CopyFrom(
        system_url_request_context_getter->GetURLRequestContext());
  }
  url_request_context_->set_cookie_store(cookie_store.get());
}

void SafeBrowsingService::DestroyURLRequestContextOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  url_request_context_->AssertNoURLRequests();

  // Need to do the CheckForLeaks on IOThread instead of in ShutDown where
  // url_request_context_getter_ is cleared,  since the URLRequestContextGetter
  // will PostTask to IOTread to delete itself.
  using base::debug::LeakTracker;
  LeakTracker<SafeBrowsingURLRequestContextGetter>::CheckForLeaks();

  url_request_context_.reset();
}

SafeBrowsingProtocolConfig SafeBrowsingService::GetProtocolConfig() const {
  SafeBrowsingProtocolConfig config;
  // On Windows, get the safe browsing client name from the browser
  // distribution classes in installer util. These classes don't yet have
  // an analog on non-Windows builds so just keep the name specified here.
#if defined(OS_WIN)
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  config.client_name = dist->GetSafeBrowsingName();
#else
#if defined(GOOGLE_CHROME_BUILD)
  config.client_name = "googlechrome";
#else
  config.client_name = "chromium";
#endif

  // Mark client string to allow server to differentiate mobile.
#if defined(OS_ANDROID)
  config.client_name.append("-a");
#elif defined(OS_IOS)
  config.client_name.append("-i");
#endif

#endif  // defined(OS_WIN)
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    return;
  enabled_ = true;

  SafeBrowsingProtocolConfig config = GetProtocolConfig();

#if defined(FULL_SAFE_BROWSING)
  DCHECK(database_manager_.get());
  database_manager_->StartOnIOThread();

  DCHECK(!protocol_manager_);
  protocol_manager_ = SafeBrowsingProtocolManager::Create(
      database_manager_.get(), url_request_context_getter, config);
  protocol_manager_->Initialize();
#endif

  DCHECK(!ping_manager_);
  ping_manager_ = SafeBrowsingPingManager::Create(
      url_request_context_getter, config);
}

void SafeBrowsingService::StopOnIOThread(bool shutdown) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

#if defined(FULL_SAFE_BROWSING)
  database_manager_->StopOnIOThread(shutdown);
#endif
  ui_manager_->StopOnIOThread(shutdown);

  if (enabled_) {
    enabled_ = false;

#if defined(FULL_SAFE_BROWSING)
    // This cancels all in-flight GetHash requests. Note that database_manager_
    // relies on the protocol_manager_ so if the latter is destroyed, the
    // former must be stopped.
    delete protocol_manager_;
    protocol_manager_ = NULL;
#endif
    delete ping_manager_;
    ping_manager_ = NULL;
  }
}

void SafeBrowsingService::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingService::StartOnIOThread, this,
                 url_request_context_getter_));
}

void SafeBrowsingService::Stop(bool shutdown) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingService::StopOnIOThread, this, shutdown));
}

void SafeBrowsingService::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        AddPrefService(profile->GetPrefs());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        RemovePrefService(profile->GetPrefs());
      break;
    }
    default:
      NOTREACHED();
  }
}

void SafeBrowsingService::AddPrefService(PrefService* pref_service) {
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  PrefChangeRegistrar* registrar = new PrefChangeRegistrar();
  registrar->Init(pref_service);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::Bind(&SafeBrowsingService::RefreshState,
                            base::Unretained(this)));
  prefs_map_[pref_service] = registrar;
  RefreshState();
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

void SafeBrowsingService::RefreshState() {
  // Check if any profile requires the service to be active.
  bool enable = false;
  std::map<PrefService*, PrefChangeRegistrar*>::iterator iter;
  for (iter = prefs_map_.begin(); iter != prefs_map_.end(); ++iter) {
    if (iter->first->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      enable = true;
      break;
    }
  }

  if (enable)
    Start();
  else
    Stop(false);

#if defined(FULL_SAFE_BROWSING)
  if (csd_service_)
    csd_service_->SetEnabledAndRefreshState(enable);
  if (download_service_)
    download_service_->SetEnabled(enable);
#endif
}
