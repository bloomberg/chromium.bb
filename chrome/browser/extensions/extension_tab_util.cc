// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/url_constants.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

using content::NavigationEntry;
using content::WebContents;

namespace extensions {

namespace {

namespace keys = tabs_constants;

WindowController* GetAppWindowController(const WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);
  if (!registry)
    return nullptr;
  AppWindow* app_window = registry->GetAppWindowForWebContents(contents);
  if (!app_window)
    return nullptr;
  return WindowControllerList::GetInstance()->FindWindowById(
      app_window->session_id().id());
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the window cannot be found by id.
Browser* GetBrowserInProfileWithId(Profile* profile,
                                   const int window_id,
                                   bool include_incognito,
                                   std::string* error_message) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile()
          ? profile->GetOffTheRecordProfile()
          : nullptr;
  for (auto* browser : *BrowserList::GetInstance()) {
    if ((browser->profile() == profile ||
         browser->profile() == incognito_profile) &&
        ExtensionTabUtil::GetWindowId(browser) == window_id &&
        browser->window()) {
      return browser;
    }
  }

  if (error_message)
    *error_message = ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));

  return nullptr;
}

Browser* CreateBrowser(Profile* profile,
                       int window_id,
                       bool user_gesture,
                       std::string* error) {
  Browser::CreateParams params(Browser::TYPE_TABBED, profile, user_gesture);
  Browser* browser = new Browser(params);
  browser->window()->Show();
  return browser;
}

// Use this function for reporting a tab id to an extension. It will
// take care of setting the id to TAB_ID_NONE if necessary (for
// example with devtools).
int GetTabIdForExtensions(const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && !ExtensionTabUtil::BrowserSupportsTabs(browser))
    return -1;
  return SessionTabHelper::IdForTab(web_contents);
}

ExtensionTabUtil::Delegate* g_delegate = nullptr;

}  // namespace

ExtensionTabUtil::OpenTabParams::OpenTabParams()
    : create_browser_if_needed(false) {
}

ExtensionTabUtil::OpenTabParams::~OpenTabParams() {
}

