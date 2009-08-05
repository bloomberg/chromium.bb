// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/sync/personalization.h"

#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "net/url_request/url_request.h"

using sync_api::SyncManager;

// TODO(ncarter): Move these switches into chrome_switches.  They are here
// now because we want to keep them secret during early development.
namespace switches {
  const wchar_t kSyncServiceURL[]  = L"sync-url";
  const wchar_t kSyncServicePort[] = L"sync-port";
  const wchar_t kSyncUserForTest[] = L"sync-user-for-test";
  const wchar_t kSyncPasswordForTest[] = L"sync-password-for-test";
}

// TODO(munjal): Move these preferences to common/pref_names.h.
// Names of various preferences.
namespace prefs {
  const wchar_t kSyncPath[] = L"sync";
  const wchar_t kSyncLastSyncedTime[] = L"sync.last_synced_time";
  const wchar_t kSyncUserName[]       = L"sync.username";
  const wchar_t kSyncHasSetupCompleted[] = L"sync.has_setup_completed";
}

// Top-level path for our network layer DataSource.
static const char kCloudyResourcesPath[] = "resources";
// Path for cloudy:stats page.
static const char kCloudyStatsPath[] = "stats";
// Path for the gaia sync login dialog.
static const char kCloudyGaiaLoginPath[] = "gaialogin";
static const char kCloudyMergeAndSyncPath[] = "mergeandsync";
static const char kCloudyThrobberPath[] = "throbber.png";
static const char kCloudySetupFlowPath[] = "setup";

namespace Personalization {

static std::wstring MakeAuthErrorText(AuthErrorState state) {
  switch (state) {
    case AUTH_ERROR_INVALID_GAIA_CREDENTIALS:
      return L"INVALID_GAIA_CREDENTIALS";
    case AUTH_ERROR_USER_NOT_SIGNED_UP:
      return L"USER_NOT_SIGNED_UP";
    case AUTH_ERROR_CONNECTION_FAILED:
      return L"CONNECTION_FAILED";
    default:
      return std::wstring();
  }
}

bool IsP13NDisabled(Profile* profile) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableP13n))
    return true;
  return !profile || profile->GetProfilePersonalization() == NULL;
}

bool NeedsDOMUI(const GURL& url) {
  return url.SchemeIs(kPersonalizationScheme) &&
      (url.path().find(kCloudyGaiaLoginPath) != std::string::npos) ||
      (url.path().find(kCloudySetupFlowPath) != std::string::npos) ||
      (url.path().find(kCloudyMergeAndSyncPath) != std::string::npos);
}

class CloudyResourceSource : public ChromeURLDataManager::DataSource {
 public:
  CloudyResourceSource()
      : DataSource(kCloudyResourcesPath, MessageLoop::current()) {
  }
  virtual ~CloudyResourceSource() { }

  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string& path) const {
    if (path == kCloudyThrobberPath)
      return "image/png";
    else
      return "text/html";
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CloudyResourceSource);
};

class CloudyStatsSource : public ChromeURLDataManager::DataSource {
 public:
  CloudyStatsSource() : DataSource(kCloudyStatsPath, MessageLoop::current()) {
  }
  virtual ~CloudyStatsSource() { }
  virtual void StartDataRequest(const std::string& path, int request_id) {
    std::string response(MakeCloudyStats());
    scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
    html_bytes->data.resize(response.size());
    std::copy(response.begin(), response.end(), html_bytes->data.begin());
    SendResponse(request_id, html_bytes);
  }
  virtual std::string GetMimeType(const std::string& path) const {
    return "text/html";
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CloudyStatsSource);
};

DOMMessageHandler* CreateNewTabPageHandler(DOMUI* dom_ui) {
  return (new NewTabPageSyncHandler())->Attach(dom_ui);
}

std::string GetNewTabSource() {
  static const StringPiece new_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_P13N_HTML));

  std::string data_uri("data:text/html,");
  data_uri.append(std::string(new_tab_html.data(), new_tab_html.size()));
  return GURL(data_uri).spec();
}

std::wstring GetMenuItemInfoText(Browser* browser) {
  browser->command_updater()->UpdateCommandEnabled(IDC_P13N_INFO, true);
  return kMenuLabelStartSync;
}

void HandleMenuItemClick(Profile* p) {
  // The menu item is enabled either when the sync is not enabled by the user
  // or when it's enabled but the user name is empty. In the former case enable
  // sync. In the latter case, show the login dialog.
  ProfileSyncService* service = p->GetProfilePersonalization()->sync_service();
  DCHECK(service);
  if (service->IsSyncEnabledByUser()) {
      ShowOptionsWindow(OPTIONS_PAGE_USER_DATA, OPTIONS_GROUP_NONE, p);
  } else {
    service->EnableForUser();
  }
}

}  // namespace Personalization

