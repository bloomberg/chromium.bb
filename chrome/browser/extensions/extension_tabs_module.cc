// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/extensions/extension_window_list.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/snapshot_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ui/views/ash/panel_view_aura.h"
#endif

namespace Get = extensions::api::windows::Get;
namespace GetAll = extensions::api::windows::GetAll;
namespace GetCurrent = extensions::api::windows::GetCurrent;
namespace GetLastFocused = extensions::api::windows::GetLastFocused;
namespace errors = extension_manifest_errors;
namespace keys = extension_tabs_module_constants;

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::RenderViewHost;
using content::WebContents;

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
        ExtensionTabUtil::GetWindowId(*browser) == window_id &&
        ((*browser)->window()))
      return *browser;
  }

  if (error_message)
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));

  return NULL;
}

bool GetBrowserFromWindowID(
    UIThreadExtensionFunction* function, int window_id, Browser** browser) {
  if (window_id == extension_misc::kCurrentWindowId) {
    *browser = function->GetCurrentBrowser();
    if (!(*browser) || !(*browser)->window()) {
      function->SetError(keys::kNoCurrentWindowError);
      return false;
    }
  } else {
    std::string error;
    *browser = GetBrowserInProfileWithId(
        function->profile(), window_id, function->include_incognito(), &error);
    if (!*browser) {
      function->SetError(error);
      return false;
    }
  }
  return true;
}

ExtensionWindowController::ProfileMatchType ProfileMatchType(
    bool include_incognito) {
  return include_incognito ?
      ExtensionWindowController::MATCH_INCOGNITO
      : ExtensionWindowController::MATCH_NORMAL_ONLY;
}

