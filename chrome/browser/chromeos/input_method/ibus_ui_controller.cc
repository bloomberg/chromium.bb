// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Remvoe IBusUiController

#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"

#include <sstream>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method_ibus.h"

namespace chromeos {
namespace input_method {
namespace {

// Returns pointer of IBusPanelService. This function returns NULL if it is not
// ready.
ibus::IBusPanelService* GetIBusPanelService() {
  return DBusThreadManager::Get()->GetIBusPanelService();
}

// Returns a ui::InputMethodIBus object which is associated with the root
// window. Returns NULL if the Ash shell has already been destructed.
static ui::InputMethodIBus* GetChromeInputMethod() {
  if (!ash::Shell::HasInstance())
    return NULL;
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  if (!root_window)
    return NULL;
  return static_cast<ui::InputMethodIBus*>(root_window->GetProperty(
      aura::client::kRootWindowInputMethodKey));
}

}  // namespace

// A class for customizing the behavior of ui::InputMethodIBus for Chrome OS.
class IBusChromeOSClientImpl : public ui::internal::IBusClient {
 public:
  explicit IBusChromeOSClientImpl(IBusUiController* ui) : ui_(ui) {}

  // ui::IBusClient override.
  virtual InputMethodType GetInputMethodType() OVERRIDE {
    InputMethodManager* manager = GetInputMethodManager();
    DCHECK(manager);
    return InputMethodUtil::IsKeyboardLayout(
        manager->GetCurrentInputMethod().id()) ?
            INPUT_METHOD_XKB_LAYOUT : INPUT_METHOD_NORMAL;
  }

  virtual void SetCursorLocation(const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head) OVERRIDE {
    if (!ui_)
      return;
    // We don't have to call ibus_input_context_set_cursor_location() on
    // Chrome OS because the candidate window for IBus is integrated with
    // Chrome.
    ui_->SetCursorLocation(cursor_location, composition_head);
  }

  void set_ui(IBusUiController* ui) {
    ui_ = ui;
  }

 private:
  IBusUiController* ui_;
  DISALLOW_COPY_AND_ASSIGN(IBusChromeOSClientImpl);
};

// The real implementation of the IBusUiController.
IBusUiController::IBusUiController() {
  ui::InputMethodIBus* input_method = GetChromeInputMethod();
  DCHECK(input_method);
  input_method->set_ibus_client(scoped_ptr<ui::internal::IBusClient>(
      new IBusChromeOSClientImpl(this)).Pass());
}

IBusUiController::~IBusUiController() {
  ui::InputMethodIBus* input_method = GetChromeInputMethod();
  if (input_method) {
    ui::internal::IBusClient* client = input_method->ibus_client();
    // We assume that no objects other than |this| set an IBus client.
    DCHECK(client);
    static_cast<IBusChromeOSClientImpl*>(client)->set_ui(NULL);
  }
}

void IBusUiController::NotifyCandidateClicked(int index, int button,
                                              int flags) {
  ibus::IBusPanelService* service = GetIBusPanelService();
  if (service) {
    service->CandidateClicked(
        index,
        static_cast<ibus::IBusMouseButton>(button),
        flags);
  }
}

void IBusUiController::NotifyCursorUp() {
  ibus::IBusPanelService* service = GetIBusPanelService();
  if (service)
    service->CursorUp();
}

void IBusUiController::NotifyCursorDown() {
  ibus::IBusPanelService* service = GetIBusPanelService();
  if (service)
    service->CursorDown();
}

void IBusUiController::NotifyPageUp() {
  ibus::IBusPanelService* service = GetIBusPanelService();
  if (service)
    service->PageUp();
}

void IBusUiController::NotifyPageDown() {
  ibus::IBusPanelService* service = GetIBusPanelService();
  if (service)
    service->PageDown();
}

void IBusUiController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IBusUiController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IBusUiController::HideAuxiliaryText() {
  FOR_EACH_OBSERVER(Observer, observers_, OnHideAuxiliaryText());
}

void IBusUiController::HideLookupTable() {
  FOR_EACH_OBSERVER(Observer, observers_, OnHideLookupTable());
}

void IBusUiController::UpdateAuxiliaryText(const std::string& text,
                         bool visible) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateAuxiliaryText(text, visible));
}

void IBusUiController::SetCursorLocation(const gfx::Rect& cursor_location,
                                         const gfx::Rect& composition_head) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnSetCursorLocation(cursor_location, composition_head));
}

void IBusUiController::UpdatePreeditText(const std::string& text,
                                         uint32 cursor_pos,
                                         bool visible) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdatePreeditText(text, cursor_pos, visible));
}

void IBusUiController::HidePreeditText() {
  FOR_EACH_OBSERVER(Observer, observers_, OnHidePreeditText());
}

void IBusUiController::UpdateLookupTable(const ibus::IBusLookupTable& table,
                                         bool visible) {
  FOR_EACH_OBSERVER(Observer, observers_, OnUpdateLookupTable(table, visible));
}

}  // namespace input_method
}  // namespace chromeos
