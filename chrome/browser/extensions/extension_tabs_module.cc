// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace keys = extension_tabs_module_constants;
namespace errors = extension_manifest_errors;

const int CaptureVisibleTabFunction::kDefaultQuality = 90;

namespace {

// |error_message| can optionally be passed in a will be set with an appropriate
// message if the window cannot be found by id.
Browser* GetBrowserInProfileWithId(Profile* profile,
                                   const int window_id,
                                   bool include_incognito,
                                   std::string* error_message) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator browser = BrowserList::begin();
       browser != BrowserList::end(); ++browser) {
    if (((*browser)->profile() == profile ||
         (*browser)->profile() == incognito_profile) &&
        ExtensionTabUtil::GetWindowId(*browser) == window_id)
      return *browser;
  }

  if (error_message)
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));

  return NULL;
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
bool GetTabById(int tab_id,
                Profile* profile,
                bool include_incognito,
                Browser** browser,
                TabStripModel** tab_strip,
                TabContentsWrapper** contents,
                int* tab_index,
                std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                   browser, tab_strip, contents, tab_index))
    return true;

  if (error_message)
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::IntToString(tab_id));

  return false;
}

// Takes |url_string| and returns a GURL which is either valid and absolute
// or invalid. If |url_string| is not directly interpretable as a valid (it is
// likely a relative URL) an attempt is made to resolve it. |extension| is
// provided so it can be resolved relative to its extension base
// (chrome-extension://<id>/). Using the source frame url would be more correct,
// but because the api shipped with urls resolved relative to their extension
// base, we decided it wasn't worth breaking existing extensions to fix.
GURL ResolvePossiblyRelativeURL(const std::string& url_string,
                                const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

bool IsCrashURL(const GURL& url) {
  // Check a fixed-up URL, to normalize the scheme and parse hosts correctly.
  GURL fixed_url =
      URLFixerUpper::FixupURL(url.possibly_invalid_spec(), std::string());
  return (fixed_url.SchemeIs(chrome::kChromeUIScheme) &&
          (fixed_url.host() == chrome::kChromeUIBrowserCrashHost ||
           fixed_url.host() == chrome::kChromeUICrashHost));
}

// Reads the |value| as either a single integer value or a list of integers.
bool ReadOneOrMoreIntegers(
    Value* value, std::vector<int>* result) {
  if (value->IsType(Value::TYPE_INTEGER)) {
    int tab_id;
    if (!value->GetAsInteger(&tab_id))
      return false;
    result->push_back(tab_id);
    return true;

  } else if (value->IsType(Value::TYPE_LIST)) {
    ListValue* tabs = static_cast<ListValue*>(value);
    for (size_t i = 0; i < tabs->GetSize(); ++i) {
      int tab_id;
      if (!tabs->GetInteger(i, &tab_id))
        return false;
      result->push_back(tab_id);
    }
    return true;
  }
  return false;
}

// A three state enum to distinguish between when a boolean query argument is
// set or not.
enum QueryArg {
  NOT_SET = -1,
  MATCH_FALSE,
  MATCH_TRUE
};

bool MatchesQueryArg(QueryArg arg, bool value) {
  if (arg == NOT_SET)
    return true;

  return (arg == MATCH_TRUE && value) || (arg == MATCH_FALSE && !value);
}

QueryArg ParseBoolQueryArg(base::DictionaryValue* query, const char* key) {
  if (query->HasKey(key)) {
    bool value = false;
    CHECK(query->GetBoolean(key, &value));
    return value ? MATCH_TRUE : MATCH_FALSE;
  }
  return NOT_SET;
}

}  // namespace

// Windows ---------------------------------------------------------------------

bool GetWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser || !browser->window()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));
    return false;
  }

  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetCurrentWindowFunction::RunImpl() {
  Browser* browser = GetCurrentBrowser();
  if (!browser || !browser->window()) {
    error_ = keys::kNoCurrentWindowError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetLastFocusedWindowFunction::RunImpl() {
  Browser* browser = BrowserList::FindAnyBrowser(
      profile(), include_incognito());
  if (!browser || !browser->window()) {
    error_ = keys::kNoLastFocusedWindowError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetAllWindowsFunction::RunImpl() {
  bool populate_tabs = false;
  if (HasOptionalArgument(0)) {
    DictionaryValue* args;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

    if (args->HasKey(keys::kPopulateKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kPopulateKey,
          &populate_tabs));
    }
  }

  result_.reset(new ListValue());
  Profile* incognito_profile =
      include_incognito() && profile()->HasOffTheRecordProfile() ?
          profile()->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator browser = BrowserList::begin();
    browser != BrowserList::end(); ++browser) {
      // Only examine browsers in the current profile that have windows.
      if (((*browser)->profile() == profile() ||
           (*browser)->profile() == incognito_profile) &&
          (*browser)->window()) {
        static_cast<ListValue*>(result_.get())->
          Append(ExtensionTabUtil::CreateWindowValue(*browser, populate_tabs));
      }
  }

  return true;
}

bool CreateWindowFunction::ShouldOpenIncognitoWindow(
    const base::DictionaryValue* args,
    std::vector<GURL>* urls,
    bool* is_error) {
  *is_error = false;
  const IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  bool incognito = false;
  if (args && args->HasKey(keys::kIncognitoKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kIncognitoKey,
                                                 &incognito));
    if (incognito && incognito_availability == IncognitoModePrefs::DISABLED) {
      error_ = keys::kIncognitoModeIsDisabled;
      *is_error = true;
      return false;
    }
    if (!incognito && incognito_availability == IncognitoModePrefs::FORCED) {
      error_ = keys::kIncognitoModeIsForced;
      *is_error = true;
      return false;
    }
  } else if (incognito_availability == IncognitoModePrefs::FORCED) {
    // If incognito argument is not specified explicitly, we default to
    // incognito when forced so by policy.
    incognito = true;
  }

  // If we are opening an incognito window.
  if (incognito) {
    std::string first_url_erased;
    // Guest session is an exception as it always opens in incognito mode.
    for (size_t i = 0; i < urls->size();) {
      if (browser::IsURLAllowedInIncognito((*urls)[i]) &&
          !Profile::IsGuestSession()) {
        if (first_url_erased.empty())
          first_url_erased = (*urls)[i].spec();
        urls->erase(urls->begin() + i);
      } else {
        i++;
      }
    }
    if (urls->empty() && !first_url_erased.empty()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          keys::kURLsNotAllowedInIncognitoError, first_url_erased);
      *is_error = true;
      return false;
    }
  }
  return incognito;
}

