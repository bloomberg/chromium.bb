// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_entry.h"

namespace extensions {

namespace {

void ShowSettingsApiBubble(SettingsApiOverrideType type,
                           Browser* browser,
                           views::View* anchor_view,
                           views::BubbleBorder::Arrow arrow) {
  std::unique_ptr<ExtensionMessageBubbleController> settings_api_bubble(
      new ExtensionMessageBubbleController(
          new SettingsApiBubbleDelegate(browser->profile(), type), browser));
  if (!settings_api_bubble->ShouldShow())
    return;

  // TODO(devlin): This should go through the ToolbarActionsBar.
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_view,
      std::unique_ptr<ToolbarActionsBarBubbleDelegate>(
          new ExtensionMessageBubbleBridge(std::move(settings_api_bubble))));
  bubble->set_arrow(arrow);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show();
}

}  // namespace

void MaybeShowExtensionControlledHomeNotification(Browser* browser) {
#if !defined(OS_WIN)
  return;
#endif

  // The bubble will try to anchor itself against the home button
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)->
      toolbar()->home_button();
  ShowSettingsApiBubble(BUBBLE_TYPE_HOME_PAGE,
                        browser,
                        anchor_view,
                        views::BubbleBorder::TOP_LEFT);
}

void MaybeShowExtensionControlledSearchNotification(
    Profile* profile,
    content::WebContents* web_contents,
    AutocompleteMatch::Type match_type) {
#if !defined(OS_WIN)
  return;
#endif

  if (AutocompleteMatch::IsSearchType(match_type) &&
      match_type != AutocompleteMatchType::SEARCH_OTHER_ENGINE) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    ToolbarView* toolbar =
        BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
    ShowSettingsApiBubble(BUBBLE_TYPE_SEARCH_ENGINE, browser,
                          toolbar->app_menu_button(),
                          views::BubbleBorder::TOP_RIGHT);
  }
}

void MaybeShowExtensionControlledNewTabPage(
    Browser* browser, content::WebContents* web_contents) {
#if !defined(OS_WIN)
  return;
#endif

  content::NavigationEntry* entry =
      web_contents->GetController().GetActiveEntry();
  if (!entry)
    return;
  GURL active_url = entry->GetURL();
  if (!active_url.SchemeIs("chrome-extension"))
    return;  // Not a URL that we care about.

  // See if the current active URL matches a transformed NewTab URL.
  GURL ntp_url(chrome::kChromeUINewTabURL);
  bool ignored_param;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url,
      web_contents->GetBrowserContext(),
      &ignored_param);
  if (ntp_url != active_url)
    return;  // Not being overridden by an extension.

  std::unique_ptr<ExtensionMessageBubbleController> ntp_overridden_bubble(
      new ExtensionMessageBubbleController(
          new NtpOverriddenBubbleDelegate(browser->profile()), browser));
  if (!ntp_overridden_bubble->ShouldShow())
    return;

  ToolbarActionsBarBubbleViews* bubble =
      new ToolbarActionsBarBubbleViews(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar()
          ->app_menu_button(),
      std::unique_ptr<ToolbarActionsBarBubbleDelegate>(
          new ExtensionMessageBubbleBridge(std::move(ntp_overridden_bubble))));
  bubble->set_arrow(views::BubbleBorder::TOP_RIGHT);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show();
}

}  // namespace extensions
