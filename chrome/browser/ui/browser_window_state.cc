// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_state.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace chrome {
namespace {

// Parse two comma-separated integers from str. Return true on success.
bool ParseCommaSeparatedIntegers(const std::string& str,
                                 int* ret_num1,
                                 int* ret_num2) {
  size_t num1_size = str.find_first_of(',');
  if (num1_size == std::string::npos)
    return false;

  size_t num2_pos = num1_size + 1;
  size_t num2_size = str.size() - num2_pos;
  int num1, num2;
  if (!base::StringToInt(str.substr(0, num1_size), &num1) ||
      !base::StringToInt(str.substr(num2_pos, num2_size), &num2))
    return false;

  *ret_num1 = num1;
  *ret_num2 = num2;
  return true;
}

}  // namespace

std::string GetWindowPlacementKey(const Browser* browser) {
  std::string name(prefs::kBrowserWindowPlacement);
  if (!browser->app_name().empty()) {
    name.append("_");
    name.append(browser->app_name());
  }
  return name;
}

bool ShouldSaveWindowPlacement(const Browser* browser) {
  switch (browser->type()) {
  case Browser::TYPE_TABBED:
    return true;
  case Browser::TYPE_POPUP:
    // Only save the window placement of popups if they are restored,
    // or the window belongs to DevTools or an App.
    return browser_defaults::kRestorePopups || browser->is_devtools() ||
        browser->is_app();
  case Browser::TYPE_PANEL:
    // Do not save the window placement of panels.
    return false;
  default:
    return false;
  }
}

void SaveWindowPlacement(const Browser* browser,
                         const gfx::Rect& bounds,
                         ui::WindowShowState show_state) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->SetWindowBounds(browser->session_id(), bounds, show_state);
}

gfx::Rect GetSavedWindowBounds(const Browser* browser) {
  gfx::Rect restored_bounds = browser->override_bounds();
  WindowSizer::GetBrowserWindowBounds(browser->app_name(),
                                      restored_bounds,
                                      browser,
                                      &restored_bounds);

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = parsed_command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = parsed_command_line.HasSwitch(switches::kPlaybackMode);
  if (record_mode || playback_mode) {
    // In playback/record mode we always fix the size of the browser and
    // move it to (0,0).  The reason for this is two reasons:  First we want
    // resize/moves in the playback to still work, and Second we want
    // playbacks to work (as much as possible) on machines w/ different
    // screen sizes.
    restored_bounds = gfx::Rect(0, 0, 800, 600);
  }

  // The following options override playback/record.
  if (parsed_command_line.HasSwitch(switches::kWindowSize)) {
    std::string str =
        parsed_command_line.GetSwitchValueASCII(switches::kWindowSize);
    int width, height;
    if (ParseCommaSeparatedIntegers(str, &width, &height))
      restored_bounds.set_size(gfx::Size(width, height));
  }
  if (parsed_command_line.HasSwitch(switches::kWindowPosition)) {
    std::string str =
        parsed_command_line.GetSwitchValueASCII(switches::kWindowPosition);
    int x, y;
    if (ParseCommaSeparatedIntegers(str, &x, &y))
      restored_bounds.set_origin(gfx::Point(x, y));
  }

  return restored_bounds;
}

ui::WindowShowState GetSavedWindowShowState(const Browser* browser) {
  // Only tabbed browsers use the command line or preference state, with the
  // exception of devtools.
  bool show_state = !browser->is_type_tabbed() && !browser->is_devtools();

#if defined(USE_AURA)
  // Apps save state on aura.
  show_state &= !browser->is_app();
#endif

  if (show_state)
    return browser->initial_show_state();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kStartMaximized))
    return ui::SHOW_STATE_MAXIMIZED;

  if (browser->initial_show_state() != ui::SHOW_STATE_DEFAULT)
    return browser->initial_show_state();

  const DictionaryValue* window_pref = browser->profile()->GetPrefs()->
      GetDictionary(GetWindowPlacementKey(browser).c_str());
  bool maximized = false;
  window_pref->GetBoolean("maximized", &maximized);

  return maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_DEFAULT;
}

}  // namespace chrome
