// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"

namespace chromeos {

MockIBusInputContextClient::MockIBusInputContextClient()
    : initialize_call_count_(0),
      is_initialized_(false),
      reset_object_proxy_call_caount_(0),
      set_capabilities_call_count_(0),
      focus_in_call_count_(0),
      focus_out_call_count_(0),
      reset_call_count_(0),
      set_cursor_location_call_count_(0),
      process_key_event_call_count_(0),
      set_surrounding_text_call_count_(0) {
}

MockIBusInputContextClient::~MockIBusInputContextClient() {}

void MockIBusInputContextClient::Initialize(
    dbus::Bus* bus, const dbus::ObjectPath& object_path) {
  initialize_call_count_++;
  is_initialized_ = true;
}

void MockIBusInputContextClient::SetInputContextHandler(
    IBusInputContextHandlerInterface* handler) {
}

void MockIBusInputContextClient::SetSetCursorLocationHandler(
    const SetCursorLocationHandler& set_cursor_location_handler) {
}

void MockIBusInputContextClient::UnsetSetCursorLocationHandler() {
}

void MockIBusInputContextClient::ResetObjectProxy() {
  reset_object_proxy_call_caount_++;
  is_initialized_ = false;
}

bool MockIBusInputContextClient::IsObjectProxyReady() const {
  if (is_initialized_)
    return true;
  else
    return false;
}

void MockIBusInputContextClient::SetCapabilities(uint32 capabilities) {
  set_capabilities_call_count_++;
}

void MockIBusInputContextClient::FocusIn() {
  focus_in_call_count_++;
}

void MockIBusInputContextClient::FocusOut() {
  focus_out_call_count_++;
}

void MockIBusInputContextClient::Reset() {
  reset_call_count_++;
}

void MockIBusInputContextClient::SetCursorLocation(
    const ibus::Rect& cursor_location,
    const ibus::Rect& composition_head) {
  set_cursor_location_call_count_++;
}

void MockIBusInputContextClient::ProcessKeyEvent(
    uint32 keyval,
    uint32 keycode,
    uint32 state,
    const ProcessKeyEventCallback& callback,
    const ErrorCallback& error_callback) {
  process_key_event_call_count_++;
  if (!process_key_event_handler_.is_null()) {
    process_key_event_handler_.Run(keyval, keycode, state, callback,
                                   error_callback);
  }
}

void MockIBusInputContextClient::SetSurroundingText(
    const std::string& text,
    uint32 cursor_pos,
    uint32 anchor_pos) {
  set_surrounding_text_call_count_++;
  set_surrounding_text_handler_.Run(text, cursor_pos, anchor_pos);
}

void MockIBusInputContextClient::PropertyActivate(
    const std::string& key,
    ibus::IBusPropertyState state) {
}

bool MockIBusInputContextClient::IsXKBLayout() {
  return true;
}

void MockIBusInputContextClient::SetIsXKBLayout(bool is_xkb_layout) {
}

}  // namespace chromeos