bool GetWindowFromWindowID(UIThreadExtensionFunction* function,
                           int window_id,
                           ExtensionWindowController** controller) {
  if (window_id == extension_misc::kCurrentWindowId) {
    ExtensionWindowController* extension_window_controller =
        function->dispatcher()->delegate()->GetExtensionWindowController();
    // If there is a window controller associated with this extension, use that.
    if (extension_window_controller) {
      *controller = extension_window_controller;
    } else {
      // Otherwise get the focused or most recently added window.
      *controller = ExtensionWindowList::GetInstance()->CurrentWindow(
          function->profile(),
          ProfileMatchType(function->include_incognito()));
    }
    if (!(*controller)) {
      function->SetError(keys::kNoCurrentWindowError);
      return false;
    }
  } else {
    *controller = ExtensionWindowList::GetInstance()->FindWindowById(
        function->profile(),
        ProfileMatchType(function->include_incognito()),
        window_id);
    if (!(*controller)) {
      function->SetError(ExtensionErrorUtils::FormatErrorMessage(
          keys::kWindowNotFoundError, base::IntToString(window_id)));
      return false;
    }
  }
  return true;
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

// Reads the |value| as either a single integer value or a list of integers.
bool ReadOneOrMoreIntegers(
    Value* value, std::vector<int>* result) {
  if (value->IsType(Value::TYPE_INTEGER)) {
    int tab_id = -1;
    if (!value->GetAsInteger(&tab_id))
      return false;
    result->push_back(tab_id);
    return true;

  } else if (value->IsType(Value::TYPE_LIST)) {
    ListValue* tabs = static_cast<ListValue*>(value);
    for (size_t i = 0; i < tabs->GetSize(); ++i) {
      int tab_id = -1;
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
  scoped_ptr<Get::Params> params(Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  ExtensionWindowController* controller;
  if (!GetWindowFromWindowID(this, params->window_id, &controller))
    return false;

  if (populate_tabs)
    result_.reset(controller->CreateWindowValueWithTabs());
  else
    result_.reset(controller->CreateWindowValue());
  return true;
}

bool GetCurrentWindowFunction::RunImpl() {
  scoped_ptr<GetCurrent::Params> params(GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  ExtensionWindowController* controller;
  if (!GetWindowFromWindowID(this,
                             extension_misc::kCurrentWindowId,
                             &controller)) {
    return false;
  }
  if (populate_tabs)
    result_.reset(controller->CreateWindowValueWithTabs());
  else
    result_.reset(controller->CreateWindowValue());
  return true;
}

bool GetLastFocusedWindowFunction::RunImpl() {
  scoped_ptr<GetLastFocused::Params> params(
      GetLastFocused::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  // Note: currently this returns the last active browser. If we decide to
  // include other window types (e.g. panels), we will need to add logic to
  // ExtensionWindowList that mirrors the active behavior of BrowserList.
  Browser* browser = BrowserList::FindAnyBrowser(
      profile(), include_incognito());
  if (!browser || !browser->window()) {
    error_ = keys::kNoLastFocusedWindowError;
    return false;
  }
  ExtensionWindowController* controller =
      browser->extension_window_controller();
  if (populate_tabs)
    result_.reset(controller->CreateWindowValueWithTabs());
  else
    result_.reset(controller->CreateWindowValue());
  return true;
}

bool GetAllWindowsFunction::RunImpl() {
  scoped_ptr<GetAll::Params> params(GetAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  ListValue* window_list = new ListValue();
  const ExtensionWindowList::WindowList& windows =
      ExtensionWindowList::GetInstance()->windows();
  for (ExtensionWindowList::WindowList::const_iterator iter =
           windows.begin();
       iter != windows.end(); ++iter) {
    if (!(*iter)->MatchesProfile(
            profile(), ProfileMatchType(include_incognito())))
      continue;
    if (populate_tabs)
      window_list->Append((*iter)->CreateWindowValueWithTabs());
    else
      window_list->Append((*iter)->CreateWindowValue());
  }
  result_.reset(window_list);
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

  // Remove all URLs that are not allowed in an incognito session. Note that a
  // ChromeOS guest session is not considered incognito in this case.
  if (incognito && !Profile::IsGuestSession()) {
    std::string first_url_erased;
    for (size_t i = 0; i < urls->size();) {
      if (browser::IsURLAllowedInIncognito((*urls)[i])) {
        i++;
      } else {
        if (first_url_erased.empty())
          first_url_erased = (*urls)[i].spec();
        urls->erase(urls->begin() + i);
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
        GURL url = ExtensionTabUtil::ResolvePossiblyRelativeURL(
            *i, GetExtension());
        if (!url.is_valid()) {
          error_ = ExtensionErrorUtils::FormatErrorMessage(
              keys::kInvalidUrlError, *i);
          return false;
        }
        // Don't let the extension crash the browser or renderers.
        if (ExtensionTabUtil::IsCrashURL(url)) {
          error_ = keys::kNoCrashBrowserError;
          return false;
        }
        urls.push_back(url);
      }
    }
  }

  // Look for optional tab id.
  if (args) {
    int tab_id = -1;
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
  gfx::Rect window_bounds;
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
  bool is_error = true;
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
    int bounds_val = -1;
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
        extension_id = GetExtension()->id();
        bool use_panels = false;
#if !defined(OS_ANDROID)
        use_panels = PanelManager::ShouldUsePanels(extension_id);
#endif
#if defined(USE_ASH)
        if (CommandLine::ForCurrentProcess()->HasSwitch(
                ash::switches::kAuraPanelManager))
          use_panels = true;
#endif
        if (use_panels)
          window_type = Browser::TYPE_PANEL;
        else
          window_type = Browser::TYPE_POPUP;
      } else if (type_str == keys::kWindowTypeValueShell &&
          GetExtension()->is_platform_app()) {
        GURL window_url = urls.empty() ? GURL(chrome::kAboutBlankURL) : urls[0];
        ShellWindow* shell_window =
            ShellWindow::Create(window_profile, GetExtension(), window_url);
        // TODO(mihaip): It might be less janky to pass in the desired bounds
        // into ShellWindow::Create directly.
        shell_window->SetBounds(window_bounds);
        result_.reset(shell_window->extension_window_controller()->
            CreateWindowValueWithTabs());
        return true;
      } else if (type_str != keys::kWindowTypeValueNormal) {
        error_ = keys::kInvalidWindowTypeError;
        return false;
      }
    }
  }

#if defined(USE_ASH)
  // Aura Panels create a new PanelViewAura.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAuraPanelManager) &&
      window_type == Browser::TYPE_PANEL) {
    // Note: Panels ignore all but the first url provided.
    std::string title =
        web_app::GenerateApplicationNameFromExtensionId(extension_id);
    PanelViewAura* panel_view = new PanelViewAura(title);
    panel_view->Init(window_profile, urls[0], panel_bounds);
    result_.reset(
        panel_view->extension_window_controller()->CreateWindowValueWithTabs());
    return true;
  }
#endif

  // Create a new BrowserWindow.
  Browser::CreateParams create_params;
  if (extension_id.empty()) {
    create_params = Browser::CreateParams(window_type, window_profile);
    create_params.initial_bounds = window_bounds;
  } else {
    create_params = Browser::CreateParams::CreateForApp(
        window_type,
        web_app::GenerateApplicationNameFromExtensionId(extension_id),
        (window_type == Browser::TYPE_PANEL ? panel_bounds : popup_bounds),
        window_profile);
  }
  create_params.initial_show_state = ui::SHOW_STATE_NORMAL;
  Browser* new_window = Browser::CreateWithParams(create_params);

  for (std::vector<GURL>::iterator i = urls.begin(); i != urls.end(); ++i) {
    TabContentsWrapper* tab = new_window->AddSelectedTabWithURL(
        *i, content::PAGE_TRANSITION_LINK);
    if (window_type == Browser::TYPE_PANEL)
      tab->extension_tab_helper()->SetExtensionAppIconById(extension_id);
  }
  if (contents) {
    TabStripModel* target_tab_strip = new_window->tabstrip_model();
    target_tab_strip->InsertTabContentsAt(urls.size(), contents,
                                          TabStripModel::ADD_NONE);
  } else if (urls.empty()) {
    new_window->NewTab();
  }
  new_window->SelectNumberedTab(0);

  // Unlike other window types, Panels do not take focus by default.
  if (!saw_focus_key && window_type == Browser::TYPE_PANEL)
    focused = false;

  if (focused)
    new_window->window()->Show();
  else
    new_window->window()->ShowInactive();

  if (new_window->profile()->IsOffTheRecord() && !include_incognito()) {
    // Don't expose incognito windows if the extension isn't allowed.
    result_.reset(Value::CreateNullValue());
  } else {
    result_.reset(
        new_window->extension_window_controller()->CreateWindowValueWithTabs());
  }

  return true;
}

bool UpdateWindowFunction::RunImpl() {
  int window_id = extension_misc::kUnknownWindowId;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));

  ExtensionWindowController* controller;
  if (!GetWindowFromWindowID(this, window_id, &controller))
    return false;

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
    } else if (state_str == keys::kShowStateValueFullscreen) {
      show_state = ui::SHOW_STATE_FULLSCREEN;
    } else {
      error_ = keys::kInvalidWindowStateError;
      return false;
    }
  }

  if (show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_DEFAULT)
    controller->SetFullscreenMode(false, GetExtension()->url());

  switch (show_state) {
    case ui::SHOW_STATE_MINIMIZED:
      controller->window()->Minimize();
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      controller->window()->Maximize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      if (controller->window()->IsMinimized() ||
          controller->window()->IsMaximized())
        controller->window()->Restore();
      controller->SetFullscreenMode(true, GetExtension()->url());
      break;
    case ui::SHOW_STATE_NORMAL:
      controller->window()->Restore();
      break;
    default:
      break;
  }

  gfx::Rect bounds;
  if (controller->window()->IsMinimized())
    bounds = controller->window()->GetRestoredBounds();
  else
    bounds = controller->window()->GetBounds();
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
        show_state == ui::SHOW_STATE_MAXIMIZED ||
        show_state == ui::SHOW_STATE_FULLSCREEN) {
      error_ = keys::kInvalidWindowStateError;
      return false;
    }
    controller->window()->SetBounds(bounds);
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
      controller->window()->Activate();
    } else {
      if (show_state == ui::SHOW_STATE_MAXIMIZED ||
          show_state == ui::SHOW_STATE_FULLSCREEN) {
        error_ = keys::kInvalidWindowStateError;
        return false;
      }
      controller->window()->Deactivate();
    }
  }

  bool draw_attention = false;
  if (update_props->HasKey(keys::kDrawAttentionKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kDrawAttentionKey, &draw_attention));
    controller->window()->FlashFrame(draw_attention);
  }

  result_.reset(controller->CreateWindowValue());

  return true;
}

