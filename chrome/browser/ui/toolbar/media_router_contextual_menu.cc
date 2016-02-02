// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/component_migration_helper.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model_delegate.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#endif  // defined(GOOGLE_CHROME_BUILD)

MediaRouterContextualMenu::MediaRouterContextualMenu(Browser* browser)
    : browser_(browser),
      menu_model_(this) {
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_ABOUT,
                                  IDS_MEDIA_ROUTER_ABOUT);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_LEARN_MORE,
                                  IDS_MEDIA_ROUTER_LEARN_MORE);
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_HELP,
                                  IDS_MEDIA_ROUTER_HELP);
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_REMOVE_TOOLBAR_ACTION,
                                  IDS_EXTENSIONS_UNINSTALL);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
#if defined(GOOGLE_CHROME_BUILD)
  menu_model_.AddCheckItemWithStringId(IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE,
                                       IDS_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE);
#endif  // defined(GOOGLE_CHROME_BUILD)
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_REPORT_ISSUE,
                                  IDS_MEDIA_ROUTER_REPORT_ISSUE);
}

MediaRouterContextualMenu::~MediaRouterContextualMenu() {
}

bool MediaRouterContextualMenu::IsCommandIdChecked(int command_id) const {
#if defined(GOOGLE_CHROME_BUILD)
  if (command_id == IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE) {
    return browser_->profile()->GetPrefs()->GetBoolean(
        prefs::kMediaRouterEnableCloudServices);
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
  return false;
}

bool MediaRouterContextualMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool MediaRouterContextualMenu::IsCommandIdVisible(int command_id) const {
#if defined(GOOGLE_CHROME_BUILD)
  if (command_id == IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE) {
    // Cloud services preference is not set or used if the user is not signed
    // in or has disabled sync.
    if (browser_->profile()->IsSyncAllowed()) {
      SigninManagerBase* signin_manager =
          SigninManagerFactory::GetForProfile(browser_->profile());
      return signin_manager && signin_manager->IsAuthenticated() &&
          ProfileSyncServiceFactory::GetForProfile(
              browser_->profile())->IsSyncActive();
    }
    return false;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
  return true;
}

bool MediaRouterContextualMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void MediaRouterContextualMenu::ExecuteCommand(int command_id,
                                               int event_flags) {
  const char kAboutPageUrl[] =
      "https://www.google.com/chrome/devices/chromecast/";
  const char kCastHelpCenterPageUrl[] =
      "https://support.google.com/chromecast#topic=3058948";
  const char kCastLearnMorePageUrl[] =
      "https://www.google.com/chrome/devices/chromecast/learn.html";

#if defined(GOOGLE_CHROME_BUILD)
  PrefService* pref_service;
#endif  // defined(GOOGLE_CHROME_BUILD)
  switch (command_id) {
    case IDC_MEDIA_ROUTER_ABOUT:
      chrome::ShowSingletonTab(browser_, GURL(kAboutPageUrl));
      break;
#if defined(GOOGLE_CHROME_BUILD)
    case IDC_MEDIA_ROUTER_CLOUD_SERVICES_TOGGLE:
      pref_service = browser_->profile()->GetPrefs();
      pref_service->SetBoolean(prefs::kMediaRouterEnableCloudServices,
          !pref_service->GetBoolean(prefs::kMediaRouterEnableCloudServices));

      // If this has been set before, this is a no-op.
      pref_service->SetBoolean(prefs::kMediaRouterCloudServicesPrefSet,
                               true);
      break;
#endif  // defined(GOOGLE_CHROME_BUILD)
    case IDC_MEDIA_ROUTER_HELP:
      chrome::ShowSingletonTab(browser_, GURL(kCastHelpCenterPageUrl));
      base::RecordAction(base::UserMetricsAction(
          "MediaRouter_Ui_Navigate_Help"));
      break;
    case IDC_MEDIA_ROUTER_LEARN_MORE:
      chrome::ShowSingletonTab(browser_, GURL(kCastLearnMorePageUrl));
      break;
    case IDC_MEDIA_ROUTER_REMOVE_TOOLBAR_ACTION:
      RemoveMediaRouterComponentAction();
      break;
    case IDC_MEDIA_ROUTER_REPORT_ISSUE:
      ReportIssue();
      break;
    default:
      NOTREACHED();
  }
}

void MediaRouterContextualMenu::ReportIssue() {
  // Opens feedback page loaded from the media router extension.
  // This is temporary until feedback UI is redesigned.
  media_router::MediaRouterMojoImpl* media_router =
      static_cast<media_router::MediaRouterMojoImpl*>(
          media_router::MediaRouterFactory::GetApiForBrowserContext(
              static_cast<content::BrowserContext*>(browser_->profile())));
  if (media_router->media_route_provider_extension_id().empty())
    return;
  std::string feedback_url(extensions::kExtensionScheme +
                           std::string(url::kStandardSchemeSeparator) +
                           media_router->media_route_provider_extension_id() +
                           "/feedback.html");
  chrome::ShowSingletonTab(browser_, GURL(feedback_url));
}

void MediaRouterContextualMenu::RemoveMediaRouterComponentAction() {
  ToolbarActionsModel::Get(browser_->profile())->component_migration_helper()
      ->OnActionRemoved(ComponentToolbarActionsFactory::kMediaRouterActionId);
}
