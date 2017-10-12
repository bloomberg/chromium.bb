// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_mus.h"

#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/views/mus/mus_client.h"
#include "ui/wm/public/activation_client.h"
namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, public:

WMHelperMus::WMHelperMus() {
  aura::FocusSynchronizer* focus_synchronizer =
      views::MusClient::Get()->window_tree_client()->focus_synchronizer();
  SetActiveFocusClient(focus_synchronizer->active_focus_client(),
                       focus_synchronizer->active_focus_client_root());
  focus_synchronizer->AddObserver(this);
}

WMHelperMus::~WMHelperMus() {
  if (active_focus_client_)
    active_focus_client_->RemoveObserver(this);
  views::MusClient::Get()
      ->window_tree_client()
      ->focus_synchronizer()
      ->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, private:

const display::ManagedDisplayInfo& WMHelperMus::GetDisplayInfo(
    int64_t display_id) const {
  // TODO(penghuang): Return real display info when it is supported in mus.
  static const display::ManagedDisplayInfo info;
  return info;
}

aura::Window* WMHelperMus::GetPrimaryDisplayContainer(int container_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperMus::GetActiveWindow() const {
  return active_window_;
}

aura::Window* WMHelperMus::GetFocusedWindow() const {
  return focused_window_;
}

ui::CursorSize WMHelperMus::GetCursorSize() const {
  NOTIMPLEMENTED();
  return ui::CursorSize::kNormal;
}

const display::Display& WMHelperMus::GetCursorDisplay() const {
  NOTIMPLEMENTED();
  // TODO(penghuang): Return real display when supported in mus.
  static const display::Display display;
  return display;
}

void WMHelperMus::AddPreTargetHandler(ui::EventHandler* handler) {
  aura::Env::GetInstance()->AddPreTargetHandler(handler);
}

void WMHelperMus::PrependPreTargetHandler(ui::EventHandler* handler) {
  aura::Env::GetInstance()->PrependPreTargetHandler(handler);
}

void WMHelperMus::RemovePreTargetHandler(ui::EventHandler* handler) {
  aura::Env::GetInstance()->RemovePreTargetHandler(handler);
}

void WMHelperMus::AddPostTargetHandler(ui::EventHandler* handler) {
  aura::Env::GetInstance()->AddPostTargetHandler(handler);
}

void WMHelperMus::RemovePostTargetHandler(ui::EventHandler* handler) {
  aura::Env::GetInstance()->RemovePostTargetHandler(handler);
}

bool WMHelperMus::IsTabletModeWindowManagerEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

double WMHelperMus::GetDefaultDeviceScaleFactor() const {
  NOTIMPLEMENTED();
  return 1.0;
}

bool WMHelperMus::AreVerifiedSyncTokensNeeded() const {
  return true;
}

void WMHelperMus::OnActiveFocusClientChanged(
    aura::client::FocusClient* focus_client,
    aura::Window* focus_client_root) {
  SetActiveFocusClient(focus_client, focus_client_root);
}

void WMHelperMus::OnWindowFocused(aura::Window* gained_focus,
                                  aura::Window* lost_focus) {
  if (focused_window_ == gained_focus)
    return;

  SetActiveWindow(GetActivationClient()->GetActiveWindow());
  SetFocusedWindow(gained_focus);
}

void WMHelperMus::OnKeyboardDeviceConfigurationChanged() {
  NotifyKeyboardDeviceConfigurationChanged();
}

void WMHelperMus::SetActiveFocusClient(aura::client::FocusClient* focus_client,
                                       aura::Window* window) {
  if (active_focus_client_)
    active_focus_client_->RemoveObserver(this);
  active_focus_client_ = focus_client;
  root_with_active_focus_client_ = window;
  if (active_focus_client_) {
    active_focus_client_->AddObserver(this);
    SetActiveWindow(GetActivationClient()->GetActiveWindow());
    SetFocusedWindow(active_focus_client_->GetFocusedWindow());
  } else {
    SetActiveWindow(nullptr);
    SetFocusedWindow(nullptr);
  }
}

void WMHelperMus::SetActiveWindow(aura::Window* window) {
  if (active_window_ == window)
    return;

  aura::Window* lost_active = active_window_;
  active_window_ = window;
  NotifyWindowActivated(active_window_, lost_active);
}

void WMHelperMus::SetFocusedWindow(aura::Window* window) {
  if (focused_window_ == window)
    return;

  aura::Window* lost_focus = focused_window_;
  focused_window_ = window;
  NotifyWindowFocused(focused_window_, lost_focus);
}

wm::ActivationClient* WMHelperMus::GetActivationClient() {
  return root_with_active_focus_client_
             ? wm::GetActivationClient(root_with_active_focus_client_)
             : nullptr;
}

}  // namespace exo