bool RemoveWindowFunction::RunImpl() {
  int window_id = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  ExtensionWindowController* controller;
  if (!GetWindowFromWindowID(this, window_id, &controller))
    return false;

  ExtensionWindowController::Reason reason;
  if (!controller->CanClose(&reason)) {
    if (reason == ExtensionWindowController::REASON_NOT_EDITABLE)
      error_ = keys::kTabStripNotEditableError;
    return false;
  }
  controller->window()->Close();
  return true;
}

// Tabs ------------------------------------------------------------------------

bool GetSelectedTabFunction::RunImpl() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  if (HasOptionalArgument(0))
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();
  TabContentsWrapper* contents = tab_strip->GetActiveTabContents();
  if (!contents) {
    error_ = keys::kNoSelectedTabError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateTabValue(contents->web_contents(),
      tab_strip,
      tab_strip->active_index()));
  return true;
}

bool GetAllTabsInWindowFunction::RunImpl() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (HasOptionalArgument(0))
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
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
  QueryArg current_window = ParseBoolQueryArg(query, keys::kCurrentWindowKey);
  QueryArg focused_window =
      ParseBoolQueryArg(query, keys::kLastFocusedWindowKey);

  QueryArg loading = NOT_SET;
  if (query->HasKey(keys::kStatusKey)) {
    std::string status;
    EXTENSION_FUNCTION_VALIDATE(query->GetString(keys::kStatusKey, &status));
    loading = (status == keys::kStatusValueLoading) ? MATCH_TRUE : MATCH_FALSE;
  }

  // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
  // not grant access to the content of the tabs, only to seeing their URLs and
  // meta data.
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

  int window_id = extension_misc::kUnknownWindowId;
  if (query->HasKey(keys::kWindowIdKey))
    EXTENSION_FUNCTION_VALIDATE(
        query->GetInteger(keys::kWindowIdKey, &window_id));

  int index = -1;
  if (query->HasKey(keys::kIndexKey))
    EXTENSION_FUNCTION_VALIDATE(
        query->GetInteger(keys::kIndexKey, &index));

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

    if (!include_incognito() && profile() != (*browser)->profile())
      continue;

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(*browser))
      continue;

    if (window_id == extension_misc::kCurrentWindowId &&
        *browser != GetCurrentBrowser())
      continue;

    if (!MatchesQueryArg(current_window, *browser == GetCurrentBrowser()))
      continue;

    if (!MatchesQueryArg(focused_window,
        *browser == BrowserList::GetLastActive()))
      continue;

    if (!window_type.empty() &&
        window_type !=
        (*browser)->extension_window_controller()->GetWindowTypeText())
      continue;

    TabStripModel* tab_strip = (*browser)->tabstrip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      const WebContents* web_contents =
          tab_strip->GetTabContentsAt(i)->web_contents();

      if (index > -1 && i != index)
        continue;

      if (!MatchesQueryArg(selected, tab_strip->IsTabSelected(i)))
        continue;

      if (!MatchesQueryArg(active, i == tab_strip->active_index()))
        continue;

      if (!MatchesQueryArg(pinned, tab_strip->IsTabPinned(i)))
        continue;

      if (!title.empty() && !MatchPattern(web_contents->GetTitle(),
                                          UTF8ToUTF16(title)))
        continue;

      if (!url_pattern.MatchesURL(web_contents->GetURL()))
        continue;

      if (!MatchesQueryArg(loading, web_contents->IsLoading()))
        continue;

      result->Append(ExtensionTabUtil::CreateTabValue(
          web_contents, tab_strip, i));
    }
  }

  result_.reset(result);
  return true;
}

