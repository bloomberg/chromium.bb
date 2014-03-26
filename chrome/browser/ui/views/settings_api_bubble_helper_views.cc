// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"

#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"

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

const extensions::SettingsOverrides* FindOverridingExtension(
    content::BrowserContext* browser_context,
    SettingsApiOverrideType type,
    const Extension** extension) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(browser_context)->enabled_extensions();

  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end();
       ++it) {
    const extensions::SettingsOverrides* settings =
        extensions::SettingsOverrides::Get(*it);
    if (settings) {
      if ((type == BUBBLE_TYPE_HOME_PAGE && settings->homepage) ||
          (type == BUBBLE_TYPE_STARTUP_PAGES &&
              !settings->startup_pages.empty()) ||
          (type == BUBBLE_TYPE_SEARCH_ENGINE && settings->search_engine)) {
        *extension = *it;
        return settings;
      }
    }
  }

  return NULL;
}

const Extension* OverridesHomepage(content::BrowserContext* browser_context,
                                   GURL* home_page_url) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_HOME_PAGE, &extension);
  if (settings && home_page_url)
    *home_page_url = *settings->homepage;
  return extension;
}

const Extension* OverridesStartupPages(content::BrowserContext* browser_context,
                                       std::vector<GURL>* startup_pages) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_STARTUP_PAGES, &extension);
  if (settings && startup_pages) {
    startup_pages->clear();
    for (std::vector<GURL>::const_iterator it = settings->startup_pages.begin();
         it != settings->startup_pages.end();
         ++it)
      startup_pages->push_back(GURL(*it));
  }
  return extension;
}

const Extension* OverridesSearchEngine(
    content::BrowserContext* browser_context,
    api::manifest_types::ChromeSettingsOverrides::Search_provider*
        search_provider) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_SEARCH_ENGINE, &extension);
  if (settings && search_provider)
    search_provider = settings->search_engine.get();
  return extension;
}

}  // namespace extensions