class ProfilePersonalizationImpl : public ProfilePersonalization,
                                   public NotificationObserver {
 public:
  explicit ProfilePersonalizationImpl(Profile *p)
      : profile_(p) {
    // g_browser_process and/or io_thread may not exist during testing.
    if (g_browser_process && g_browser_process->io_thread()) {
      // Add our network layer data source for 'cloudy' URLs.
      // TODO(timsteele): This one belongs in BrowserAboutHandler.
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(&chrome_url_data_manager,
                            &ChromeURLDataManager::AddDataSource,
                            new Personalization::CloudyStatsSource()));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(&chrome_url_data_manager,
                            &ChromeURLDataManager::AddDataSource,
                            new Personalization::CloudyResourceSource()));
    }

    registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                   Source<Profile>(profile_));
  }
  virtual ~ProfilePersonalizationImpl() {}

  // ProfilePersonalization implementation
  virtual ProfileSyncService* sync_service() {
    if (!sync_service_.get())
      InitSyncService();
    return sync_service_.get();
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK_EQ(type.value, NotificationType::BOOKMARK_MODEL_LOADED);
    if (!sync_service_.get())
      InitSyncService();
    registrar_.RemoveAll();
  }

  void InitSyncService() {
    sync_service_.reset(new ProfileSyncService(profile_));
    sync_service_->Initialize();
  }

 private:
  Profile* profile_;
  NotificationRegistrar registrar_;
  scoped_ptr<ProfileSyncService> sync_service_;
  DISALLOW_COPY_AND_ASSIGN(ProfilePersonalizationImpl);
};

namespace Personalization {

void CloudyResourceSource::StartDataRequest(const std::string& path_raw,
                                            int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  if (path_raw == kCloudyThrobberPath) {
    ResourceBundle::GetSharedInstance().LoadImageResourceBytes(IDR_THROBBER,
        &html_bytes->data);
    SendResponse(request_id, html_bytes);
    return;
  }

  std::string response;
  if (path_raw == kCloudyGaiaLoginPath) {
    static const StringPiece html(ResourceBundle::GetSharedInstance()
         .GetRawDataResource(IDR_GAIA_LOGIN_HTML));
    response = html.as_string();
  } else if (path_raw == kCloudyMergeAndSyncPath) {
    static const StringPiece html(ResourceBundle::GetSharedInstance()
         .GetRawDataResource(IDR_MERGE_AND_SYNC_HTML));
    response = html.as_string();
  } else if (path_raw == kCloudySetupFlowPath) {
    static const StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_SYNC_SETUP_FLOW_HTML));
    response = html.as_string();
  }
  // Send the response.
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

ProfilePersonalization* CreateProfilePersonalization(Profile* p) {
  return new ProfilePersonalizationImpl(p);
}

void CleanupProfilePersonalization(ProfilePersonalization* p) {
  if (p) delete p;
}

static void AddBoolDetail(ListValue* details, const std::wstring& stat_name,
                          bool stat_value) {
  DictionaryValue* val = new DictionaryValue;
  val->SetString(L"stat_name", stat_name);
  val->SetBoolean(L"stat_value", stat_value);
  details->Append(val);
}

static void AddIntDetail(ListValue* details, const std::wstring& stat_name,
                         int64 stat_value) {
  DictionaryValue* val = new DictionaryValue;
  val->SetString(L"stat_name", stat_name);
  val->SetString(L"stat_value", FormatNumber(stat_value));
  details->Append(val);
}

std::string MakeCloudyStats() {
  FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return std::string();
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  ProfilePersonalization* p13n_profile = profile->GetProfilePersonalization();
  ProfileSyncService* service = p13n_profile->sync_service();

  DictionaryValue strings;
  if (!service->IsSyncEnabledByUser()) {
    strings.SetString(L"summary", L"SYNC DISABLED");
  } else {
    SyncManager::Status full_status(service->QueryDetailedSyncStatus());

    strings.SetString(L"summary",
        ProfileSyncService::BuildSyncStatusSummaryText(
            full_status.summary));

    strings.Set(L"authenticated",
        new FundamentalValue(full_status.authenticated));
    strings.SetString(L"auth_problem",
        MakeAuthErrorText(service->GetAuthErrorState()));

    strings.SetString(L"time_since_sync", service->GetLastSyncedTimeString());

    ListValue* details = new ListValue();
    strings.Set(L"details", details);
    AddBoolDetail(details, L"Server Up", full_status.server_up);
    AddBoolDetail(details, L"Server Reachable", full_status.server_reachable);
    AddBoolDetail(details, L"Server Broken", full_status.server_broken);
    AddBoolDetail(details, L"Notifications Enabled",
                  full_status.notifications_enabled);
    AddIntDetail(details, L"Notifications Received",
                 full_status.notifications_received);
    AddIntDetail(details, L"Notifications Sent",
                 full_status.notifications_sent);
    AddIntDetail(details, L"Unsynced Count", full_status.unsynced_count);
    AddIntDetail(details, L"Conflicting Count", full_status.conflicting_count);
    AddBoolDetail(details, L"Syncing", full_status.syncing);
    AddBoolDetail(details, L"Syncer Paused", full_status.syncer_paused);
    AddBoolDetail(details, L"Initial Sync Ended",
                  full_status.initial_sync_ended);
    AddBoolDetail(details, L"Syncer Stuck", full_status.syncer_stuck);
    AddIntDetail(details, L"Updates Available", full_status.updates_available);
    AddIntDetail(details, L"Updates Received", full_status.updates_received);
    AddBoolDetail(details, L"Disk Full", full_status.disk_full);
    AddBoolDetail(details, L"Invalid Store", full_status.invalid_store);
    AddIntDetail(details, L"Max Consecutive Errors",
                 full_status.max_consecutive_errors);
  }

  static const StringPiece sync_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_ABOUT_SYNC_HTML));

  return jstemplate_builder::GetTemplateHtml(
      sync_html, &strings , "t" /* template root node id */);
}

}  // namespace Personalization

#endif  // CHROME_PERSONALIZATION