bool CreateTabFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (args->HasKey(keys::kWindowIdKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(
        keys::kWindowIdKey, &window_id));

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  // Ensure the selected browser is tabbed.
  if (!browser->is_type_tabbed() && browser->IsAttemptingToCloseBrowser())
    browser = Browser::GetTabbedBrowser(profile(), include_incognito());

  if (!browser || !browser->window())
    return false;

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  content::NavigationController* opener = NULL;
  if (args->HasKey(keys::kOpenerTabIdKey)) {
    int opener_id = -1;
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(
        keys::kOpenerTabIdKey, &opener_id));

    TabContentsWrapper* opener_contents = NULL;
    if (!ExtensionTabUtil::GetTabById(
            opener_id, profile(), include_incognito(),
            NULL, NULL, &opener_contents, NULL))
      return false;

    opener = &opener_contents->web_contents()->GetController();
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  std::string url_string;
  GURL url;
  if (args->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                &url_string));
    url = ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string,
                                                       GetExtension());
    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       url_string);
      return false;
    }
  }

  // Don't let extensions crash the browser or renderers.
  if (ExtensionTabUtil::IsCrashURL(url)) {
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

  // The tab may have been created in a different window, so make sure we look
  // at the right tab strip.
  tab_strip = params.browser->tabstrip_model();
  int new_index = tab_strip->GetIndexOfTabContents(params.target_contents);
  if (opener)
    tab_strip->SetOpenerOfTabContentsAt(new_index, opener);

  if (active)
    params.target_contents->web_contents()->GetView()->SetInitialFocus();

  // Return data about the newly created tab.
  if (has_callback()) {
    result_.reset(ExtensionTabUtil::CreateTabValue(
        params.target_contents->web_contents(),
        tab_strip, new_index));
  }

  return true;
}

