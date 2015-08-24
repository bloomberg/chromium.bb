// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/windows.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

///////////////////////////////////////////////////////////////////////////////
// WindowController

// static
WindowController::TypeFilter WindowController::GetAllWindowFilter() {
  // This needs to be updated if there is a change to
  // extensions::api::windows:WindowType.
  COMPILE_ASSERT(api::windows::WINDOW_TYPE_LAST == 5,
                 Update_extensions_WindowController_to_match_WindowType);
  return ((1 << api::windows::WINDOW_TYPE_NORMAL) |
          (1 << api::windows::WINDOW_TYPE_PANEL) |
          (1 << api::windows::WINDOW_TYPE_POPUP) |
          (1 << api::windows::WINDOW_TYPE_APP) |
          (1 << api::windows::WINDOW_TYPE_DEVTOOLS));
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypes(
    const std::vector<api::windows::WindowType>& types) {
  WindowController::TypeFilter filter = kNoWindowFilter;
  for (auto& window_type : types)
    filter |= 1 << window_type;
  return filter;
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypesValues(
    const base::ListValue* types) {
  WindowController::TypeFilter filter = WindowController::kNoWindowFilter;
  if (!types)
    return filter;
  for (size_t i = 0; i < types->GetSize(); i++) {
    std::string window_type;
    if (types->GetString(i, &window_type))
      filter |= 1 << api::windows::ParseWindowType(window_type);
  }
  return filter;
}

WindowController::WindowController(ui::BaseWindow* window, Profile* profile)
    : window_(window), profile_(profile) {
}

WindowController::~WindowController() {
}

Browser* WindowController::GetBrowser() const {
  return NULL;
}

namespace keys = tabs_constants;

base::DictionaryValue* WindowController::CreateWindowValue() const {
  base::DictionaryValue* result = new base::DictionaryValue();

  result->SetInteger(keys::kIdKey, GetWindowId());
  result->SetString(keys::kWindowTypeKey, GetWindowTypeText());
  result->SetBoolean(keys::kFocusedKey, window()->IsActive());
  result->SetBoolean(keys::kIncognitoKey, profile_->IsOffTheRecord());
  result->SetBoolean(keys::kAlwaysOnTopKey, window()->IsAlwaysOnTop());

  std::string window_state;
  if (window()->IsMinimized()) {
    window_state = keys::kShowStateValueMinimized;
  } else if (window()->IsFullscreen()) {
    window_state = keys::kShowStateValueFullscreen;
  } else if (window()->IsMaximized()) {
    window_state = keys::kShowStateValueMaximized;
  } else {
    window_state = keys::kShowStateValueNormal;
  }
  result->SetString(keys::kShowStateKey, window_state);

  gfx::Rect bounds;
  if (window()->IsMinimized())
    bounds = window()->GetRestoredBounds();
  else
    bounds = window()->GetBounds();
  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());

  return result;
}

bool WindowController::MatchesFilter(TypeFilter filter) const {
  TypeFilter type = 1 << api::windows::ParseWindowType(GetWindowTypeText());
  return (type & filter) != 0;
}

}  // namespace extensions