// Opens a new tab for a given extension. Returns nullptr and sets |error| if an
// error occurs.
base::DictionaryValue* ExtensionTabUtil::OpenTab(
    UIThreadExtensionFunction* function,
    const OpenTabParams& params,
    bool user_gesture,
    std::string* error) {
  ChromeExtensionFunctionDetails chrome_details(function);
  Profile* profile = chrome_details.GetProfile();
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params.window_id.get())
    window_id = *params.window_id;

  Browser* browser = GetBrowserFromWindowID(chrome_details, window_id, error);
  if (!browser) {
    if (!params.create_browser_if_needed) {
      return nullptr;
    }
    browser = CreateBrowser(profile, window_id, user_gesture, error);
    if (!browser)
      return nullptr;
  }

  // Ensure the selected browser is tabbed.
  if (!browser->is_type_tabbed() && browser->IsAttemptingToCloseBrowser())
    browser = chrome::FindTabbedBrowser(profile, function->include_incognito());
  if (!browser || !browser->window()) {
    if (error)
      *error = keys::kNoCurrentWindowError;
    return nullptr;
  }

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  WebContents* opener = nullptr;
  Browser* opener_browser = nullptr;
  if (params.opener_tab_id.get()) {
    int opener_id = *params.opener_tab_id;

    if (!ExtensionTabUtil::GetTabById(
            opener_id, profile, function->include_incognito(), &opener_browser,
            nullptr, &opener, nullptr)) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessage(keys::kTabNotFoundError,
                                                base::IntToString(opener_id));
      }
      return nullptr;
    }
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  GURL url;
  if (params.url.get()) {
    std::string url_string = *params.url;
    url = ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string,
                                                       function->extension());
    if (!url.is_valid()) {
      *error =
          ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, url_string);
      return nullptr;
    }
  } else {
    url = GURL(chrome::kChromeUINewTabURL);
  }

  // Don't let extensions crash the browser or renderers.
  if (ExtensionTabUtil::IsKillURL(url)) {
    *error = keys::kNoCrashBrowserError;
    return nullptr;
  }

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = true;
  if (params.active.get())
    active = *params.active;

  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  if (params.pinned.get())
    pinned = *params.pinned;

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window.
  if (url.SchemeIs(kExtensionScheme) &&
      (!function->extension() ||
       !IncognitoInfo::IsSplitMode(function->extension())) &&
      browser->profile()->IsOffTheRecord()) {
    Profile* profile = browser->profile()->GetOriginalProfile();

    browser = chrome::FindTabbedBrowser(profile, false);
    if (!browser) {
      Browser::CreateParams params =
          Browser::CreateParams(Browser::TYPE_TABBED, profile, user_gesture);
      browser = new Browser(params);
      browser->window()->Show();
    }
  }

  if (opener_browser && browser != opener_browser) {
    if (error) {
      *error = "Tab opener must be in the same window as the updated tab.";
    }
    return nullptr;
  }

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  if (params.index.get())
    index = *params.index;

  TabStripModel* tab_strip = browser->tab_strip_model();

  index = std::min(std::max(index, -1), tab_strip->count());

  int add_types = active ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE;
  add_types |= TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  chrome::NavigateParams navigate_params(
      browser, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  chrome::Navigate(&navigate_params);

  // The tab may have been created in a different window, so make sure we look
  // at the right tab strip.
  tab_strip = navigate_params.browser->tab_strip_model();
  int new_index =
      tab_strip->GetIndexOfWebContents(navigate_params.target_contents);
  if (opener) {
    // Only set the opener if the opener tab is in the same tab strip as the
    // new tab.
    if (tab_strip->GetIndexOfWebContents(opener) != TabStripModel::kNoTab)
      tab_strip->SetOpenerOfWebContentsAt(new_index, opener);
  }

  if (active)
    navigate_params.target_contents->SetInitialFocus();

  // Return data about the newly created tab.
  return ExtensionTabUtil::CreateTabObject(navigate_params.target_contents,
                                           tab_strip, new_index,
                                           function->extension())
      ->ToValue()
      .release();
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    ChromeUIThreadExtensionFunction* function,
    int window_id,
    std::string* error) {
  if (window_id == extension_misc::kCurrentWindowId) {
    Browser* result = function->GetCurrentBrowser();
    if (!result || !result->window()) {
      if (error)
        *error = keys::kNoCurrentWindowError;
      return nullptr;
    }
    return result;
  } else {
    return GetBrowserInProfileWithId(function->GetProfile(),
                                     window_id,
                                     function->include_incognito(),
                                     error);
  }
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    const ChromeExtensionFunctionDetails& details,
    int window_id,
    std::string* error) {
  if (window_id == extension_misc::kCurrentWindowId) {
    Browser* result = details.GetCurrentBrowser();
    if (!result || !result->window()) {
      if (error)
        *error = keys::kNoCurrentWindowError;
      return nullptr;
    }
    return result;
  } else {
    return GetBrowserInProfileWithId(details.GetProfile(),
                                     window_id,
                                     details.function()->include_incognito(),
                                     error);
  }
}

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return GetWindowId(browser);
  }
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return SessionTabHelper::IdForTab(web_contents);
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return SessionTabHelper::IdForWindowContainingTab(web_contents);
}

// static
std::unique_ptr<api::tabs::Tab> ExtensionTabUtil::CreateTabObject(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  // If we have a matching AppWindow with a controller, get the tab value
  // from its controller instead.
  WindowController* controller = GetAppWindowController(contents);
  if (controller &&
      (!extension || controller->IsVisibleToExtension(extension))) {
    return controller->CreateTabObject(extension, tab_index);
  }
  std::unique_ptr<api::tabs::Tab> result =
      CreateTabObject(contents, tab_strip, tab_index);
  ScrubTabForExtension(extension, contents, result.get());
  return result;
}

std::unique_ptr<base::ListValue> ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  std::unique_ptr<base::ListValue> tab_list(new base::ListValue());
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(
        CreateTabObject(tab_strip->GetWebContentsAt(i), tab_strip, i, extension)
            ->ToValue());
  }

  return tab_list;
}

