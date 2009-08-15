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
#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/sync/auth_error_state.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "net/url_request/url_request.h"

using sync_api::SyncManager;

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

bool NeedsDOMUI(const GURL& url) {
  return url.SchemeIs(chrome::kChromeUIScheme) &&
      (url.path().find(chrome::kSyncGaiaLoginPath) != std::string::npos) ||
      (url.path().find(chrome::kSyncSetupFlowPath) != std::string::npos) ||
      (url.path().find(chrome::kSyncMergeAndSyncPath) != std::string::npos);
}

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
  ProfileSyncService* service = p->GetProfileSyncService();
  if (!service)
    return;  // Incognito has no sync service (or TestingProfile, etc).
  if (service->HasSyncSetupCompleted()) {
      ShowOptionsWindow(OPTIONS_PAGE_CONTENT, OPTIONS_GROUP_NONE, p);
  } else {
    service->EnableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_WRENCH);
  }
}

}  // namespace Personalization

namespace Personalization {

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

std::string AboutSync() {
  FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return std::string();
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  ProfileSyncService* service = profile->GetProfileSyncService();

  DictionaryValue strings;
  if (!service || !service->HasSyncSetupCompleted()) {
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