bool CreateWindowFunction::RunImpl() {
  DictionaryValue* args = NULL;
  std::vector<GURL> urls;
  TabContentsWrapper* contents = NULL;

  if (HasOptionalArgument(0))
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  // Look for optional url.
  if (args) {
    if (args->HasKey(keys::kUrlKey)) {
      Value* url_value;
      std::vector<std::string> url_strings;
      args->Get(keys::kUrlKey, &url_value);

      // First, get all the URLs the client wants to open.
      if (url_value->IsType(Value::TYPE_STRING)) {
        std::string url_string;
        url_value->GetAsString(&url_string);
        url_strings.push_back(url_string);
      } else if (url_value->IsType(Value::TYPE_LIST)) {
        const ListValue* url_list = static_cast<const ListValue*>(url_value);
        for (size_t i = 0; i < url_list->GetSize(); ++i) {
          std::string url_string;
          EXTENSION_FUNCTION_VALIDATE(url_list->GetString(i, &url_string));
          url_strings.push_back(url_string);
        }
      }

      // Second, resolve, validate and convert them to GURLs.
      for (std::vector<std::string>::iterator i = url_strings.begin();
           i != url_strings.end(); ++i) {
        GURL url = ResolvePossiblyRelativeURL(*i, GetExtension());
        if (!url.is_valid()) {
          error_ = ExtensionErrorUtils::FormatErrorMessage(
              keys::kInvalidUrlError, *i);
          return false;
        }
        // Don't let the extension crash the browser or renderers.
        if (IsCrashURL(url)) {
          error_ = keys::kNoCrashBrowserError;
          return false;
        }
        urls.push_back(url);
      }
    }
  }

  // Look for optional tab id.
  if (args) {
    int tab_id;
    if (args->HasKey(keys::kTabIdKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kTabIdKey, &tab_id));

      // Find the tab and detach it from the original window.
      Browser* source_browser = NULL;
      TabStripModel* source_tab_strip = NULL;
      int tab_index = -1;
      if (!GetTabById(tab_id, profile(), include_incognito(),
                      &source_browser, &source_tab_strip, &contents,
                      &tab_index, &error_))
        return false;
      contents = source_tab_strip->DetachTabContentsAt(tab_index);
      if (!contents) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, base::IntToString(tab_id));
        return false;
      }
    }
  }

  // Try to position the new browser relative its originating browser window.
  gfx::Rect  window_bounds;
  // The call offsets the bounds by kWindowTilePixels (defined in WindowSizer to
  // be 10)
  //
  // NOTE(rafaelw): It's ok if GetCurrentBrowser() returns NULL here.
  // GetBrowserWindowBounds will default to saved "default" values for the app.
  WindowSizer::GetBrowserWindowBounds(std::string(), gfx::Rect(),
                                      GetCurrentBrowser(), &window_bounds);

  // Calculate popup and panels bounds separately.
  gfx::Rect popup_bounds;
  gfx::Rect panel_bounds;  // Use 0x0 for panels. Panel manager sizes them.

  // In ChromiumOS the default popup bounds is 0x0 which indicates default
  // window sizes in PanelBrowserView. In other OSs use the same default
  // bounds as windows.
#if defined(OS_CHROMEOS)
  popup_bounds = panel_bounds;
#else
  popup_bounds = window_bounds;  // Use window size as default for popups