// static
std::unique_ptr<api::tabs::Tab> ExtensionTabUtil::CreateTabObject(
    content::WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  // If we have a matching AppWindow with a controller, get the tab value
  // from its controller instead.
  WindowController* controller = GetAppWindowController(contents);
  if (controller)
    return controller->CreateTabObject(nullptr, tab_index);

  if (!tab_strip)
    ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index);
  bool is_loading = contents->IsLoading();
  auto tab_object = base::MakeUnique<api::tabs::Tab>();
  tab_object->id = base::MakeUnique<int>(GetTabIdForExtensions(contents));
  tab_object->index = tab_index;
  tab_object->window_id = GetWindowIdOfTab(contents);
  tab_object->status =
      base::MakeUnique<std::string>(GetTabStatusText(is_loading));
  tab_object->active = tab_strip && tab_index == tab_strip->active_index();
  tab_object->selected = tab_strip && tab_index == tab_strip->active_index();
  tab_object->highlighted = tab_strip && tab_strip->IsTabSelected(tab_index);
  tab_object->pinned = tab_strip && tab_strip->IsTabPinned(tab_index);
  tab_object->audible = base::MakeUnique<bool>(contents->WasRecentlyAudible());
  tab_object->discarded =
      g_browser_process->GetTabManager()->IsTabDiscarded(contents);
  tab_object->auto_discardable =
      g_browser_process->GetTabManager()->IsTabAutoDiscardable(contents);
  tab_object->muted_info = CreateMutedInfo(contents);
  tab_object->incognito = contents->GetBrowserContext()->IsOffTheRecord();
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object->width = base::MakeUnique<int>(contents_size.width());
  tab_object->height = base::MakeUnique<int>(contents_size.height());

  tab_object->url = base::MakeUnique<std::string>(contents->GetURL().spec());
  tab_object->title =
      base::MakeUnique<std::string>(base::UTF16ToUTF8(contents->GetTitle()));
  NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry && entry->GetFavicon().valid) {
    tab_object->fav_icon_url =
        base::MakeUnique<std::string>(entry->GetFavicon().url.spec());
  }
  if (tab_strip) {
    WebContents* opener = tab_strip->GetOpenerOfWebContentsAt(tab_index);
    if (opener) {
      tab_object->opener_tab_id =
          base::MakeUnique<int>(GetTabIdForExtensions(opener));
    }
  }

  return tab_object;
}

// static
std::unique_ptr<api::tabs::MutedInfo> ExtensionTabUtil::CreateMutedInfo(
    content::WebContents* contents) {
  DCHECK(contents);
  std::unique_ptr<api::tabs::MutedInfo> info(new api::tabs::MutedInfo);
  info->muted = contents->IsAudioMuted();
  switch (chrome::GetTabAudioMutedReason(contents)) {
    case TabMutedReason::NONE:
      break;
    case TabMutedReason::AUDIO_INDICATOR:
    case TabMutedReason::CONTENT_SETTING:
    case TabMutedReason::CONTEXT_MENU:
      info->reason = api::tabs::MUTED_INFO_REASON_USER;
      break;
    case TabMutedReason::MEDIA_CAPTURE:
      info->reason = api::tabs::MUTED_INFO_REASON_CAPTURE;
      break;
    case TabMutedReason::EXTENSION:
      info->reason = api::tabs::MUTED_INFO_REASON_EXTENSION;
      info->extension_id.reset(
          new std::string(chrome::GetExtensionIdForMutedTab(contents)));
      break;
  }
  return info;
}

// static
void ExtensionTabUtil::SetPlatformDelegate(Delegate* delegate) {
  // Allow setting it only once (also allow reset to nullptr, but then take
  // special care to free it).
  CHECK(!g_delegate || !delegate);
  g_delegate = delegate;
}

