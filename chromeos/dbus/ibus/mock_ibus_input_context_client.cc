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
      process_key_event_call_count_(0) {
}

MockIBusInputContextClient::~MockIBusInputContextClient() {}

void MockIBusInputContextClient::Initialize(
    dbus::Bus* bus, const dbus::ObjectPath& object_path) {
  initialize_call_count_++;
  is_initialized_ = true;
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

void MockIBusInputContextClient::SetCommitTextHandler(
    const CommitTextHandler& commit_text_handler) {
}

void MockIBusInputContextClient::SetForwardKeyEventHandler(
    const ForwardKeyEventHandler& forward_key_event_handler) {
}

void MockIBusInputContextClient::SetUpdatePreeditTextHandler(
    const UpdatePreeditTextHandler& update_preedit_text_handler) {
}

void MockIBusInputContextClient::SetShowPreeditTextHandler(
    const ShowPreeditTextHandler& show_preedit_text_handler) {
}

void MockIBusInputContextClient::SetHidePreeditTextHandler(
    const HidePreeditTextHandler& hide_preedit_text_handler) {
}

void MockIBusInputContextClient::UnsetCommitTextHandler() {
}

void MockIBusInputContextClient::UnsetForwardKeyEventHandler() {
}

void MockIBusInputContextClient::UnsetUpdatePreeditTextHandler() {
}

void MockIBusInputContextClient::UnsetShowPreeditTextHandler() {
}

void MockIBusInputContextClient::UnsetHidePreeditTextHandler() {
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
    int32 x, int32 y, int32 w, int32 h) {
  set_cursor_location_call_count_++;
}

void MockIBusInputContextClient::ProcessKeyEvent(
    uint32 keyval,
    uint32 keycode,
    uint32 state,
    const ProcessKeyEventCallback& callback) {
  process_key_event_call_count_++;
}

}  // namespace chromeos
