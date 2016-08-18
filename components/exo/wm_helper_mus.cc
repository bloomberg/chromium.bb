// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_mus.h"

#include "ash/common/display/display_info.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, public:

WMHelperMus::WMHelperMus() {}

WMHelperMus::~WMHelperMus() {}

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, private:

const ash::DisplayInfo WMHelperMus::GetDisplayInfo(int64_t display_id) const {
  NOTIMPLEMENTED();
  return ash::DisplayInfo();
}

aura::Window* WMHelperMus::GetContainer(int container_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperMus::GetActiveWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperMus::GetFocusedWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

ui::CursorSetType WMHelperMus::GetCursorSet() const {
  NOTIMPLEMENTED();
  return ui::CursorSetType::CURSOR_SET_NORMAL;
}

void WMHelperMus::AddPreTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void WMHelperMus::PrependPreTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void WMHelperMus::RemovePreTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void WMHelperMus::AddPostTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void WMHelperMus::RemovePostTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

bool WMHelperMus::IsMaximizeModeWindowManagerEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace exo