#endif

  Profile* window_profile = profile();
  Browser::Type window_type = Browser::TYPE_TABBED;
  bool focused = true;
  bool saw_focus_key = false;
  std::string extension_id;

  // Decide whether we are opening a normal window or an incognito window.
  bool is_error;
  bool open_incognito_window = ShouldOpenIncognitoWindow(args, &urls,
                                                         &is_error);
  if (is_error) {
    // error_ member variable is set inside of ShouldOpenIncognitoWindow.
    return false;
  }
  if (open_incognito_window) {
    window_profile = window_profile->GetOffTheRecordProfile();
  }

  if (args) {
    // Any part of the bounds can optionally be set by the caller.
    int bounds_val;
    if (args->HasKey(keys::kLeftKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kLeftKey,
                                                   &bounds_val));
      window_bounds.set_x(bounds_val);
      popup_bounds.set_x(bounds_val);
      panel_bounds.set_x(bounds_val);
    }

    if (args->HasKey(keys::kTopKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kTopKey,
                                                   &bounds_val));
      window_bounds.set_y(bounds_val);
      popup_bounds.set_y(bounds_val);
      panel_bounds.set_y(bounds_val);
    }

    if (args->HasKey(keys::kWidthKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kWidthKey,
                                                   &bounds_val));
      window_bounds.set_width(bounds_val);
      popup_bounds.set_width(bounds_val);
      panel_bounds.set_width(bounds_val);
    }

    if (args->HasKey(keys::kHeightKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kHeightKey,
                                                   &bounds_val));
      window_bounds.set_height(bounds_val);
      popup_bounds.set_height(bounds_val);
      panel_bounds.set_height(bounds_val);
    }

    if (args->HasKey(keys::kFocusedKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kFocusedKey,
                                                   &focused));
      saw_focus_key = true;
    }

    std::string type_str;
    if (args->HasKey(keys::kWindowTypeKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kWindowTypeKey,
                                                  &type_str));
      if (type_str == keys::kWindowTypeValuePopup) {
        window_type = Browser::TYPE_POPUP;
        extension_id = GetExtension()->id();
      } else if (type_str == keys::kWindowTypeValuePanel) {
        if (CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kDisablePanels)) {
          window_type = Browser::TYPE_POPUP;
        } else {
          window_type = Browser::TYPE_PANEL;
        }
        extension_id = GetExtension()->id();
      } else if (type_str != keys::kWindowTypeValueNormal) {
        error_ = keys::kInvalidWindowTypeError;
        return false;
      }
    }
  }

  // Unlike other window types, Panels do not take focus by default.
  if (!saw_focus_key && window_type == Browser::TYPE_PANEL)
    focused = false;

  Browser* new_window;
  if (extension_id.empty()) {
    new_window = Browser::CreateForType(window_type, window_profile);
    new_window->window()->SetBounds(window_bounds);
  } else {
    new_window = Browser::CreateForApp(
        window_type,
        web_app::GenerateApplicationNameFromExtensionId(extension_id),
        (window_type == Browser::TYPE_PANEL ? panel_bounds : popup_bounds),
        window_profile);
  }
  for (std::vector<GURL>::iterator i = urls.begin(); i != urls.end(); ++i)
    new_window->AddSelectedTabWithURL(*i, content::PAGE_TRANSITION_LINK);
  if (contents) {
    TabStripModel* target_tab_strip = new_window->tabstrip_model();
    target_tab_strip->InsertTabContentsAt(urls.size(), contents,
                                          TabStripModel::ADD_NONE);
  } else if (urls.empty()) {
    new_window->NewTab();
  }
  new_window->SelectNumberedTab(0);

  if (focused)
    new_window->window()->Show();
  else
    new_window->window()->ShowInactive();

  if (new_window->profile()->IsOffTheRecord() && !include_incognito()) {
    // Don't expose incognito windows if the extension isn't allowed.
    result_.reset(Value::CreateNullValue());
  } else {
    result_.reset(ExtensionTabUtil::CreateWindowValue(new_window, true));
  }

  return true;
}

bool UpdateWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser || !browser->window()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));
    return false;
  }

  ui::WindowShowState show_state = ui::SHOW_STATE_DEFAULT;  // No change.
  std::string state_str;
  if (update_props->HasKey(keys::kShowStateKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetString(keys::kShowStateKey,
                                                        &state_str));
    if (state_str == keys::kShowStateValueNormal) {
      show_state = ui::SHOW_STATE_NORMAL;
    } else if (state_str == keys::kShowStateValueMinimized) {
      show_state = ui::SHOW_STATE_MINIMIZED;
    } else if (state_str == keys::kShowStateValueMaximized) {
      show_state = ui::SHOW_STATE_MAXIMIZED;
    } else {
      error_ = keys::kInvalidWindowStateError;
      return false;
    }
  }

  switch (show_state) {
    case ui::SHOW_STATE_MINIMIZED:
      browser->window()->Minimize();
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      browser->window()->Maximize();
      break;
    case ui::SHOW_STATE_NORMAL:
      browser->window()->Restore();
      break;
    default:
      break;
  }

  gfx::Rect bounds = browser->window()->GetRestoredBounds();
  bool set_bounds = false;

  // Any part of the bounds can optionally be set by the caller.
  int bounds_val;
  if (update_props->HasKey(keys::kLeftKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kLeftKey,
        &bounds_val));
    bounds.set_x(bounds_val);
    set_bounds = true;
  }

  if (update_props->HasKey(keys::kTopKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kTopKey,
        &bounds_val));
    bounds.set_y(bounds_val);
    set_bounds = true;
  }

  if (update_props->HasKey(keys::kWidthKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kWidthKey,
        &bounds_val));
    bounds.set_width(bounds_val);
    set_bounds = true;
  }

  if (update_props->HasKey(keys::kHeightKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kHeightKey,
        &bounds_val));
    bounds.set_height(bounds_val);
    set_bounds = true;
  }

  if (set_bounds) {
    if (show_state == ui::SHOW_STATE_MINIMIZED ||
        show_state == ui::SHOW_STATE_MAXIMIZED) {
      error_ = keys::kInvalidWindowStateError;
      return false;
    }
    browser->window()->SetBounds(bounds);
  }

  bool active_val = false;
  if (update_props->HasKey(keys::kFocusedKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kFocusedKey, &active_val));
    if (active_val) {
      if (show_state == ui::SHOW_STATE_MINIMIZED) {
        error_ = keys::kInvalidWindowStateError;
        return false;
      }
      browser->window()->Activate();
    } else {
      if (show_state == ui::SHOW_STATE_MAXIMIZED) {
        error_ = keys::kInvalidWindowStateError;
        return false;
      }
      browser->window()->Deactivate();
    }
  }

  bool draw_attention = false;
  if (update_props->HasKey(keys::kDrawAttentionKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kDrawAttentionKey, &draw_attention));
    if (draw_attention)
        browser->window()->FlashFrame();
  }

  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));

  return true;
}

