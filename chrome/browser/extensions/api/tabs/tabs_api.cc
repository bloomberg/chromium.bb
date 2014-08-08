// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "apps/app_window.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"

#if defined(USE_ASH)
#include "apps/app_window_registry.h"
#include "ash/ash_switches.h"
#include "chrome/browser/extensions/api/tabs/ash_panel_contents.h"
#endif

using apps::AppWindow;
using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace extensions {

namespace windows = api::windows;
namespace keys = tabs_constants;
namespace tabs = api::tabs;

using api::tabs::InjectDetails;

namespace {

bool GetBrowserFromWindowID(ChromeUIThreadExtensionFunction* function,
                            int window_id,
                            Browser** browser) {
  std::string error;
  Browser* result;
  result =
      ExtensionTabUtil::GetBrowserFromWindowID(function, window_id, &error);
  if (!result) {
    function->SetError(error);
    return false;
  }

  *browser = result;
  return true;
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
bool GetTabById(int tab_id,
                Profile* profile,
                bool include_incognito,
                Browser** browser,
                TabStripModel** tab_strip,
                content::WebContents** contents,
                int* tab_index,
                std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                   browser, tab_strip, contents, tab_index)) {
    return true;
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::IntToString(tab_id));
  }

  return false;
}

// Returns true if either |boolean| is a null pointer, or if |*boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(bool* boolean, bool value) {
  return !boolean || *boolean == value;
}

template <typename T>
void AssignOptionalValue(const scoped_ptr<T>& source,
                         scoped_ptr<T>& destination) {
  if (source.get()) {
    destination.reset(new T(*source.get()));
  }
}

}  // namespace

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZoomSettings::MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZoomSettings::SCOPE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZoomSettings::MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZoomSettings::SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZoomSettings::MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZoomSettings::SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZoomSettings::MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZoomSettings::SCOPE_PER_TAB;
      break;
  }
}

// Windows ---------------------------------------------------------------------

