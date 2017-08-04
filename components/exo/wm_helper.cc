// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace exo {
namespace {
WMHelper* g_instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper, public:

WMHelper::WMHelper() {}

WMHelper::~WMHelper() {}

// static
void WMHelper::SetInstance(WMHelper* helper) {
  DCHECK_NE(!!helper, !!g_instance);
  g_instance = helper;
}

// static
WMHelper* WMHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool WMHelper::HasInstance() {
  return !!g_instance;
}

void WMHelper::AddActivationObserver(ActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WMHelper::RemoveActivationObserver(ActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WMHelper::AddFocusObserver(FocusObserver* observer) {
  focus_observers_.AddObserver(observer);
}

void WMHelper::RemoveFocusObserver(FocusObserver* observer) {
  focus_observers_.RemoveObserver(observer);
}

void WMHelper::AddCursorObserver(CursorObserver* observer) {
  cursor_observers_.AddObserver(observer);
}

void WMHelper::RemoveCursorObserver(CursorObserver* observer) {
  cursor_observers_.RemoveObserver(observer);
}

void WMHelper::AddTabletModeObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.AddObserver(observer);
}

void WMHelper::RemoveTabletModeObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.RemoveObserver(observer);
}

void WMHelper::AddInputDeviceEventObserver(InputDeviceEventObserver* observer) {
  input_device_event_observers_.AddObserver(observer);
}

void WMHelper::RemoveInputDeviceEventObserver(
    InputDeviceEventObserver* observer) {
  input_device_event_observers_.RemoveObserver(observer);
}

void WMHelper::AddDisplayConfigurationObserver(
    DisplayConfigurationObserver* observer) {
  display_config_observers_.AddObserver(observer);
}

void WMHelper::RemoveDisplayConfigurationObserver(
    DisplayConfigurationObserver* observer) {
  display_config_observers_.RemoveObserver(observer);
}

void WMHelper::AddDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.AddObserver(observer);
}

void WMHelper::RemoveDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.RemoveObserver(observer);
}

void WMHelper::SetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, this);
}

void WMHelper::ResetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, nullptr);
}

void WMHelper::NotifyWindowActivated(aura::Window* gained_active,
                                     aura::Window* lost_active) {
  for (ActivationObserver& observer : activation_observers_)
    observer.OnWindowActivated(gained_active, lost_active);
}

void WMHelper::NotifyWindowFocused(aura::Window* gained_focus,
                                   aura::Window* lost_focus) {
  for (FocusObserver& observer : focus_observers_)
    observer.OnWindowFocused(gained_focus, lost_focus);
}

void WMHelper::NotifyCursorVisibilityChanged(bool is_visible) {
  for (CursorObserver& observer : cursor_observers_)
    observer.OnCursorVisibilityChanged(is_visible);
}

void WMHelper::NotifyCursorSizeChanged(ui::CursorSize cursor_size) {
  for (CursorObserver& observer : cursor_observers_)
    observer.OnCursorSizeChanged(cursor_size);
}

void WMHelper::NotifyCursorDisplayChanged(const display::Display& display) {
  for (CursorObserver& observer : cursor_observers_)
    observer.OnCursorDisplayChanged(display);
}

void WMHelper::NotifyTabletModeStarted() {
  for (TabletModeObserver& observer : tablet_mode_observers_)
    observer.OnTabletModeStarted();
}

void WMHelper::NotifyTabletModeEnding() {
  for (TabletModeObserver& observer : tablet_mode_observers_)
    observer.OnTabletModeEnding();
}

void WMHelper::NotifyTabletModeEnded() {
  for (TabletModeObserver& observer : tablet_mode_observers_)
    observer.OnTabletModeEnded();
}

void WMHelper::NotifyKeyboardDeviceConfigurationChanged() {
  for (InputDeviceEventObserver& observer : input_device_event_observers_)
    observer.OnKeyboardDeviceConfigurationChanged();
}

void WMHelper::NotifyDisplayConfigurationChanged() {
  for (DisplayConfigurationObserver& observer : display_config_observers_)
    observer.OnDisplayConfigurationChanged();
}

void WMHelper::OnDragEntered(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragEntered(event);
}

int WMHelper::OnDragUpdated(const ui::DropTargetEvent& event) {
  int valid_operation = ui::DragDropTypes::DRAG_NONE;
  for (DragDropObserver& observer : drag_drop_observers_)
    valid_operation = valid_operation | observer.OnDragUpdated(event);
  return valid_operation;
}

void WMHelper::OnDragExited() {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragExited();
}

int WMHelper::OnPerformDrop(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnPerformDrop(event);
  // TODO(hirono): Return the correct result instead of always returning
  // DRAG_MOVE.
  return ui::DragDropTypes::DRAG_MOVE;
}
}  // namespace exo
