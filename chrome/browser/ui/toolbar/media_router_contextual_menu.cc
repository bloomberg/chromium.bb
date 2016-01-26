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
  menu_model_.AddItemWithStringId(IDC_MEDIA_ROUTER_REPORT_ISSUE,
                                  IDS_MEDIA_ROUTER_REPORT_ISSUE);
}

MediaRouterContextualMenu::~MediaRouterContextualMenu() {
}

bool MediaRouterContextualMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool MediaRouterContextualMenu::IsCommandIdEnabled(int command_id) const {
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

  switch (command_id) {
    case IDC_MEDIA_ROUTER_ABOUT:
      chrome::ShowSingletonTab(browser_, GURL(kAboutPageUrl));
      break;
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