bool WindowsGetFunction::RunSync() {
  scoped_ptr<windows::Get::Params> params(windows::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  WindowController* controller;
  if (!windows_util::GetWindowFromWindowID(this,
                                           params->window_id,
                                           &controller)) {
    return false;
  }

  if (populate_tabs)
    SetResult(controller->CreateWindowValueWithTabs(extension()));
  else
    SetResult(controller->CreateWindowValue());
  return true;
}

bool WindowsGetCurrentFunction::RunSync() {
  scoped_ptr<windows::GetCurrent::Params> params(
      windows::GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  WindowController* controller;
  if (!windows_util::GetWindowFromWindowID(this,
                                           extension_misc::kCurrentWindowId,
                                           &controller)) {
    return false;
  }
  if (populate_tabs)
    SetResult(controller->CreateWindowValueWithTabs(extension()));
  else
    SetResult(controller->CreateWindowValue());
  return true;
}

bool WindowsGetLastFocusedFunction::RunSync() {
  scoped_ptr<windows::GetLastFocused::Params> params(
      windows::GetLastFocused::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  // Note: currently this returns the last active browser. If we decide to
  // include other window types (e.g. panels), we will need to add logic to
  // WindowControllerList that mirrors the active behavior of BrowserList.
  Browser* browser = chrome::FindAnyBrowser(
      GetProfile(), include_incognito(), chrome::GetActiveDesktop());
  if (!browser || !browser->window()) {
    error_ = keys::kNoLastFocusedWindowError;
    return false;
  }
  WindowController* controller =
      browser->extension_window_controller();
  if (populate_tabs)
    SetResult(controller->CreateWindowValueWithTabs(extension()));
  else
    SetResult(controller->CreateWindowValue());
  return true;
}

bool WindowsGetAllFunction::RunSync() {
  scoped_ptr<windows::GetAll::Params> params(
      windows::GetAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool populate_tabs = false;
  if (params->get_info.get() && params->get_info->populate.get())
    populate_tabs = *params->get_info->populate;

  base::ListValue* window_list = new base::ListValue();
  const WindowControllerList::ControllerList& windows =
      WindowControllerList::GetInstance()->windows();
  for (WindowControllerList::ControllerList::const_iterator iter =
           windows.begin();
       iter != windows.end(); ++iter) {
    if (!this->CanOperateOnWindow(*iter))
      continue;
    if (populate_tabs)
      window_list->Append((*iter)->CreateWindowValueWithTabs(extension()));
    else
      window_list->Append((*iter)->CreateWindowValue());
  }
  SetResult(window_list);
  return true;
}

bool WindowsCreateFunction::ShouldOpenIncognitoWindow(
    const windows::Create::Params::CreateData* create_data,
    std::vector<GURL>* urls, bool* is_error) {
  *is_error = false;
  const IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(GetProfile()->GetPrefs());
  bool incognito = false;
  if (create_data && create_data->incognito) {
    incognito = *create_data->incognito;
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
  if (incognito && !GetProfile()->IsGuestSession()) {
    std::string first_url_erased;
    for (size_t i = 0; i < urls->size();) {
      if (chrome::IsURLAllowedInIncognito((*urls)[i], GetProfile())) {
        i++;
      } else {
        if (first_url_erased.empty())
          first_url_erased = (*urls)[i].spec();
        urls->erase(urls->begin() + i);
      }
    }
    if (urls->empty() && !first_url_erased.empty()) {
      error_ = ErrorUtils::FormatErrorMessage(
          keys::kURLsNotAllowedInIncognitoError, first_url_erased);
      *is_error = true;
      return false;
    }
  }
  return incognito;
}

bool WindowsCreateFunction::RunSync() {
  scoped_ptr<windows::Create::Params> params(
      windows::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::vector<GURL> urls;
  TabStripModel* source_tab_strip = NULL;
  int tab_index = -1;

  windows::Create::Params::CreateData* create_data = params->create_data.get();

  // Look for optional url.
  if (create_data && create_data->url) {
    std::vector<std::string> url_strings;
    // First, get all the URLs the client wants to open.
    if (create_data->url->as_string)
      url_strings.push_back(*create_data->url->as_string);
    else if (create_data->url->as_strings)
      url_strings.swap(*create_data->url->as_strings);

    // Second, resolve, validate and convert them to GURLs.
    for (std::vector<std::string>::iterator i = url_strings.begin();
         i != url_strings.end(); ++i) {
      GURL url = ExtensionTabUtil::ResolvePossiblyRelativeURL(*i, extension());
      if (!url.is_valid()) {
        error_ = ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, *i);
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

  // Look for optional tab id.
  if (create_data && create_data->tab_id) {
    // Find the tab. |source_tab_strip| and |tab_index| will later be used to
    // move the tab into the created window.
    if (!GetTabById(*create_data->tab_id,
                    GetProfile(),
                    include_incognito(),
                    NULL,
                    &source_tab_strip,
                    NULL,
                    &tab_index,
                    &error_))
      return false;
  }

  Profile* window_profile = GetProfile();
  Browser::Type window_type = Browser::TYPE_TABBED;
  bool create_panel = false;

  // panel_create_mode only applies if create_panel = true
  PanelManager::CreateMode panel_create_mode = PanelManager::CREATE_AS_DOCKED;

  gfx::Rect window_bounds;
  bool focused = true;
  bool saw_focus_key = false;
  std::string extension_id;

  // Decide whether we are opening a normal window or an incognito window.
  bool is_error = true;
  bool open_incognito_window = ShouldOpenIncognitoWindow(create_data, &urls,
                                                         &is_error);
  if (is_error) {
    // error_ member variable is set inside of ShouldOpenIncognitoWindow.
    return false;
  }
  if (open_incognito_window) {
    window_profile = window_profile->GetOffTheRecordProfile();
  }

  if (create_data) {
    // Figure out window type before figuring out bounds so that default
    // bounds can be set according to the window type.
    switch (create_data->type) {
      case windows::Create::Params::CreateData::TYPE_POPUP:
        window_type = Browser::TYPE_POPUP;
        extension_id = extension()->id();
        break;
      case windows::Create::Params::CreateData::TYPE_PANEL:
      case windows::Create::Params::CreateData::TYPE_DETACHED_PANEL: {
        extension_id = extension()->id();
        bool use_panels = PanelManager::ShouldUsePanels(extension_id);
        if (use_panels) {
          create_panel = true;
          // Non-ash supports both docked and detached panel types.
          if (chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH &&
              create_data->type ==
              windows::Create::Params::CreateData::TYPE_DETACHED_PANEL) {
            panel_create_mode = PanelManager::CREATE_AS_DETACHED;
          }
        } else {
          window_type = Browser::TYPE_POPUP;
        }
        break;
      }
      case windows::Create::Params::CreateData::TYPE_NONE:
      case windows::Create::Params::CreateData::TYPE_NORMAL:
        break;
      default:
        error_ = keys::kInvalidWindowTypeError;
        return false;
    }

    // Initialize default window bounds according to window type.
    if (window_type == Browser::TYPE_TABBED ||
        window_type == Browser::TYPE_POPUP ||
        create_panel) {
      // Try to position the new browser relative to its originating
      // browser window. The call offsets the bounds by kWindowTilePixels
      // (defined in WindowSizer to be 10).
      //
      // NOTE(rafaelw): It's ok if GetCurrentBrowser() returns NULL here.
      // GetBrowserWindowBounds will default to saved "default" values for
      // the app.
      ui::WindowShowState show_state = ui::SHOW_STATE_DEFAULT;
      WindowSizer::GetBrowserWindowBoundsAndShowState(std::string(),
                                                      gfx::Rect(),
                                                      GetCurrentBrowser(),
                                                      &window_bounds,
                                                      &show_state);
    }

    if (create_panel && PanelManager::CREATE_AS_DETACHED == panel_create_mode) {
      window_bounds.set_origin(
          PanelManager::GetInstance()->GetDefaultDetachedPanelOrigin());
    }

    // Any part of the bounds can optionally be set by the caller.
    if (create_data->left)
      window_bounds.set_x(*create_data->left);

    if (create_data->top)
      window_bounds.set_y(*create_data->top);

    if (create_data->width)
      window_bounds.set_width(*create_data->width);

    if (create_data->height)
      window_bounds.set_height(*create_data->height);

    if (create_data->focused) {
      focused = *create_data->focused;
      saw_focus_key = true;
    }
  }

  if (create_panel) {
    if (urls.empty())
      urls.push_back(GURL(chrome::kChromeUINewTabURL));

#if defined(USE_ASH)
    if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH) {
      AppWindow::CreateParams create_params;
      create_params.window_type = AppWindow::WINDOW_TYPE_V1_PANEL;
      create_params.window_spec.bounds = window_bounds;
      create_params.focused = saw_focus_key && focused;
      AppWindow* app_window =
          new AppWindow(window_profile, new ChromeAppDelegate(), extension());
      AshPanelContents* ash_panel_contents = new AshPanelContents(app_window);
      app_window->Init(urls[0], ash_panel_contents, create_params);
      SetResult(ash_panel_contents->GetExtensionWindowController()
                    ->CreateWindowValueWithTabs(extension()));
      return true;
    }
#endif
    std::string title =
        web_app::GenerateApplicationNameFromExtensionId(extension_id);
    // Note: Panels ignore all but the first url provided.
    Panel* panel = PanelManager::GetInstance()->CreatePanel(
        title, window_profile, urls[0], window_bounds, panel_create_mode);

    // Unlike other window types, Panels do not take focus by default.
    if (!saw_focus_key || !focused)
      panel->ShowInactive();
    else
      panel->Show();

    SetResult(panel->extension_window_controller()->CreateWindowValueWithTabs(
        extension()));
    return true;
  }

  // Create a new BrowserWindow.
  chrome::HostDesktopType host_desktop_type = chrome::GetActiveDesktop();
  if (create_panel)
    window_type = Browser::TYPE_POPUP;
  Browser::CreateParams create_params(window_type, window_profile,
                                      host_desktop_type);
  if (extension_id.empty()) {
    create_params.initial_bounds = window_bounds;
  } else {
    create_params = Browser::CreateParams::CreateForApp(
        web_app::GenerateApplicationNameFromExtensionId(extension_id),
        false /* trusted_source */,
        window_bounds,
        window_profile,
        host_desktop_type);
  }
  create_params.initial_show_state = ui::SHOW_STATE_NORMAL;
  create_params.host_desktop_type = chrome::GetActiveDesktop();

  Browser* new_window = new Browser(create_params);

  for (std::vector<GURL>::iterator i = urls.begin(); i != urls.end(); ++i) {
    WebContents* tab = chrome::AddSelectedTabWithURL(
        new_window, *i, content::PAGE_TRANSITION_LINK);
    if (create_panel) {
      TabHelper::FromWebContents(tab)->SetExtensionAppIconById(extension_id);
    }
  }

  WebContents* contents = NULL;
  // Move the tab into the created window only if it's an empty popup or it's
  // a tabbed window.
  if ((window_type == Browser::TYPE_POPUP && urls.empty()) ||
      window_type == Browser::TYPE_TABBED) {
    if (source_tab_strip)
      contents = source_tab_strip->DetachWebContentsAt(tab_index);
    if (contents) {
      TabStripModel* target_tab_strip = new_window->tab_strip_model();
      target_tab_strip->InsertWebContentsAt(urls.size(), contents,
                                            TabStripModel::ADD_NONE);
    }
  }
  // Create a new tab if the created window is still empty. Don't create a new
  // tab when it is intended to create an empty popup.
  if (!contents && urls.empty() && window_type != Browser::TYPE_POPUP) {
    chrome::NewTab(new_window);
  }
  chrome::SelectNumberedTab(new_window, 0);

  // Unlike other window types, Panels do not take focus by default.
  if (!saw_focus_key && create_panel)
    focused = false;

  if (focused)
    new_window->window()->Show();
  else
    new_window->window()->ShowInactive();

  if (new_window->profile()->IsOffTheRecord() &&
      !GetProfile()->IsOffTheRecord() && !include_incognito()) {
    // Don't expose incognito windows if extension itself works in non-incognito
    // profile and CanCrossIncognito isn't allowed.
    SetResult(base::Value::CreateNullValue());
  } else {
    SetResult(
        new_window->extension_window_controller()->CreateWindowValueWithTabs(
            extension()));
  }

  return true;
}

bool WindowsUpdateFunction::RunSync() {
  scoped_ptr<windows::Update::Params> params(
      windows::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  WindowController* controller;
  if (!windows_util::GetWindowFromWindowID(this, params->window_id,
                                            &controller))
    return false;

  ui::WindowShowState show_state = ui::SHOW_STATE_DEFAULT;  // No change.
  switch (params->update_info.state) {
    case windows::Update::Params::UpdateInfo::STATE_NORMAL:
      show_state = ui::SHOW_STATE_NORMAL;
      break;
    case windows::Update::Params::UpdateInfo::STATE_MINIMIZED:
      show_state = ui::SHOW_STATE_MINIMIZED;
      break;
    case windows::Update::Params::UpdateInfo::STATE_MAXIMIZED:
      show_state = ui::SHOW_STATE_MAXIMIZED;
      break;
    case windows::Update::Params::UpdateInfo::STATE_FULLSCREEN:
      show_state = ui::SHOW_STATE_FULLSCREEN;
      break;
    case windows::Update::Params::UpdateInfo::STATE_NONE:
      break;
    default:
      error_ = keys::kInvalidWindowStateError;
      return false;
  }

  if (show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_DEFAULT)
    controller->SetFullscreenMode(false, extension()->url());

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
      controller->SetFullscreenMode(true, extension()->url());
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
  if (params->update_info.left) {
    bounds.set_x(*params->update_info.left);
    set_bounds = true;
  }

  if (params->update_info.top) {
    bounds.set_y(*params->update_info.top);
    set_bounds = true;
  }

  if (params->update_info.width) {
    bounds.set_width(*params->update_info.width);
    set_bounds = true;
  }

  if (params->update_info.height) {
    bounds.set_height(*params->update_info.height);
    set_bounds = true;
  }

  if (set_bounds) {
    if (show_state == ui::SHOW_STATE_MINIMIZED ||
        show_state == ui::SHOW_STATE_MAXIMIZED ||
        show_state == ui::SHOW_STATE_FULLSCREEN) {
      error_ = keys::kInvalidWindowStateError;
      return false;
    }
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/251813 .
    controller->window()->SetBounds(bounds);
  }

  if (params->update_info.focused) {
    if (*params->update_info.focused) {
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

  if (params->update_info.draw_attention)
    controller->window()->FlashFrame(*params->update_info.draw_attention);

  SetResult(controller->CreateWindowValue());

  return true;
}

bool WindowsRemoveFunction::RunSync() {
  scoped_ptr<windows::Remove::Params> params(
      windows::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  WindowController* controller;
  if (!windows_util::GetWindowFromWindowID(this, params->window_id,
                                           &controller))
    return false;

  WindowController::Reason reason;
  if (!controller->CanClose(&reason)) {
    if (reason == WindowController::REASON_NOT_EDITABLE)
      error_ = keys::kTabStripNotEditableError;
    return false;
  }
  controller->window()->Close();
  return true;
}

// Tabs ------------------------------------------------------------------------

bool TabsGetSelectedFunction::RunSync() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  scoped_ptr<tabs::GetSelected::Params> params(
      tabs::GetSelected::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->window_id.get())
    window_id = *params->window_id;

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  TabStripModel* tab_strip = browser->tab_strip_model();
  WebContents* contents = tab_strip->GetActiveWebContents();
  if (!contents) {
    error_ = keys::kNoSelectedTabError;
    return false;
  }
  SetResult(ExtensionTabUtil::CreateTabValue(
      contents, tab_strip, tab_strip->active_index(), extension()));
  return true;
}

bool TabsGetAllInWindowFunction::RunSync() {
  scoped_ptr<tabs::GetAllInWindow::Params> params(
      tabs::GetAllInWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->window_id.get())
    window_id = *params->window_id;

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  SetResult(ExtensionTabUtil::CreateTabList(browser, extension()));

  return true;
}

bool TabsQueryFunction::RunSync() {
  scoped_ptr<tabs::Query::Params> params(tabs::Query::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool loading_status_set = params->query_info.status !=
      tabs::Query::Params::QueryInfo::STATUS_NONE;
  bool loading = params->query_info.status ==
      tabs::Query::Params::QueryInfo::STATUS_LOADING;

  // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
  // not grant access to the content of the tabs, only to seeing their URLs and
  // meta data.
  URLPattern url_pattern(URLPattern::SCHEME_ALL, "<all_urls>");
  if (params->query_info.url.get())
    url_pattern = URLPattern(URLPattern::SCHEME_ALL, *params->query_info.url);

  std::string title;
  if (params->query_info.title.get())
    title = *params->query_info.title;

  int window_id = extension_misc::kUnknownWindowId;
  if (params->query_info.window_id.get())
    window_id = *params->query_info.window_id;

  int index = -1;
  if (params->query_info.index.get())
    index = *params->query_info.index;

  std::string window_type;
  if (params->query_info.window_type !=
      tabs::Query::Params::QueryInfo::WINDOW_TYPE_NONE) {
    window_type = tabs::Query::Params::QueryInfo::ToString(
        params->query_info.window_type);
  }

  base::ListValue* result = new base::ListValue();
  Browser* last_active_browser = chrome::FindAnyBrowser(
      GetProfile(), include_incognito(), chrome::GetActiveDesktop());
  Browser* current_browser = GetCurrentBrowser();
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    if (!GetProfile()->IsSameProfile(browser->profile()))
      continue;

    if (!browser->window())
      continue;

    if (!include_incognito() && GetProfile() != browser->profile())
      continue;

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser))
      continue;

    if (window_id == extension_misc::kCurrentWindowId &&
        browser != current_browser) {
      continue;
    }

    if (!MatchesBool(params->query_info.current_window.get(),
                     browser == current_browser)) {
      continue;
    }

    if (!MatchesBool(params->query_info.last_focused_window.get(),
                     browser == last_active_browser)) {
      continue;
    }

    if (!window_type.empty() &&
        window_type !=
            browser->extension_window_controller()->GetWindowTypeText()) {
      continue;
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      WebContents* web_contents = tab_strip->GetWebContentsAt(i);

      if (index > -1 && i != index)
        continue;

      if (!MatchesBool(params->query_info.highlighted.get(),
                       tab_strip->IsTabSelected(i))) {
        continue;
      }

      if (!MatchesBool(params->query_info.active.get(),
                       i == tab_strip->active_index())) {
        continue;
      }

      if (!MatchesBool(params->query_info.pinned.get(),
                       tab_strip->IsTabPinned(i))) {
        continue;
      }

      if (!title.empty() && !MatchPattern(web_contents->GetTitle(),
                                          base::UTF8ToUTF16(title)))
        continue;

      if (!url_pattern.MatchesURL(web_contents->GetURL()))
        continue;

      if (loading_status_set && loading != web_contents->IsLoading())
        continue;

      result->Append(ExtensionTabUtil::CreateTabValue(
          web_contents, tab_strip, i, extension()));
    }
  }

  SetResult(result);
  return true;
}

bool TabsCreateFunction::RunSync() {
  scoped_ptr<tabs::Create::Params> params(tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionTabUtil::OpenTabParams options;
  AssignOptionalValue(params->create_properties.window_id, options.window_id);
  AssignOptionalValue(params->create_properties.opener_tab_id,
                      options.opener_tab_id);
  AssignOptionalValue(params->create_properties.selected, options.active);
  // The 'active' property has replaced the 'selected' property.
  AssignOptionalValue(params->create_properties.active, options.active);
  AssignOptionalValue(params->create_properties.pinned, options.pinned);
  AssignOptionalValue(params->create_properties.index, options.index);
  AssignOptionalValue(params->create_properties.url, options.url);

  std::string error;
  scoped_ptr<base::DictionaryValue> result(
      ExtensionTabUtil::OpenTab(this, options, &error));
  if (!result) {
    SetError(error);
    return false;
  }

  // Return data about the newly created tab.
  if (has_callback()) {
    SetResult(result.release());
  }
  return true;
}

bool TabsDuplicateFunction::RunSync() {
  scoped_ptr<tabs::Duplicate::Params> params(
      tabs::Duplicate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->tab_id;

  Browser* browser = NULL;
  TabStripModel* tab_strip = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id,
                  GetProfile(),
                  include_incognito(),
                  &browser,
                  &tab_strip,
                  NULL,
                  &tab_index,
                  &error_)) {
    return false;
  }

  WebContents* new_contents = chrome::DuplicateTabAt(browser, tab_index);
  if (!has_callback())
    return true;

  // Duplicated tab may not be in the same window as the original, so find
  // the window and the tab.
  TabStripModel* new_tab_strip = NULL;
  int new_tab_index = -1;
  ExtensionTabUtil::GetTabStripModel(new_contents,
                                     &new_tab_strip,
                                     &new_tab_index);
  if (!new_tab_strip || new_tab_index == -1) {
    return false;
  }

  // Return data about the newly created tab.
  SetResult(ExtensionTabUtil::CreateTabValue(
      new_contents, new_tab_strip, new_tab_index, extension()));

  return true;
}

bool TabsGetFunction::RunSync() {
  scoped_ptr<tabs::Get::Params> params(tabs::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->tab_id;

  TabStripModel* tab_strip = NULL;
  WebContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id,
                  GetProfile(),
                  include_incognito(),
                  NULL,
                  &tab_strip,
                  &contents,
                  &tab_index,
                  &error_))
    return false;

  SetResult(ExtensionTabUtil::CreateTabValue(
      contents, tab_strip, tab_index, extension()));
  return true;
}

bool TabsGetCurrentFunction::RunSync() {
  DCHECK(dispatcher());

  WebContents* contents = dispatcher()->delegate()->GetAssociatedWebContents();
  if (contents)
    SetResult(ExtensionTabUtil::CreateTabValue(contents, extension()));

  return true;
}

bool TabsHighlightFunction::RunSync() {
  scoped_ptr<tabs::Highlight::Params> params(
      tabs::Highlight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the window id from the params; default to current window if omitted.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->highlight_info.window_id.get())
    window_id = *params->highlight_info.window_id;

  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return false;

  TabStripModel* tabstrip = browser->tab_strip_model();
  ui::ListSelectionModel selection;
  int active_index = -1;

  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& tab_indices = *params->highlight_info.tabs.as_integers;
    // Create a new selection model as we read the list of tab indices.
    for (size_t i = 0; i < tab_indices.size(); ++i) {
      if (!HighlightTab(tabstrip, &selection, &active_index, tab_indices[i]))
        return false;
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    if (!HighlightTab(tabstrip,
                      &selection,
                      &active_index,
                      *params->highlight_info.tabs.as_integer)) {
      return false;
    }
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty()) {
    error_ = keys::kNoHighlightedTabError;
    return false;
  }

  selection.set_active(active_index);
  browser->tab_strip_model()->SetSelectionFromModel(selection);
  SetResult(browser->extension_window_controller()->CreateWindowValueWithTabs(
      extension()));
  return true;
}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         int* active_index,
                                         int index) {
  // Make sure the index is in range.
  if (!tabstrip->ContainsIndex(index)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kTabIndexNotFoundError, base::IntToString(index));
    return false;
  }

  // By default, we make the first tab in the list active.
  if (*active_index == -1)
    *active_index = index;

  selection->AddIndexToSelection(index);
  return true;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(NULL) {
}

bool TabsUpdateFunction::RunAsync() {
  scoped_ptr<tabs::Update::Params> params(tabs::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = -1;
  WebContents* contents = NULL;
  if (!params->tab_id.get()) {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }
    contents = browser->tab_strip_model()->GetActiveWebContents();
    if (!contents) {
      error_ = keys::kNoSelectedTabError;
      return false;
    }
    tab_id = SessionID::IdForTab(contents);
  } else {
    tab_id = *params->tab_id;
  }

  int tab_index = -1;
  TabStripModel* tab_strip = NULL;
  if (!GetTabById(tab_id,
                  GetProfile(),
                  include_incognito(),
                  NULL,
                  &tab_strip,
                  &contents,
                  &tab_index,
                  &error_)) {
    return false;
  }

  web_contents_ = contents;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  bool is_async = false;
  if (params->update_properties.url.get() &&
      !UpdateURL(*params->update_properties.url, tab_id, &is_async)) {
    return false;
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected.get())
    active = *params->update_properties.selected;

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active.get())
    active = *params->update_properties.active;

  if (active) {
    if (tab_strip->active_index() != tab_index) {
      tab_strip->ActivateTabAt(tab_index, false);
      DCHECK_EQ(contents, tab_strip->GetActiveWebContents());
    }
  }

  if (params->update_properties.highlighted.get()) {
    bool highlighted = *params->update_properties.highlighted;
    if (highlighted != tab_strip->IsTabSelected(tab_index))
      tab_strip->ToggleSelectionAt(tab_index);
  }

  if (params->update_properties.pinned.get()) {
    bool pinned = *params->update_properties.pinned;
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfWebContents(contents);
  }

  if (params->update_properties.opener_tab_id.get()) {
    int opener_id = *params->update_properties.opener_tab_id;

    WebContents* opener_contents = NULL;
    if (!ExtensionTabUtil::GetTabById(opener_id,
                                      GetProfile(),
                                      include_incognito(),
                                      NULL,
                                      NULL,
                                      &opener_contents,
                                      NULL))
      return false;

    tab_strip->SetOpenerOfWebContentsAt(tab_index, opener_contents);
  }

  if (!is_async) {
    PopulateResult();
    SendResponse(true);
  }
  return true;
}

bool TabsUpdateFunction::UpdateURL(const std::string &url_string,
                                   int tab_id,
                                   bool* is_async) {
  GURL url =
      ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string, extension());

  if (!url.is_valid()) {
    error_ = ErrorUtils::FormatErrorMessage(
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
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    content::RenderProcessHost* process = web_contents_->GetRenderProcessHost();
    if (!extension()->permissions_data()->CanAccessPage(
            extension(),
            web_contents_->GetURL(),
            web_contents_->GetURL(),
            tab_id,
            process ? process->GetID() : -1,
            &error_)) {
      return false;
    }

    TabHelper::FromWebContents(web_contents_)->script_executor()->ExecuteScript(
        extension_id(),
        ScriptExecutor::JAVASCRIPT,
        url.GetContent(),
        ScriptExecutor::TOP_FRAME,
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        UserScript::DOCUMENT_IDLE,
        ScriptExecutor::MAIN_WORLD,
        ScriptExecutor::DEFAULT_PROCESS,
        GURL(),
        GURL(),
        user_gesture_,
        ScriptExecutor::NO_RESULT,
        base::Bind(&TabsUpdateFunction::OnExecuteCodeFinished, this));

    *is_async = true;
    return true;
  }

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());

  // The URL of a tab contents never actually changes to a JavaScript URL, so
  // this check only makes sense in other cases.
  if (!url.SchemeIs(url::kJavaScriptScheme))
    DCHECK_EQ(url.spec(), web_contents_->GetURL().spec());

  return true;
}

void TabsUpdateFunction::PopulateResult() {
  if (!has_callback())
    return;

  SetResult(ExtensionTabUtil::CreateTabValue(web_contents_, extension()));
}

void TabsUpdateFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& url,
    const base::ListValue& script_result) {
  if (error.empty())
    PopulateResult();
  else
    error_ = error;
  SendResponse(error.empty());
}

bool TabsMoveFunction::RunSync() {
  scoped_ptr<tabs::Move::Params> params(tabs::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int new_index = params->move_properties.index;
  int* window_id = params->move_properties.window_id.get();
  scoped_ptr<base::ListValue> tab_values(new base::ListValue());

  size_t num_tabs = 0;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    num_tabs = tab_ids.size();
    for (size_t i = 0; i < tab_ids.size(); ++i) {
      if (!MoveTab(tab_ids[i], &new_index, i, tab_values.get(), window_id))
        return false;
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    num_tabs = 1;
    if (!MoveTab(*params->tab_ids.as_integer,
                 &new_index,
                 0,
                 tab_values.get(),
                 window_id)) {
      return false;
    }
  }

  if (!has_callback())
    return true;

  if (num_tabs == 0) {
    error_ = "No tabs given.";
    return false;
  } else if (num_tabs == 1) {
    scoped_ptr<base::Value> value;
    CHECK(tab_values.get()->Remove(0, &value));
    SetResult(value.release());
  } else {
    // Only return the results as an array if there are multiple tabs.
    SetResult(tab_values.release());
  }

  return true;
}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               int iteration,
                               base::ListValue* tab_values,
                               int* window_id) {
  Browser* source_browser = NULL;
  TabStripModel* source_tab_strip = NULL;
  WebContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id,
                  GetProfile(),
                  include_incognito(),
                  &source_browser,
                  &source_tab_strip,
                  &contents,
                  &tab_index,
                  &error_)) {
    return false;
  }

  // Don't let the extension move the tab if the user is dragging tabs.
  if (!source_browser->window()->IsTabStripEditable()) {
    error_ = keys::kTabStripNotEditableError;
    return false;
  }

  // Insert the tabs one after another.
  *new_index += iteration;

  if (window_id) {
    Browser* target_browser = NULL;

    if (!GetBrowserFromWindowID(this, *window_id, &target_browser))
      return false;

    if (!target_browser->window()->IsTabStripEditable()) {
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
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      WebContents* web_contents =
          source_tab_strip->DetachWebContentsAt(tab_index);
      if (!web_contents) {
        error_ = ErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, base::IntToString(tab_id));
        return false;
      }

      // Clamp move location to the last position.
      // This is ">" because it can append to a new index position.
      // -1 means set the move location to the last position.
      if (*new_index > target_tab_strip->count() || *new_index < 0)
        *new_index = target_tab_strip->count();

      target_tab_strip->InsertWebContentsAt(
          *new_index, web_contents, TabStripModel::ADD_NONE);

      if (has_callback()) {
        tab_values->Append(ExtensionTabUtil::CreateTabValue(
            web_contents, target_tab_strip, *new_index, extension()));
      }

      return true;
    }
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  // -1 means set the move location to the last position.
  if (*new_index >= source_tab_strip->count() || *new_index < 0)
    *new_index = source_tab_strip->count() - 1;

  if (*new_index != tab_index)
    source_tab_strip->MoveWebContentsAt(tab_index, *new_index, false);

  if (has_callback()) {
    tab_values->Append(ExtensionTabUtil::CreateTabValue(
        contents, source_tab_strip, *new_index, extension()));
  }

  return true;
}

bool TabsReloadFunction::RunSync() {
  scoped_ptr<tabs::Reload::Params> params(
      tabs::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool bypass_cache = false;
  if (params->reload_properties.get() &&
      params->reload_properties->bypass_cache.get()) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  content::WebContents* web_contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (!params->tab_id.get()) {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }

    if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, NULL))
      return false;
  } else {
    int tab_id = *params->tab_id;

    Browser* browser = NULL;
    if (!GetTabById(tab_id,
                    GetProfile(),
                    include_incognito(),
                    &browser,
                    NULL,
                    &web_contents,
                    NULL,
                    &error_)) {
      return false;
    }
  }

  if (web_contents->ShowingInterstitialPage()) {
    // This does as same as Browser::ReloadInternal.
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
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

bool TabsRemoveFunction::RunSync() {
  scoped_ptr<tabs::Remove::Params> params(tabs::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    for (size_t i = 0; i < tab_ids.size(); ++i) {
      if (!RemoveTab(tab_ids[i]))
        return false;
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    if (!RemoveTab(*params->tab_ids.as_integer.get()))
      return false;
  }
  return true;
}

bool TabsRemoveFunction::RemoveTab(int tab_id) {
  Browser* browser = NULL;
  WebContents* contents = NULL;
  if (!GetTabById(tab_id,
                  GetProfile(),
                  include_incognito(),
                  &browser,
                  NULL,
                  &contents,
                  NULL,
                  &error_)) {
    return false;
  }

  // Don't let the extension remove a tab if the user is dragging tabs around.
  if (!browser->window()->IsTabStripEditable()) {
    error_ = keys::kTabStripNotEditableError;
    return false;
  }
  // There's a chance that the tab is being dragged, or we're in some other
  // nested event loop. This code path ensures that the tab is safely closed
  // under such circumstances, whereas |TabStripModel::CloseWebContentsAt()|
  // does not.
  contents->Close();
  return true;
}

bool TabsCaptureVisibleTabFunction::IsScreenshotEnabled() {
  PrefService* service = GetProfile()->GetPrefs();
  if (service->GetBoolean(prefs::kDisableScreenshots)) {
    error_ = keys::kScreenshotsDisabled;
    return false;
  }
  return true;
}

WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(int window_id) {
  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(this, window_id, &browser))
    return NULL;

  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!contents) {
    error_ = keys::kInternalVisibleTabCaptureError;
    return NULL;
  }

  if (!extension()->permissions_data()->CanCaptureVisiblePage(
          SessionID::IdForTab(contents), &error_)) {
    return NULL;
  }
  return contents;
}

