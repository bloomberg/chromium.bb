// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_

#include "chromeos/dbus/ibus/ibus_input_context_client.h"

namespace chromeos {
class MockIBusInputContextClient : public IBusInputContextClient {
 public:
  MockIBusInputContextClient();
  virtual ~MockIBusInputContextClient();

  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void ResetObjectProxy() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual void SetCommitTextHandler(
      const CommitTextHandler& commit_text_handler) OVERRIDE;
  virtual void SetForwardKeyEventHandler(
      const ForwardKeyEventHandler& forward_key_event_handler) OVERRIDE;
  virtual void SetUpdatePreeditTextHandler(
      const UpdatePreeditTextHandler& update_preedit_text_handler) OVERRIDE;
  virtual void SetShowPreeditTextHandler(
      const ShowPreeditTextHandler& show_preedit_text_handler) OVERRIDE;
  virtual void SetHidePreeditTextHandler(
      const HidePreeditTextHandler& hide_preedit_text_handler) OVERRIDE;
  virtual void UnsetCommitTextHandler() OVERRIDE;
  virtual void UnsetForwardKeyEventHandler() OVERRIDE;
  virtual void UnsetUpdatePreeditTextHandler() OVERRIDE;
  virtual void UnsetShowPreeditTextHandler() OVERRIDE;
  virtual void UnsetHidePreeditTextHandler() OVERRIDE;
  virtual void SetCapabilities(uint32 capabilities) OVERRIDE;
  virtual void FocusIn() OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void SetCursorLocation(int32 x, int32 y, int32 w, int32 h) OVERRIDE;
  virtual void ProcessKeyEvent(uint32 keyval, uint32 keycode, uint32 state,
                       const ProcessKeyEventCallback& callback) OVERRIDE;
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