bool GetTabFunction::RunImpl() {
  int tab_id = -1;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabStripModel* tab_strip = NULL;
  TabContentsWrapper* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_))
    return false;

  result_.reset(ExtensionTabUtil::CreateTabValue(contents->web_contents(),
      tab_strip,
      tab_index));
  return true;
}

bool GetCurrentTabFunction::RunImpl() {
  DCHECK(dispatcher());

  WebContents* contents = dispatcher()->delegate()->GetAssociatedWebContents();
  if (contents)
    result_.reset(ExtensionTabUtil::CreateTabValue(contents));

  return true;
}

bool HighlightTabsFunction::RunImpl() {
  DictionaryValue* info = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &info));

  // Get the window id from the params; default to current window if omitted.
  int window_id = extension_misc::kCurrentWindowId;
  if (info->HasKey(keys::kWindowIdKey))
    EXTENSION_FUNCTION_VALIDATE(
        info->GetInteger(keys::kWindowIdKey, &window_id));

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

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
  result_.reset(
      browser->extension_window_controller()->CreateWindowValueWithTabs());
  return true;
}

UpdateTabFunction::UpdateTabFunction() : web_contents_(NULL) {
}

bool UpdateTabFunction::RunImpl() {
  DictionaryValue* update_props = NULL;
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
    tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());
  } else {
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&tab_id));
  }

  int tab_index = -1;
  TabStripModel* tab_strip = NULL;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_)) {
    return false;
  }

  web_contents_ = contents->web_contents();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  bool is_async = false;
  if (!UpdateURLIfPresent(update_props, &is_async))
    return false;

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
    web_contents_->Focus();
  }

  if (update_props->HasKey(keys::kHighlightedKey)) {
    bool highlighted = false;
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kHighlightedKey, &highlighted));
    if (highlighted != tab_strip->IsTabSelected(tab_index))
      tab_strip->ToggleSelectionAt(tab_index);
  }

  if (update_props->HasKey(keys::kPinnedKey)) {
    bool pinned = false;
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kPinnedKey, &pinned));
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfTabContents(contents);
  }

  if (update_props->HasKey(keys::kOpenerTabIdKey)) {
    int opener_id = -1;
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kOpenerTabIdKey, &opener_id));

    TabContentsWrapper* opener_contents = NULL;
    if (!ExtensionTabUtil::GetTabById(
            opener_id, profile(), include_incognito(),
            NULL, NULL, &opener_contents, NULL))
      return false;

    tab_strip->SetOpenerOfTabContentsAt(
        tab_index, &opener_contents->web_contents()->GetController());
  }

  if (!is_async) {
    PopulateResult();
    SendResponse(true);
  }
  return true;
}

