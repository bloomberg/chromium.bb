// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
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