bool RemoveWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser)
    return false;

  // Don't let the extension remove the window if the user is dragging tabs
  // in that window.
  if (!browser->IsTabStripEditable()) {
    error_ = keys::kTabStripNotEditableError;
    return false;
  }

  browser->CloseWindow();

  return true;
}

// Tabs ------------------------------------------------------------------------

bool GetSelectedTabFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;

  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();
  TabContentsWrapper* contents = tab_strip->GetActiveTabContents();
  if (!contents) {
    error_ = keys::kNoSelectedTabError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateTabValue(contents->tab_contents(),
      tab_strip,
      tab_strip->active_index()));
  return true;
}

bool GetAllTabsInWindowFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  result_.reset(ExtensionTabUtil::CreateTabList(browser));

  return true;
}

bool QueryTabsFunction::RunImpl() {
  DictionaryValue* query = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &query));

  QueryArg active = ParseBoolQueryArg(query, keys::kActiveKey);
  QueryArg pinned = ParseBoolQueryArg(query, keys::kPinnedKey);
  QueryArg selected = ParseBoolQueryArg(query, keys::kHighlightedKey);

  QueryArg loading = NOT_SET;
  if (query->HasKey(keys::kStatusKey)) {
    std::string status;
    EXTENSION_FUNCTION_VALIDATE(query->GetString(keys::kStatusKey, &status));
    loading = (status == keys::kStatusValueLoading) ? MATCH_TRUE : MATCH_FALSE;
  }

  URLPattern url_pattern(URLPattern::SCHEME_ALL, "<all_urls>");
  if (query->HasKey(keys::kUrlKey)) {
    std::string value;
    EXTENSION_FUNCTION_VALIDATE(query->GetString(keys::kUrlKey, &value));
    url_pattern = URLPattern(URLPattern::SCHEME_ALL, value);
  }

  std::string title;
  if (query->HasKey(keys::kTitleKey))
    EXTENSION_FUNCTION_VALIDATE(
        query->GetString(keys::kTitleKey, &title));

  int window_id = -1;
  if (query->HasKey(keys::kWindowIdKey))
    EXTENSION_FUNCTION_VALIDATE(
        query->GetInteger(keys::kWindowIdKey, &window_id));

  std::string window_type;
  if (query->HasKey(keys::kWindowTypeLongKey))
    EXTENSION_FUNCTION_VALIDATE(
        query->GetString(keys::kWindowTypeLongKey, &window_type));

  ListValue* result = new ListValue();
  for (BrowserList::const_iterator browser = BrowserList::begin();
       browser != BrowserList::end(); ++browser) {
    if (!profile()->IsSameProfile((*browser)->profile()))
      continue;

    if (!(*browser)->window())
      continue;

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(*browser))
      continue;

    if (!window_type.empty() &&
        window_type != ExtensionTabUtil::GetWindowTypeText(*browser))
      continue;

    TabStripModel* tab_strip = (*browser)->tabstrip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      const TabContents* tab_contents =
          tab_strip->GetTabContentsAt(i)->tab_contents();

      if (!MatchesQueryArg(selected, tab_strip->IsTabSelected(i)))
        continue;

      if (!MatchesQueryArg(active, i == tab_strip->active_index()))
        continue;

      if (!MatchesQueryArg(pinned, tab_strip->IsTabPinned(i)))
        continue;

      if (!title.empty() && !MatchPattern(tab_contents->GetTitle(),
                                          UTF8ToUTF16(title)))
        continue;

      if (!url_pattern.MatchesURL(tab_contents->GetURL()))
        continue;

      if (!MatchesQueryArg(loading, tab_contents->IsLoading()))
        continue;

      result->Append(ExtensionTabUtil::CreateTabValue(
          tab_contents, tab_strip, i));
    }
  }

  result_.reset(result);
  return true;
}