void TabsCaptureVisibleTabFunction::OnCaptureFailure(FailureReason reason) {
  error_ = keys::kInternalVisibleTabCaptureError;
  SendResponse(false);
}

void TabsCaptureVisibleTabFunction::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kDisableScreenshots,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool TabsDetectLanguageFunction::RunAsync() {
  scoped_ptr<tabs::DetectLanguage::Params> params(
      tabs::DetectLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = 0;
  Browser* browser = NULL;
  WebContents* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (params->tab_id.get()) {
    tab_id = *params->tab_id;
    if (!GetTabById(tab_id,
                    GetProfile(),
                    include_incognito(),
                    &browser,
                    NULL,
                    &contents,
                    NULL,
                    &error_)) {
      return false;
    }
    if (!browser || !contents)
      return false;
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      return false;
    contents = browser->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return false;
  }

  if (contents->GetController().NeedsReload()) {
    // If the tab hasn't been loaded, don't wait for the tab to load.
    error_ = keys::kCannotDetermineLanguageOfUnloadedTab;
    return false;
  }

  AddRef();  // Balanced in GotLanguage().

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!chrome_translate_client->GetLanguageState()
           .original_language()
           .empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &TabsDetectLanguageFunction::GotLanguage,
            this,
            chrome_translate_client->GetLanguageState().original_language()));
    return true;
  }
  // The tab contents does not know its language yet.  Let's wait until it
  // receives it, or until the tab is closed/navigates to some other page.
  registrar_.Add(this, chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                 content::Source<WebContents>(contents));
  registrar_.Add(
      this, chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&(contents->GetController())));
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&(contents->GetController())));
  return true;
}

void TabsDetectLanguageFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  std::string language;
  if (type == chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED) {
    const translate::LanguageDetectionDetails* lang_det_details =
        content::Details<const translate::LanguageDetectionDetails>(details)
            .ptr();
    language = lang_det_details->adopted_language;
  }

  registrar_.RemoveAll();

  // Call GotLanguage in all cases as we want to guarantee the callback is
  // called for every API call the extension made.
  GotLanguage(language);
}

void TabsDetectLanguageFunction::GotLanguage(const std::string& language) {
  SetResult(new base::StringValue(language.c_str()));
  SendResponse(true);

  Release();  // Balanced in Run()
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : execute_tab_id_(-1) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

bool ExecuteCodeInTabFunction::HasPermission() {
  if (Init() &&
      extension_->permissions_data()->HasAPIPermissionForTab(
          execute_tab_id_, APIPermission::kTab)) {
    return true;
  }
  return ExtensionFunction::HasPermission();
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage() {
  content::WebContents* contents = NULL;

  // If |tab_id| is specified, look for the tab. Otherwise default to selected
  // tab in the current window.
  CHECK_GE(execute_tab_id_, 0);
  if (!GetTabById(execute_tab_id_,
                  GetProfile(),
                  include_incognito(),
                  NULL,
                  NULL,
                  &contents,
                  NULL,
                  &error_)) {
    return false;
  }

  CHECK(contents);

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  content::RenderProcessHost* process = contents->GetRenderProcessHost();
  if (!extension()->permissions_data()->CanAccessPage(
          extension(),
          contents->GetURL(),
          contents->GetURL(),
          execute_tab_id_,
          process ? process->GetID() : -1,
          &error_)) {
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor() {
  Browser* browser = NULL;
  content::WebContents* contents = NULL;

  bool success = GetTabById(execute_tab_id_,
                            GetProfile(),
                            include_incognito(),
                            &browser,
                            NULL,
                            &contents,
                            NULL,
                            &error_) &&
                 contents && browser;

  if (!success)
    return NULL;

  return TabHelper::FromWebContents(contents)->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  return false;
}

void TabsExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  ExecuteCodeInTabFunction::OnExecuteCodeFinished(error, on_url, result);
}

bool ExecuteCodeInTabFunction::Init() {
  if (details_.get())
    return true;

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (args_->GetInteger(0, &tab_id))
    EXTENSION_FUNCTION_VALIDATE(tab_id >= 0);

  // |details| are not optional.
  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  // If the tab ID wasn't given then it needs to be converted to the
  // currently active tab's ID.
  if (tab_id == -1) {
    Browser* browser = GetCurrentBrowser();
    if (!browser)
      return false;
    content::WebContents* web_contents = NULL;
    if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id))
      return false;
  }

  execute_tab_id_ = tab_id;
  details_ = details.Pass();
  return true;
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

content::WebContents* ZoomAPIFunction::GetWebContents(int tab_id) {
  content::WebContents* web_contents = NULL;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    GetTabById(tab_id,
               GetProfile(),
               include_incognito(),
               NULL /* ignore Browser* output */,
               NULL /* ignore TabStripModel* output */,
               &web_contents,
               NULL /* ignore int tab_index output */,
               &error_);
  } else {
    Browser* browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
    else if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, NULL))
      error_ = keys::kNoSelectedTabError;
  }
  return web_contents;
}

