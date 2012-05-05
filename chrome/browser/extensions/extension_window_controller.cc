// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_window_controller.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_window_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/base_window.h"
#include "ui/gfx/rect.h"

///////////////////////////////////////////////////////////////////////////////
// ExtensionWindowController

ExtensionWindowController::ExtensionWindowController(BaseWindow* window,
                                                     Profile* profile) :
    window_(window),
    profile_(profile) {
  ExtensionWindowList::GetInstance()->AddExtensionWindow(this);
}

ExtensionWindowController::~ExtensionWindowController() {
  ExtensionWindowList::GetInstance()->RemoveExtensionWindow(this);
}

bool ExtensionWindowController::MatchesProfile(
    Profile* match_profile,
    ProfileMatchType match_type) const {
  return ((profile_ == match_profile) ||
          (match_type == MATCH_INCOGNITO &&
           (match_profile->HasOffTheRecordProfile() &&
            match_profile->GetOffTheRecordProfile() == profile_)));
}

Browser* ExtensionWindowController::GetBrowser() const {
  return NULL;
}

namespace keys = extension_tabs_module_constants;

base::DictionaryValue* ExtensionWindowController::CreateWindowValue() const {
  DictionaryValue* result = new DictionaryValue();

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
