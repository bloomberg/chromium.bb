// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"

#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

namespace {

void ShowSettingsApiBubble(extensions::SettingsApiOverrideType type,
                           const std::string& extension_id,
                           Profile* profile,
                           views::View* anchor_view,
                           views::BubbleBorder::Arrow arrow) {
  scoped_ptr<extensions::SettingsApiBubbleController> settings_api_bubble(
      new extensions::SettingsApiBubbleController(profile, type));
  if (!settings_api_bubble->ShouldShow(extension_id))
    return;

  extensions::SettingsApiBubbleController* controller =
      settings_api_bubble.get();
  extensions::ExtensionMessageBubbleView* bubble_delegate =
      new extensions::ExtensionMessageBubbleView(
          anchor_view,
          arrow,
          settings_api_bubble.PassAs<
              extensions::ExtensionMessageBubbleController>());
  views::BubbleDelegateView::CreateBubble(bubble_delegate);
  controller->Show(bubble_delegate);
}

}  // namespace

namespace extensions {

void MaybeShowExtensionControlledHomeNotification(Browser* browser) {
#if !defined(OS_WIN)
  return;
#endif

  const Extension* extension = OverridesHomepage(browser->profile(), NULL);
  if (extension) {
    // The bubble will try to anchor itself against the home button
    views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)->
        toolbar()->home_button();
    ShowSettingsApiBubble(BUBBLE_TYPE_HOME_PAGE,
                          extension->id(),
                          browser->profile(),
                          anchor_view,
                          views::BubbleBorder::TOP_LEFT);
  }
}

void MaybeShowExtensionControlledSearchNotification(
    Profile* profile,
    content::WebContents* web_contents,
    const AutocompleteMatch& match) {
#if !defined(OS_WIN)
  return;
#endif

  if (match.provider &&
      match.provider->type() == AutocompleteProvider::TYPE_SEARCH) {
    const extensions::Extension* extension =
        OverridesSearchEngine(profile, NULL);
    if (extension) {
      ToolbarView* toolbar =
          BrowserView::GetBrowserViewForBrowser(
              chrome::FindBrowserWithWebContents(web_contents))->toolbar();
      ShowSettingsApiBubble(BUBBLE_TYPE_SEARCH_ENGINE,
                            extension->id(),
                            profile,
                            toolbar->app_menu(),
                            views::BubbleBorder::TOP_RIGHT);
    }
  }
}

}  // namespace extensions
