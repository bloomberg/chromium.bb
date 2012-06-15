// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"

namespace chromeos {

MockIBusInputContextClient::MockIBusInputContextClient() {}

MockIBusInputContextClient::~MockIBusInputContextClient() {}

void MockIBusInputContextClient::Initialize(
    dbus::Bus* bus, const dbus::ObjectPath& object_path) {
}

void MockIBusInputContextClient::ResetObjectProxy() {
}

bool MockIBusInputContextClient::IsConnected() const {
  return true;
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
}

void MockIBusInputContextClient::FocusIn() {
}

void MockIBusInputContextClient::FocusOut() {
}

void MockIBusInputContextClient::Reset() {
}

void MockIBusInputContextClient::SetCursorLocation(
    int32 x, int32 y, int32 w, int32 h) {
}

void MockIBusInputContextClient::ProcessKeyEvent(
    uint32 keyval,
    uint32 keycode,
    uint32 state,
    const ProcessKeyEventCallback& callback) {
}

}  // namespace chromeos
