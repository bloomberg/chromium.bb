// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "chromeos/ime/ibus_bridge.h"

namespace chromeos {
// An implementation of IBusEngineService without ibus-daemon interaction.
// Currently this class is used only on linux desktop.
// TODO(nona): Use this on ChromeOS device once crbug.com/171351 is fixed.
class IBusEngineServiceDaemonlessImpl : public IBusEngineService {
 public:
  IBusEngineServiceDaemonlessImpl() {}
  virtual ~IBusEngineServiceDaemonlessImpl() {}

  // IBusEngineService override.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) OVERRIDE {
    IBusBridge::Get()->SetEngineHandler(handler);
  }

  // IBusEngineService override.
  virtual void UnsetEngine(IBusEngineHandlerInterface* handler) OVERRIDE {
    if (IBusBridge::Get()->GetEngineHandler() == handler)
      IBusBridge::Get()->SetEngineHandler(NULL);
  }

  // IBusEngineService override.
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) OVERRIDE {
    IBusPanelPropertyHandlerInterface* property =
        IBusBridge::Get()->GetPropertyHandler();
    if (property)
      property->RegisterProperties(property_list);
  }

  // IBusEngineService override.
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE {
    IBusInputContextHandlerInterface* input_context =
        IBusBridge::Get()->GetInputContextHandler();
    if (input_context)
      input_context->UpdatePreeditText(ibus_text, cursor_pos, is_visible);
  }

  // IBusEngineService override.
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) OVERRIDE {
    IBusPanelCandidateWindowHandlerInterface* candidate_window =
        IBusBridge::Get()->GetCandidateWindowHandler();
    if (candidate_window)
      candidate_window->UpdateAuxiliaryText(ibus_text.text(), is_visible);
  }

  // IBusEngineService override.
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE {
    IBusPanelCandidateWindowHandlerInterface* candidate_window =
        IBusBridge::Get()->GetCandidateWindowHandler();
    if (candidate_window)
      candidate_window->UpdateLookupTable(lookup_table, is_visible);
  }

  // IBusEngineService override.
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE {
    IBusPanelPropertyHandlerInterface* property_handler =
        IBusBridge::Get()->GetPropertyHandler();
    if (property_handler)
      property_handler->UpdateProperty(property);
  }

  // IBusEngineService override.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE {
    IBusInputContextHandlerInterface* input_context =
        IBusBridge::Get()->GetInputContextHandler();
    if (input_context)
      input_context->ForwardKeyEvent(keyval, keycode, state);
  }

  // IBusEngineService override.
  virtual void RequireSurroundingText() OVERRIDE {
    // Do nothing.
  }

  // IBusEngineService override.
  virtual void CommitText(const std::string& text) OVERRIDE {
    IBusInputContextHandlerInterface* input_context =
        IBusBridge::Get()->GetInputContextHandler();
    if (input_context) {
      IBusText ibus_text;
      ibus_text.set_text(text);
      input_context->CommitText(ibus_text);
    }
  }

  // IBusEngineService override.
  virtual void DeleteSurroundingText(int32 offset, uint32 length) OVERRIDE {
    IBusInputContextHandlerInterface* input_context =
        IBusBridge::Get()->GetInputContextHandler();
    if (input_context)
      input_context->DeleteSurroundingText(offset, length);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineServiceDaemonlessImpl);
};

IBusEngineService::IBusEngineService() {
}

IBusEngineService::~IBusEngineService() {
}

// static
IBusEngineService* IBusEngineService::Create() {
  return new IBusEngineServiceDaemonlessImpl();
}

}  // namespace chromeos