bool TabsSetZoomFunction::RunAsync() {
  scoped_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (PermissionsData::IsRestrictedUrl(url, url, extension(), &error_))
    return false;

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level = content::ZoomFactorToZoomLevel(params->zoom_factor);

  if (!zoom_controller->SetZoomLevelByExtension(zoom_level, extension())) {
    // Tried to zoom a tab in disabled mode.
    error_ = keys::kCannotZoomDisabledTabError;
    return false;
  }

  SendResponse(true);
  return true;
}

bool TabsGetZoomFunction::RunAsync() {
  scoped_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  double zoom_level =
      ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);
  results_ = tabs::GetZoom::Results::Create(zoom_factor);
  SendResponse(true);
  return true;
}

bool TabsSetZoomSettingsFunction::RunAsync() {
  using api::tabs::ZoomSettings;

  scoped_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (PermissionsData::IsRestrictedUrl(url, url, extension(), &error_))
    return false;

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == ZoomSettings::SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != ZoomSettings::MODE_AUTOMATIC &&
      params->zoom_settings.mode != ZoomSettings::MODE_NONE) {
    error_ = keys::kPerOriginOnlyInAutomaticError;
    return false;
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case ZoomSettings::MODE_NONE:
    case ZoomSettings::MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case ZoomSettings::SCOPE_NONE:
        case ZoomSettings::SCOPE_PER_ORIGIN:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case ZoomSettings::SCOPE_PER_TAB:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case ZoomSettings::MODE_MANUAL:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case ZoomSettings::MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  SendResponse(true);
  return true;
}

bool TabsGetZoomSettingsFunction::RunAsync() {
  scoped_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);

  results_ = api::tabs::GetZoomSettings::Results::Create(zoom_settings);
  SendResponse(true);
  return true;
}

}  // namespace extensions