bool CreateTabFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  Browser *browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (args->HasKey(keys::kWindowIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(
        keys::kWindowIdKey, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  std::string url_string;
  GURL url;
  if (args->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                &url_string));
    url = ResolvePossiblyRelativeURL(url_string, GetExtension());
    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       url_string);
      return false;
    }
  }

  // Don't let extensions crash the browser or renderers.
  if (IsCrashURL(url)) {
    error_ = keys::kNoCrashBrowserError;
    return false;
  }

  // Default to foreground for the new tab. The presence of 'selected' property
  // will override this default. This property is deprecated ('active' should
  // be used instead).
  bool active = true;
  if (args->HasKey(keys::kSelectedKey))
    EXTENSION_FUNCTION_VALIDATE(
        args->GetBoolean(keys::kSelectedKey, &active));

  // The 'active' property has replaced the 'selected' property.
  if (args->HasKey(keys::kActiveKey))
    EXTENSION_FUNCTION_VALIDATE(
        args->GetBoolean(keys::kActiveKey, &active));

  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  if (args->HasKey(keys::kPinnedKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kPinnedKey, &pinned));

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window.
  if (url.SchemeIs(chrome::kExtensionScheme) &&
      !GetExtension()->incognito_split_mode() &&
      browser->profile()->IsOffTheRecord()) {
    Profile* profile = browser->profile()->GetOriginalProfile();
    browser = BrowserList::FindTabbedBrowser(profile, false);
    if (!browser) {
      browser = Browser::Create(profile);
      browser->window()->Show();
    }
  }

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  if (args->HasKey(keys::kIndexKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kIndexKey, &index));

  TabStripModel* tab_strip = browser->tabstrip_model();

  index = std::min(std::max(index, -1), tab_strip->count());

  int add_types = active ? TabStripModel::ADD_ACTIVE :
                             TabStripModel::ADD_NONE;
  add_types |= TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  browser::NavigateParams params(browser, url, content::PAGE_TRANSITION_LINK);
  params.disposition = active ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  params.tabstrip_index = index;
  params.tabstrip_add_types = add_types;
  browser::Navigate(&params);

  if (active)
    params.target_contents->view()->SetInitialFocus();

  // Return data about the newly created tab.
  if (has_callback()) {
    result_.reset(ExtensionTabUtil::CreateTabValue(
        params.target_contents->tab_contents(),
        params.browser->tabstrip_model(),
        params.browser->tabstrip_model()->GetIndexOfTabContents(
            params.target_contents)));
  }

  return true;
}

bool GetTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabStripModel* tab_strip = NULL;
  TabContentsWrapper* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_))
    return false;

  result_.reset(ExtensionTabUtil::CreateTabValue(contents->tab_contents(),
      tab_strip,
      tab_index));
  return true;
}

bool GetCurrentTabFunction::RunImpl() {
  DCHECK(dispatcher());

  TabContents* contents = dispatcher()->delegate()->GetAssociatedTabContents();
  if (contents)
    result_.reset(ExtensionTabUtil::CreateTabValue(contents));

  return true;
}

bool HighlightTabsFunction::RunImpl() {
  DictionaryValue* info = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &info));

  // Get the window id from the params.
  int window_id = -1;
  EXTENSION_FUNCTION_VALIDATE(
      info->GetInteger(keys::kWindowIdKey, &window_id));

  Browser* browser = GetBrowserInProfileWithId(
      profile(), window_id, include_incognito(), &error_);

  if (!browser || !browser->window()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));
    return false;
  }

  TabStripModel* tabstrip = browser->tabstrip_model();
  TabStripSelectionModel selection;
  int active_index = -1;

  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(info->Get(keys::kTabsKey, &tab_value));

  std::vector<int> tab_indices;
  EXTENSION_FUNCTION_VALIDATE(ReadOneOrMoreIntegers(tab_value, &tab_indices));

  // Create a new selection model as we read the list of tab indices.
  for (size_t i = 0; i < tab_indices.size(); ++i) {
    int index = tab_indices[i];

    // Make sure the index is in range.
    if (!tabstrip->ContainsIndex(index)) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          keys::kTabIndexNotFoundError, base::IntToString(index));
      return false;
    }

    // By default, we make the first tab in the list active.
    if (active_index == -1)
      active_index = index;

    selection.AddIndexToSelection(index);
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty()) {
    error_ = keys::kNoHighlightedTabError;
    return false;
  }

  selection.set_active(active_index);
  browser->tabstrip_model()->SetSelectionFromModel(selection);
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, true));
  return true;
}

UpdateTabFunction::UpdateTabFunction() {
}

