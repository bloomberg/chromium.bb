// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"

namespace chromeos {
class MockIBusInputContextClient : public IBusInputContextClient {
 public:
  MockIBusInputContextClient();
  virtual ~MockIBusInputContextClient();

  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void ResetObjectProxy() OVERRIDE;
  virtual bool IsObjectProxyReady() const OVERRIDE;
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

  // Call count of Initialize().
  int initialize_call_count() const { return initialize_call_count_; }

  // Call count of ResetObjectProxy().
  int reset_object_proxy_call_caount() const {
    return reset_object_proxy_call_caount_;
  }

  // Call count of SetCapabilities().
  int set_capabilities_call_count() const {
    return set_capabilities_call_count_;
  }

  // Call count of FocusIn().
  int focus_in_call_count() const { return focus_in_call_count_; }

  // Call count of FocusOut().
  int focus_out_call_count() const { return focus_out_call_count_; }

  // Call count of Reset().
  int reset_call_count() const { return reset_call_count_; }

  // Call count of SetCursorLocation().
  int set_cursor_location_call_count() const {
    return set_cursor_location_call_count_;
  }

  // Call count of ProcessKeyEvent().
  int process_key_event_call_count() const {
    return process_key_event_call_count_;
  }

 private:
  int initialize_call_count_;
  bool is_initialized_;
  int reset_object_proxy_call_caount_;
  int set_capabilities_call_count_;
  int focus_in_call_count_;
  int focus_out_call_count_;
  int reset_call_count_;
  int set_cursor_location_call_count_;
  int process_key_event_call_count_;

  DISALLOW_COPY_AND_ASSIGN(MockIBusInputContextClient);
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