bool UpdateTabFunction::UpdateURLIfPresent(DictionaryValue* update_props,
                                           bool* is_async) {
  if (!update_props->HasKey(keys::kUrlKey))
    return true;

  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(update_props->GetString(
      keys::kUrlKey, &url_string));
  GURL url = ExtensionTabUtil::ResolvePossiblyRelativeURL(
      url_string, GetExtension());

  if (!url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidUrlError, url_string);
    return false;
  }

  // Don't let the extension crash the browser or renderers.
  if (ExtensionTabUtil::IsCrashURL(url)) {
    error_ = keys::kNoCrashBrowserError;
    return false;
  }

  // JavaScript URLs can do the same kinds of things as cross-origin XHR, so
  // we need to check host permissions before allowing them.
  if (url.SchemeIs(chrome::kJavaScriptScheme)) {
    if (!GetExtension()->CanExecuteScriptOnPage(
            web_contents_->GetURL(), NULL, &error_)) {
      return false;
    }

    ExtensionMsg_ExecuteCode_Params params;
    params.request_id = request_id();
    params.extension_id = extension_id();
    params.is_javascript = true;
    params.code = url.path();
    params.run_at = UserScript::DOCUMENT_IDLE;
    params.all_frames = false;
    params.in_main_world = true;

    RenderViewHost* render_view_host = web_contents_->GetRenderViewHost();
    render_view_host->Send(
        new ExtensionMsg_ExecuteCode(render_view_host->GetRoutingID(), params));

    Observe(web_contents_);
    AddRef();  // Balanced in OnExecuteCodeFinished().

    *is_async = true;
    return true;
  }

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());

  // The URL of a tab contents never actually changes to a JavaScript URL, so
  // this check only makes sense in other cases.
  if (!url.SchemeIs(chrome::kJavaScriptScheme))
    DCHECK_EQ(url.spec(), web_contents_->GetURL().spec());

  return true;
}

void UpdateTabFunction::PopulateResult() {
  if (!has_callback())
    return;

  if (GetExtension()->HasAPIPermission(ExtensionAPIPermission::kTab) &&
      web_contents_ != NULL) {
    result_.reset(ExtensionTabUtil::CreateTabValue(web_contents_));
  } else {
    result_.reset(Value::CreateNullValue());
  }
}

void UpdateTabFunction::WebContentsDestroyed(WebContents* tab) {
  CHECK_EQ(tab, web_contents_);
  web_contents_ = NULL;
}

bool UpdateTabFunction::OnMessageReceived(const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
    return false;

  int message_request_id = -1;
  PickleIterator iter(message);
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

  if (success)
    PopulateResult();
  SendResponse(success);

  Observe(NULL);
  Release();  // Balanced in UpdateURLIfPresent().
}

bool MoveTabsFunction::RunImpl() {
  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &tab_value));

  std::vector<int> tab_ids;
  EXTENSION_FUNCTION_VALIDATE(ReadOneOrMoreIntegers(tab_value, &tab_ids));

  DictionaryValue* update_props = NULL;
  int new_index;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &update_props));
  EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(keys::kIndexKey,
      &new_index));

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
      Browser* target_browser = NULL;
      int window_id = extension_misc::kUnknownWindowId;
      EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
          keys::kWindowIdKey, &window_id));

      if (!GetBrowserFromWindowID(this, window_id, &target_browser))
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
        // -1 means set the move location to the last position.
        if (new_index > target_tab_strip->count() || new_index < 0)
          new_index = target_tab_strip->count();

        target_tab_strip->InsertTabContentsAt(
            new_index, contents, TabStripModel::ADD_NONE);

        if (has_callback())
          tab_values.Append(ExtensionTabUtil::CreateTabValue(
              contents->web_contents(), target_tab_strip, new_index));

        continue;
      }
    }

    // Perform a simple within-window move.
    // Clamp move location to the last position.
    // This is ">=" because the move must be to an existing location.
    // -1 means set the move location to the last position.
    if (new_index >= source_tab_strip->count() || new_index < 0)
      new_index = source_tab_strip->count() - 1;

    if (new_index != tab_index)
      source_tab_strip->MoveTabContentsAt(tab_index, new_index, false);

    if (has_callback())
      tab_values.Append(ExtensionTabUtil::CreateTabValue(
          contents->web_contents(), source_tab_strip, new_index));
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
    int tab_id = -1;
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&tab_id));

    Browser* browser = NULL;
    if (!GetTabById(tab_id, profile(), include_incognito(),
                    &browser, NULL, &contents, NULL, &error_))
    return false;
  }

  WebContents* web_contents = contents->web_contents();
  if (web_contents->ShowingInterstitialPage()) {
    // This does as same as Browser::ReloadInternal.
    NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
    OpenURLParams params(entry->GetURL(), Referrer(), CURRENT_TAB,
                         content::PAGE_TRANSITION_RELOAD, false);
    GetCurrentBrowser()->OpenURL(params);
  } else if (bypass_cache) {
    web_contents->GetController().ReloadIgnoringCache(true);
  } else {
    web_contents->GetController().Reload(true);
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
    RenderViewHost* render_view_host =
        contents->web_contents()->GetRenderViewHost();
    render_view_host->GetDelegate()->Close(render_view_host);
  }
  return true;
}