bool UpdateTabFunction::RunImpl() {
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  Value* tab_value = NULL;
  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));
  }

  int tab_id = -1;
  TabContentsWrapper* contents = NULL;
  if (tab_value == NULL || tab_value->IsType(Value::TYPE_NULL)) {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }
    contents = browser->tabstrip_model()->GetActiveTabContents();
    if (!contents) {
      error_ = keys::kNoSelectedTabError;
      return false;
    }
    tab_id = ExtensionTabUtil::GetTabId(contents->tab_contents());
  } else {
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&tab_id));
  }

  int tab_index = -1;
  TabStripModel* tab_strip = NULL;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_)) {
    return false;
  }
  NavigationController& controller = contents->controller();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  std::string url_string;
  if (update_props->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetString(
        keys::kUrlKey, &url_string));
    GURL url = ResolvePossiblyRelativeURL(url_string, GetExtension());

    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       url_string);
      return false;
    }

    // Don't let the extension crash the browser or renderers.
    if (IsCrashURL(url)) {
      error_ = keys::kNoCrashBrowserError;
      return false;
    }

    // JavaScript URLs can do the same kinds of things as cross-origin XHR, so
    // we need to check host permissions before allowing them.
    if (url.SchemeIs(chrome::kJavaScriptScheme)) {
      if (!GetExtension()->CanExecuteScriptOnPage(
              contents->tab_contents()->GetURL(), NULL, &error_)) {
        return false;
      }

      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = request_id();
      params.extension_id = extension_id();
      params.is_javascript = true;
      params.code = url.path();
      params.all_frames = false;
      params.in_main_world = true;

      RenderViewHost* render_view_host =
          contents->tab_contents()->render_view_host();
      render_view_host->Send(
          new ExtensionMsg_ExecuteCode(render_view_host->routing_id(),
                                       params));

      Observe(contents->tab_contents());
      AddRef();  // balanced in Observe()

      return true;
    }

    controller.LoadURL(
        url, GURL(), content::PAGE_TRANSITION_LINK, std::string());

    // The URL of a tab contents never actually changes to a JavaScript URL, so
    // this check only makes sense in other cases.
    if (!url.SchemeIs(chrome::kJavaScriptScheme))
      DCHECK_EQ(url.spec(), contents->tab_contents()->GetURL().spec());
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (update_props->HasKey(keys::kSelectedKey))
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kSelectedKey, &active));

  // The 'active' property has replaced 'selected'.
  if (update_props->HasKey(keys::kActiveKey))
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kActiveKey, &active));

  if (active) {
    if (tab_strip->active_index() != tab_index) {
      tab_strip->ActivateTabAt(tab_index, false);
      DCHECK_EQ(contents, tab_strip->GetActiveTabContents());
    }
    contents->tab_contents()->Focus();
  }

  bool highlighted = false;
  if (update_props->HasKey(keys::kHighlightedKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kHighlightedKey, &highlighted));
    if (highlighted != tab_strip->IsTabSelected(tab_index))
      tab_strip->ToggleSelectionAt(tab_index);
  }

  bool pinned = false;
  if (update_props->HasKey(keys::kPinnedKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(keys::kPinnedKey,
                                                         &pinned));
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfTabContents(contents);
  }

  if (has_callback()) {
    if (GetExtension()->HasAPIPermission(ExtensionAPIPermission::kTab)) {
      result_.reset(ExtensionTabUtil::CreateTabValue(contents->tab_contents(),
                                                     tab_strip,
                                                     tab_index));
    } else {
      result_.reset(Value::CreateNullValue());
    }
  }

  SendResponse(true);
  return true;
}

bool UpdateTabFunction::OnMessageReceived(const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
    return false;

  int message_request_id;
  void* iter = NULL;
  if (!message.ReadInt(&iter, &message_request_id)) {
    NOTREACHED() << "malformed extension message";
    return true;
  }

  if (message_request_id != request_id())
    return false;

  IPC_BEGIN_MESSAGE_MAP(UpdateTabFunction, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                        OnExecuteCodeFinished)
  IPC_END_MESSAGE_MAP()
  return true;
}

void UpdateTabFunction::OnExecuteCodeFinished(int request_id,
                                              bool success,
                                              const std::string& error) {
  if (!error.empty()) {
    CHECK(!success);
    error_ = error;
  }

  SendResponse(success);

  Observe(NULL);
  Release();  // balanced in Execute()
}

bool MoveTabsFunction::RunImpl() {
  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));

  std::vector<int> tab_ids;
  EXTENSION_FUNCTION_VALIDATE(ReadOneOrMoreIntegers(tab_value, &tab_ids));

  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  int new_index;
  EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
      keys::kIndexKey, &new_index));
  EXTENSION_FUNCTION_VALIDATE(new_index >= 0);

  ListValue tab_values;
  for (size_t i = 0; i < tab_ids.size(); ++i) {
    Browser* source_browser = NULL;
    TabStripModel* source_tab_strip = NULL;
    TabContentsWrapper* contents = NULL;
    int tab_index = -1;
    if (!GetTabById(tab_ids[i], profile(), include_incognito(),
                    &source_browser, &source_tab_strip, &contents,
                    &tab_index, &error_))
      return false;

    // Don't let the extension move the tab if the user is dragging tabs.
    if (!source_browser->IsTabStripEditable()) {
      error_ = keys::kTabStripNotEditableError;
      return false;
    }

    // Insert the tabs one after another.
    new_index += i;

    if (update_props->HasKey(keys::kWindowIdKey)) {
      Browser* target_browser;
      int window_id;
      EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
          keys::kWindowIdKey, &window_id));
      target_browser = GetBrowserInProfileWithId(profile(), window_id,
                                                 include_incognito(), &error_);
      if (!target_browser)
        return false;

      if (!target_browser->IsTabStripEditable()) {
        error_ = keys::kTabStripNotEditableError;
        return false;
      }

      if (!target_browser->is_type_tabbed()) {
        error_ = keys::kCanOnlyMoveTabsWithinNormalWindowsError;
        return false;
      }

      if (target_browser->profile() != source_browser->profile()) {
        error_ = keys::kCanOnlyMoveTabsWithinSameProfileError;
        return false;
      }

      // If windowId is different from the current window, move between windows.
      if (ExtensionTabUtil::GetWindowId(target_browser) !=
          ExtensionTabUtil::GetWindowId(source_browser)) {
        TabStripModel* target_tab_strip = target_browser->tabstrip_model();
        contents = source_tab_strip->DetachTabContentsAt(tab_index);
        if (!contents) {
          error_ = ExtensionErrorUtils::FormatErrorMessage(
              keys::kTabNotFoundError, base::IntToString(tab_ids[i]));
          return false;
        }

        // Clamp move location to the last position.
        // This is ">" because it can append to a new index position.
        if (new_index > target_tab_strip->count())
          new_index = target_tab_strip->count();

        target_tab_strip->InsertTabContentsAt(
            new_index, contents, TabStripModel::ADD_NONE);

        if (has_callback())
          tab_values.Append(ExtensionTabUtil::CreateTabValue(
              contents->tab_contents(), target_tab_strip, new_index));

        continue;
      }
    }

    // Perform a simple within-window move.
    // Clamp move location to the last position.
    // This is ">=" because the move must be to an existing location.
    if (new_index >= source_tab_strip->count())
      new_index = source_tab_strip->count() - 1;

    if (new_index != tab_index)
      source_tab_strip->MoveTabContentsAt(tab_index, new_index, false);

    if (has_callback())
      tab_values.Append(ExtensionTabUtil::CreateTabValue(
          contents->tab_contents(), source_tab_strip, new_index));
  }

  if (!has_callback())
    return true;

  // Only return the results as an array if there are multiple tabs.
  if (tab_ids.size() > 1) {
    result_.reset(tab_values.DeepCopy());
  } else if (tab_ids.size() == 1) {
    Value* value = NULL;
    CHECK(tab_values.Get(0, &value));
    result_.reset(value->DeepCopy());
  }
  return true;
}

