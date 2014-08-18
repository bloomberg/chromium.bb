// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/screen_accelerator_handler.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/debug_utils.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/public/activation_client.h"

namespace athena {
namespace {

enum Command {
  CMD_PRINT_LAYER_HIERARCHY,
  CMD_PRINT_WINDOW_HIERARCHY,
  CMD_ROTATE_SCREEN,
};

const int EF_ALL_DOWN =
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;

const AcceleratorData accelerator_data[] = {
    {TRIGGER_ON_PRESS, ui::VKEY_L, EF_ALL_DOWN, CMD_PRINT_LAYER_HIERARCHY,
     AF_DEBUG},
    {TRIGGER_ON_PRESS, ui::VKEY_W, EF_ALL_DOWN, CMD_PRINT_WINDOW_HIERARCHY,
     AF_DEBUG},
    {TRIGGER_ON_PRESS, ui::VKEY_F3,
     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     CMD_ROTATE_SCREEN, AF_NONE},
};

void PrintLayerHierarchy(aura::Window* root_window) {
  ui::PrintLayerHierarchy(
      root_window->layer(),
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
}

void PrintWindowHierarchy(aura::Window* window,
                          aura::Window* active,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->name());
  if (name.empty())
    name = "\"\"";
  *out << indent_str << name << " (" << window << ")"
       << " type=" << window->type()
       << ((window == active) ? " [active] " : " ")
       << (window->IsVisible() ? " visible " : " ")
       << window->bounds().ToString() << '\n';

  for (size_t i = 0; i < window->children().size(); ++i)
    PrintWindowHierarchy(window->children()[i], active, indent + 3, out);
}

void HandlePrintWindowHierarchy(aura::Window* root_window) {
  aura::Window* active =
      aura::client::GetActivationClient(root_window)->GetActiveWindow();
  std::ostringstream out;
  out << "RootWindow :\n";
  PrintWindowHierarchy(root_window, active, 0, &out);
  // Error so logs can be collected from end-users.
  LOG(ERROR) << out.str();
}

void HandleRotateScreen() {
  ScreenManager* screen_manager = ScreenManager::Get();
  gfx::Display::Rotation current_rotation =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation();
  if (current_rotation == gfx::Display::ROTATE_0)
    screen_manager->SetRotation(gfx::Display::ROTATE_90);
  else if (current_rotation == gfx::Display::ROTATE_90)
    screen_manager->SetRotation(gfx::Display::ROTATE_180);
  else if (current_rotation == gfx::Display::ROTATE_180)
    screen_manager->SetRotation(gfx::Display::ROTATE_270);
  else if (current_rotation == gfx::Display::ROTATE_270)
    screen_manager->SetRotation(gfx::Display::ROTATE_0);
}

}  // namespace

// static
ScreenAcceleratorHandler::ScreenAcceleratorHandler(aura::Window* root_window)
    : root_window_(root_window) {
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

ScreenAcceleratorHandler::~ScreenAcceleratorHandler() {
}

bool ScreenAcceleratorHandler::IsCommandEnabled(int command_id) const {
  return true;
}

bool ScreenAcceleratorHandler::OnAcceleratorFired(
    int command_id,
    const ui::Accelerator& accelerator) {
  switch (command_id) {
    case CMD_PRINT_LAYER_HIERARCHY:
      PrintLayerHierarchy(root_window_);
      return true;
    case CMD_PRINT_WINDOW_HIERARCHY:
      HandlePrintWindowHierarchy(root_window_);
      return true;
    case CMD_ROTATE_SCREEN:
      HandleRotateScreen();
      return true;
  }
  return false;
}

}  // namesapce athena
