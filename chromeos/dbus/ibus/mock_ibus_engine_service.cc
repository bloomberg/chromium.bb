// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"

namespace chromeos {

MockIBusEngineService::MockIBusEngineService() {
}

MockIBusEngineService::~MockIBusEngineService() {
}

void MockIBusEngineService::SetEngine(IBusEngineHandlerInterface* handler) {
}

void MockIBusEngineService::UnsetEngine() {
}

void MockIBusEngineService::RegisterProperties(
      const IBusPropertyList& property_list) {
}

void MockIBusEngineService::UpdatePreedit(const IBusText& ibus_text,
                                          uint32 cursor_pos,
                                          bool is_visible,
                                          IBusEnginePreeditFocusOutMode mode) {
}

void MockIBusEngineService::UpdateAuxiliaryText(const IBusText& ibus_text,
                                                bool is_visible) {
}

void MockIBusEngineService::UpdateLookupTable(
    const IBusLookupTable& lookup_table,
    bool is_visible) {
}

void MockIBusEngineService::UpdateProperty(const IBusProperty& property) {
}

void MockIBusEngineService::ForwardKeyEvent(uint32 keyval,
                                            uint32 keycode,
                                            uint32 state) {
}

void MockIBusEngineService::RequireSurroundingText() {
}

void MockIBusEngineService::CommitText(const std::string& text) {
}

}  // namespace chromeos