bool ReloadTabFunction::RunImpl() {
  bool bypass_cache = false;
  if (HasOptionalArgument(1)) {
      DictionaryValue* reload_props = NULL;
      EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &reload_props));

      if (reload_props->HasKey(keys::kBypassCache)) {
        EXTENSION_FUNCTION_VALIDATE(reload_props->GetBoolean(
            keys::kBypassCache,
            &bypass_cache));
      }
  }

  TabContentsWrapper* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  Value* tab_value = NULL;
  if (HasOptionalArgument(0))
    EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));

  if (tab_value == NULL || tab_value->IsType(Value::TYPE_NULL)) {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }

    if (!ExtensionTabUtil::GetDefaultTab(browser, &contents, NULL))
      return false;
  } else {
    int tab_id;
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&tab_id));

    Browser* browser = NULL;
    if (!GetTabById(tab_id, profile(), include_incognito(),
                    &browser, NULL, &contents, NULL, &error_))
    return false;
  }

  TabContents* tab_contents = contents->tab_contents();
  if (tab_contents->showing_interstitial_page()) {
    // This does as same as Browser::ReloadInternal.
    NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
    GetCurrentBrowser()->OpenURL(entry->url(), GURL(), CURRENT_TAB,
                                 content::PAGE_TRANSITION_RELOAD);
  } else if (bypass_cache) {
    tab_contents->controller().ReloadIgnoringCache(true);
  } else {
    tab_contents->controller().Reload(true);
  }

  return true;
}

bool RemoveTabsFunction::RunImpl() {
  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));

  std::vector<int> tab_ids;
  EXTENSION_FUNCTION_VALIDATE(ReadOneOrMoreIntegers(tab_value, &tab_ids));

  for (size_t i = 0; i < tab_ids.size(); ++i) {
    Browser* browser = NULL;
    TabContentsWrapper* contents = NULL;
    if (!GetTabById(tab_ids[i], profile(), include_incognito(),
                    &browser, NULL, &contents, NULL, &error_))
      return false;

    // Don't let the extension remove a tab if the user is dragging tabs around.
    if (!browser->IsTabStripEditable()) {
      error_ = keys::kTabStripNotEditableError;
      return false;
    }

    // Close the tab in this convoluted way, since there's a chance that the tab
    // is being dragged, or we're in some other nested event loop. This code
    // path should ensure that the tab is safely closed under such
    // circumstances, whereas |Browser::CloseTabContents()| does not.
    RenderViewHost* render_view_host = contents->render_view_host();
    render_view_host->delegate()->Close(render_view_host);
  }
  return true;
}