// static
void ExtensionTabUtil::ScrubTabForExtension(const Extension* extension,
                                            content::WebContents* contents,
                                            api::tabs::Tab* tab) {
  bool has_permission = false;
  if (extension) {
    bool api_permission = false;
    std::string url;
    if (contents) {
      api_permission = extension->permissions_data()->HasAPIPermissionForTab(
          GetTabId(contents), APIPermission::kTab);
      url = contents->GetURL().spec();
    } else {
      api_permission =
          extension->permissions_data()->HasAPIPermission(APIPermission::kTab);
      url = *tab->url;
    }
    bool host_permission = extension->permissions_data()
                               ->active_permissions()
                               .HasExplicitAccessToOrigin(GURL(url));
    has_permission = api_permission || host_permission;
  }
  if (!has_permission) {
    tab->url.reset();
    tab->title.reset();
    tab->fav_icon_url.reset();
  }
  if (g_delegate)
    g_delegate->ScrubTabForExtension(extension, contents, tab);
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (auto* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);
    if (index != -1) {
      *tab_strip_model = tab_strip;
      *tab_index = index;
      return true;
    }
  }

  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     WebContents** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);

  *contents = browser->tab_strip_model()->GetActiveWebContents();
  if (*contents) {
    if (tab_id)
      *tab_id = GetTabId(*contents);
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  if (tab_id == api::tabs::TAB_ID_NONE)
    return false;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile()
          ? profile->GetOffTheRecordProfile()
          : nullptr;
  for (auto* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        WebContents* target_contents = target_tab_strip->GetWebContentsAt(i);
        if (SessionTabHelper::IdForTab(target_contents) == tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

bool ExtensionTabUtil::IsKillURL(const GURL& url) {
  static const char* const kill_hosts[] = {
      chrome::kChromeUICrashHost,         chrome::kChromeUIDelayedHangUIHost,
      chrome::kChromeUIHangUIHost,        chrome::kChromeUIKillHost,
      chrome::kChromeUIQuitHost,          chrome::kChromeUIRestartHost,
      content::kChromeUIBrowserCrashHost, content::kChromeUIMemoryExhaustHost,
  };

  // Check a fixed-up URL, to normalize the scheme and parse hosts correctly.
  GURL fixed_url =
      url_formatter::FixupURL(url.possibly_invalid_spec(), std::string());
  if (!fixed_url.SchemeIs(content::kChromeUIScheme))
    return false;

  base::StringPiece fixed_host = fixed_url.host_piece();
  for (size_t i = 0; i < arraysize(kill_hosts); ++i) {
    if (fixed_host == kill_hosts[i])
      return true;
  }

  return false;
}

void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser) {
    Browser::CreateParams params = Browser::CreateParams(profile, user_gesture);
    browser = new Browser(params);
  }
  chrome::NavigateParams params(browser, web_contents);

  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is hosted
  // in a tab?
  if (disposition == WindowOpenDisposition::NEW_POPUP)
    params.extension_app_id = extension_id;

  params.disposition = disposition;
  params.window_bounds = initial_rect;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  chrome::Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

// static
void ExtensionTabUtil::ForEachTab(
    const base::Callback<void(WebContents*)>& callback) {
  for (TabContentsIterator iterator; !iterator.done(); iterator.Next())
    callback.Run(*iterator);
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser != nullptr)
    return browser->extension_window_controller();

  return nullptr;
}

bool ExtensionTabUtil::OpenOptionsPageFromAPI(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  if (!OptionsPageInfo::HasOptionsPage(extension))
    return false;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // This version of OpenOptionsPage() is only called when the extension
  // initiated the command via chrome.runtime.openOptionsPage. For a spanning
  // mode extension, this API could only be called from a regular profile, since
  // that's the only place it's running.
  DCHECK(!profile->IsOffTheRecord() || IncognitoInfo::IsSplitMode(extension));
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile, true));
  return extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser);
}

bool ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
  if (!OptionsPageInfo::HasOptionsPage(extension))
    return false;

  // Force the options page to open in non-OTR window if the extension is not
  // running in split mode, because it won't be able to save settings from OTR.
  // This version of OpenOptionsPage() can be called from an OTR window via e.g.
  // the action menu, since that's not initiated by the extension.
  std::unique_ptr<chrome::ScopedTabbedBrowserDisplayer> displayer;
  if (browser->profile()->IsOffTheRecord() &&
      !IncognitoInfo::IsSplitMode(extension)) {
    displayer.reset(new chrome::ScopedTabbedBrowserDisplayer(
        browser->profile()->GetOriginalProfile()));
    browser = displayer->browser();
  }

  GURL url_to_navigate;
  bool open_in_tab = OptionsPageInfo::ShouldOpenInTab(extension);
  if (open_in_tab) {
    // Options page tab is simply e.g. chrome-extension://.../options.html.
    url_to_navigate = OptionsPageInfo::GetOptionsPage(extension);
  } else {
    // Options page tab is Extension settings pointed at that Extension's ID,
    // e.g. chrome://extensions?options=...
    url_to_navigate = GURL(chrome::kChromeUIExtensionsURL);
    GURL::Replacements replacements;
    std::string query =
        base::StringPrintf("options=%s", extension->id().c_str());
    replacements.SetQueryStr(query);
    url_to_navigate = url_to_navigate.ReplaceComponents(replacements);
  }

  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser, url_to_navigate));
  // We need to respect path differences because we don't want opening the
  // options page to close a page that might be open to extension content.
  // However, if the options page opens inside the chrome://extensions page, we
  // can override an existing page.
  // Note: default ref behavior is IGNORE_REF, which is correct.
  params.path_behavior = open_in_tab
                             ? chrome::NavigateParams::RESPECT
                             : chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  params.url = url_to_navigate;
  chrome::ShowSingletonTabOverwritingNTP(browser, params);
  return true;
}

// static
bool ExtensionTabUtil::BrowserSupportsTabs(Browser* browser) {
  return browser && browser->tab_strip_model() && !browser->is_devtools();
}

}  // namespace extensions
