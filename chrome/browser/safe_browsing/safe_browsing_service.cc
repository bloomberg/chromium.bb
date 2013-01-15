// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_tracker.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/startup_metric_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

using content::BrowserThread;

namespace {

// Filename suffix for the cookie database.
const FilePath::CharType kCookiesFile[] = FILE_PATH_LITERAL(" Cookies");

// The default URL prefix where browser fetches chunk updates, hashes,
// and reports safe browsing hits and malware details.
const char* const kSbDefaultURLPrefix =
    "https://safebrowsing.google.com/safebrowsing";

FilePath CookieFilePath() {
  return FilePath(
      SafeBrowsingService::GetBaseFilename().value() + kCookiesFile);
}

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
  virtual SafeBrowsingService* CreateSafeBrowsingService() {
    return new SafeBrowsingService();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactoryImpl);
};

static base::LazyInstance<SafeBrowsingServiceFactoryImpl>
    g_safe_browsing_service_factory_impl = LAZY_INSTANCE_INITIALIZER;

// static
FilePath SafeBrowsingService::GetCookieFilePathForTesting() {
  return CookieFilePath();
}

// static
FilePath SafeBrowsingService::GetBaseFilename() {
  FilePath path;
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
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClientSidePhishingDetection)) {
    csd_service_.reset(
        safe_browsing::ClientSideDetectionService::Create(
            url_request_context_getter_));
  }
  download_service_.reset(new safe_browsing::DownloadProtectionService(
      this,
      url_request_context_getter_));
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
  return protocol_manager_;
}

SafeBrowsingPingManager* SafeBrowsingService::ping_manager() const {
  return ping_manager_;
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

void SafeBrowsingService::InitURLRequestContextOnIOThread(
    net::URLRequestContextGetter* system_url_request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!url_request_context_.get());

  scoped_refptr<net::CookieStore> cookie_store = new net::CookieMonster(
      new SQLitePersistentCookieStore(CookieFilePath(), false, NULL),
      NULL);

  url_request_context_.reset(new net::URLRequestContext);
  // |system_url_request_context_getter| may be NULL during tests.
  if (system_url_request_context_getter) {
    url_request_context_->CopyFrom(
        system_url_request_context_getter->GetURLRequestContext());
  }
  url_request_context_->set_cookie_store(cookie_store);
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

void SafeBrowsingService::StartOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    return;
  enabled_ = true;

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
#endif
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  config.disable_auto_update =
      cmdline->HasSwitch(switches::kSbDisableAutoUpdate) ||
      cmdline->HasSwitch(switches::kDisableBackgroundNetworking);
  config.url_prefix =
      cmdline->HasSwitch(switches::kSbURLPrefix) ?
      cmdline->GetSwitchValueASCII(switches::kSbURLPrefix) :
      kSbDefaultURLPrefix;

#if defined(FULL_SAFE_BROWSING)
  DCHECK(database_manager_);
  database_manager_->StartOnIOThread();

  DCHECK(!protocol_manager_);
  protocol_manager_ =
      SafeBrowsingProtocolManager::Create(database_manager_,
                                          url_request_context_getter_,
                                          config);
  protocol_manager_->Initialize();
#endif

  DCHECK(!ping_manager_);
  ping_manager_ =
      SafeBrowsingPingManager::Create(url_request_context_getter_,
                                      config);
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
      base::Bind(&SafeBrowsingService::StartOnIOThread, this));
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
  if (csd_service_.get())
    csd_service_->SetEnabledAndRefreshState(enable);
  if (download_service_.get()) {
    download_service_->SetEnabled(
        enable && !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableImprovedDownloadProtection));
  }
#endif
}