bool CaptureVisibleTabFunction::GetTabToCapture(
    WebContents** web_contents, TabContentsWrapper** wrapper) {
  Browser* browser = NULL;
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  if (HasOptionalArgument(0))
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &window_id));

  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  *web_contents = browser->GetSelectedWebContents();
  if (*web_contents == NULL) {
    error_ = keys::kInternalVisibleTabCaptureError;
    return false;
  }

  *wrapper = browser->GetSelectedTabContentsWrapper();

  return true;
};

bool CaptureVisibleTabFunction::RunImpl() {
  WebContents* web_contents = NULL;
  TabContentsWrapper* wrapper = NULL;
  if (!GetTabToCapture(&web_contents, &wrapper))
    return false;

  image_format_ = FORMAT_JPEG;  // Default format is JPEG.
  image_quality_ = kDefaultQuality;  // Default quality setting.

  if (HasOptionalArgument(1)) {
    DictionaryValue* options = NULL;
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

  // captureVisibleTab() can return an image containing sensitive information
  // that the browser would otherwise protect.  Ensure the extension has
  // permission to do this.
  if (!GetExtension()->CanCaptureVisiblePage(web_contents->GetURL(), &error_))
    return false;

  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  content::RenderWidgetHostView* view = render_view_host->GetView();
  if (!view) {
    error_ = keys::kInternalVisibleTabCaptureError;
    return false;
  }
  skia::PlatformCanvas* temp_canvas = new skia::PlatformCanvas;
  render_view_host->CopyFromBackingStore(
      gfx::Rect(),
      view->GetViewBounds().size(),
      temp_canvas,
      base::Bind(&CaptureVisibleTabFunction::CopyFromBackingStoreComplete,
                 this,
                 base::Owned(temp_canvas)));
  return true;
}

void CaptureVisibleTabFunction::CopyFromBackingStoreComplete(
    skia::PlatformCanvas* canvas,
    bool succeeded) {
  if (succeeded) {
    VLOG(1) << "captureVisibleTab() got image from backing store.";
    SendResultFromBitmap(skia::GetTopDevice(*canvas)->accessBitmap(false));
    return;
  }

  WebContents* web_contents = NULL;
  TabContentsWrapper* wrapper = NULL;
  if (!GetTabToCapture(&web_contents, &wrapper)) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
    return;
  }

  // Ask the renderer for a snapshot of the tab.
  registrar_.Add(this,
                 chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
                 content::Source<WebContents>(web_contents));
  AddRef();  // Balanced in CaptureVisibleTabFunction::Observe().
  wrapper->snapshot_tab_helper()->CaptureSnapshot();
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

  if (contents->web_contents()->GetController().NeedsReload()) {
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
                 content::Source<WebContents>(contents->web_contents()));
  registrar_.Add(
      this, chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(
          &(contents->web_contents()->GetController())));
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(
          &(contents->web_contents()->GetController())));
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