bool CaptureVisibleTabFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;

  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
  }

  if (!browser) {
    error_ = keys::kNoCurrentWindowError;
    return false;
  }

  image_format_ = FORMAT_JPEG;  // Default format is JPEG.
  image_quality_ = kDefaultQuality;  // Default quality setting.

  if (HasOptionalArgument(1)) {
    DictionaryValue* options;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));

    if (options->HasKey(keys::kFormatKey)) {
      std::string format;
      EXTENSION_FUNCTION_VALIDATE(
          options->GetString(keys::kFormatKey, &format));

      if (format == keys::kFormatValueJpeg) {
        image_format_ = FORMAT_JPEG;
      } else if (format == keys::kFormatValuePng) {
        image_format_ = FORMAT_PNG;
      } else {
        // Schema validation should make this unreachable.
        EXTENSION_FUNCTION_VALIDATE(0);
      }
    }

    if (options->HasKey(keys::kQualityKey)) {
      EXTENSION_FUNCTION_VALIDATE(
          options->GetInteger(keys::kQualityKey, &image_quality_));
    }
  }

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents) {
    error_ = keys::kInternalVisibleTabCaptureError;
    return false;
  }

  // captureVisibleTab() can return an image containing sensitive information
  // that the browser would otherwise protect.  Ensure the extension has
  // permission to do this.
  if (!GetExtension()->CanCaptureVisiblePage(tab_contents->GetURL(), &error_))
    return false;

  RenderViewHost* render_view_host = tab_contents->render_view_host();

  // If a backing store is cached for the tab we want to capture,
  // and it can be copied into a bitmap, then use it to generate the image.
  BackingStore* backing_store = render_view_host->GetBackingStore(false);
  if (backing_store && CaptureSnapshotFromBackingStore(backing_store))
    return true;

  // Ask the renderer for a snapshot of the tab.
  TabContentsWrapper* wrapper = browser->GetSelectedTabContentsWrapper();
  wrapper->CaptureSnapshot();
  registrar_.Add(this,
                 chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
                 content::Source<TabContentsWrapper>(wrapper));
  AddRef();  // Balanced in CaptureVisibleTabFunction::Observe().

  return true;
}

// Build the image of a tab's contents out of a backing store.
// This may fail if we can not copy a backing store into a bitmap.
// For example, some uncommon X11 visual modes are not supported by
// CopyFromBackingStore().
bool CaptureVisibleTabFunction::CaptureSnapshotFromBackingStore(
    BackingStore* backing_store) {

  skia::PlatformCanvas temp_canvas;
  if (!backing_store->CopyFromBackingStore(gfx::Rect(backing_store->size()),
                                           &temp_canvas)) {
    return false;
  }
  VLOG(1) << "captureVisibleTab() got image from backing store.";

  SendResultFromBitmap(
      skia::GetTopDevice(temp_canvas)->accessBitmap(false));
  return true;
}

// If a backing store was not available in CaptureVisibleTabFunction::RunImpl,
// than the renderer was asked for a snapshot.  Listen for a notification
// that the snapshot is available.
void CaptureVisibleTabFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN);

  const SkBitmap *screen_capture =
      content::Details<const SkBitmap>(details).ptr();
  const bool error = screen_capture->empty();

  if (error) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
  } else {
    VLOG(1) << "captureVisibleTab() got image from renderer.";
    SendResultFromBitmap(*screen_capture);
  }

  Release();  // Balanced in CaptureVisibleTabFunction::RunImpl().
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void CaptureVisibleTabFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  std::vector<unsigned char> data;
  SkAutoLockPixels screen_capture_lock(screen_capture);
  bool encoded = false;
  std::string mime_type;
  switch (image_format_) {
    case FORMAT_JPEG:
      encoded = gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(screen_capture.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          screen_capture.width(),
          screen_capture.height(),
          static_cast<int>(screen_capture.rowBytes()),
          image_quality_,
          &data);
      mime_type = keys::kMimeTypeJpeg;
      break;
    case FORMAT_PNG:
      encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
          screen_capture,
          true,  // Discard transparency.
          &data);
      mime_type = keys::kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  if (!encoded) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
    return;
  }

  std::string base64_result;
  base::StringPiece stream_as_string(
      reinterpret_cast<const char*>(vector_as_array(&data)), data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(0, base::StringPrintf("data:%s;base64,",
                                             mime_type.c_str()));
  result_.reset(new StringValue(base64_result));
  SendResponse(true);
}

bool DetectTabLanguageFunction::RunImpl() {
  int tab_id = 0;
  Browser* browser = NULL;
  TabContentsWrapper* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));
    if (!GetTabById(tab_id, profile(), include_incognito(),
                    &browser, NULL, &contents, NULL, &error_)) {
      return false;
    }
    if (!browser || !contents)
      return false;
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      return false;
    contents = browser->tabstrip_model()->GetActiveTabContents();
    if (!contents)
      return false;
  }

  if (contents->controller().needs_reload()) {
    // If the tab hasn't been loaded, don't wait for the tab to load.
    error_ = keys::kCannotDetermineLanguageOfUnloadedTab;
    return false;
  }

  AddRef();  // Balanced in GotLanguage()

  TranslateTabHelper* helper = contents->translate_tab_helper();
  if (!helper->language_state().original_language().empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &DetectTabLanguageFunction::GotLanguage, this,
        helper->language_state().original_language()));
    return true;
  }
  // The tab contents does not know its language yet.  Let's  wait until it
  // receives it, or until the tab is closed/navigates to some other page.
  registrar_.Add(this, chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                 content::Source<TabContents>(contents->tab_contents()));
  registrar_.Add(
      this, content::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&(contents->controller())));
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&(contents->controller())));
  return true;
}

void DetectTabLanguageFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  std::string language;
  if (type == chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED)
    language = *content::Details<std::string>(details).ptr();

  registrar_.RemoveAll();

  // Call GotLanguage in all cases as we want to guarantee the callback is
  // called for every API call the extension made.
  GotLanguage(language);
}

void DetectTabLanguageFunction::GotLanguage(const std::string& language) {
  result_.reset(Value::CreateStringValue(language.c_str()));
  SendResponse(true);

  Release();  // Balanced in Run()
}
