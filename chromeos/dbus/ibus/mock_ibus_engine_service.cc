// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"

namespace chromeos {

MockIBusEngineService::MockIBusEngineService()
    : register_properties_call_count_(0),
      update_preedit_call_count_(0),
      update_auxiliary_text_call_count_(0),
      update_lookup_table_call_count_(0),
      update_property_call_count_(0),
      forward_key_event_call_count_(0),
      commit_text_call_count_(0),
      current_engine_(NULL) {
}

MockIBusEngineService::~MockIBusEngineService() {
}

void MockIBusEngineService::SetEngine(IBusEngineHandlerInterface* handler) {
  current_engine_ = handler;
}

void MockIBusEngineService::UnsetEngine(IBusEngineHandlerInterface* handler) {
  current_engine_ = NULL;
}

void MockIBusEngineService::RegisterProperties(
      const IBusPropertyList& property_list) {
  ++register_properties_call_count_;
}

void MockIBusEngineService::UpdatePreedit(const IBusText& ibus_text,
                                          uint32 cursor_pos,
                                          bool is_visible,
                                          IBusEnginePreeditFocusOutMode mode) {
  ++update_preedit_call_count_;
}

void MockIBusEngineService::UpdateAuxiliaryText(const IBusText& ibus_text,
                                                bool is_visible) {
  ++update_auxiliary_text_call_count_;
}

void MockIBusEngineService::UpdateLookupTable(
    const IBusLookupTable& lookup_table,
    bool is_visible) {
  ++update_lookup_table_call_count_;
}

void MockIBusEngineService::UpdateProperty(const IBusProperty& property) {
  ++update_property_call_count_;
}

void MockIBusEngineService::ForwardKeyEvent(uint32 keyval,
                                            uint32 keycode,
                                            uint32 state) {
  ++forward_key_event_call_count_;
}

void MockIBusEngineService::RequireSurroundingText() {
}

void MockIBusEngineService::CommitText(const std::string& text) {
  ++commit_text_call_count_;
}

IBusEngineHandlerInterface* MockIBusEngineService::GetEngine() const {
  return current_engine_;
}

}  // namespace chromeos
