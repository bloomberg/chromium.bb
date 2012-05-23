// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_

#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
class MockIBusInputContextClient : public IBusInputContextClient {
 public:
  MockIBusInputContextClient();
  virtual ~MockIBusInputContextClient();

  MOCK_METHOD2(Initialize, void(dbus::Bus* bus,
                                const dbus::ObjectPath& object_path));
  MOCK_METHOD0(ResetObjectProxy, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD1(SetCommitTextHandler,
               void(const CommitTextHandler& commit_text_handler));
  MOCK_METHOD1(SetForwardKeyEventHandler,
               void(const ForwardKeyEventHandler& forward_key_event_handler));
  MOCK_METHOD1(
      SetUpdatePreeditTextHandler,
      void(const UpdatePreeditTextHandler& update_preedit_text_handler));
  MOCK_METHOD1(SetShowPreeditTextHandler,
               void(const ShowPreeditTextHandler& show_preedit_text_handler));
  MOCK_METHOD1(SetHidePreeditTextHandler,
               void(const HidePreeditTextHandler& hide_preedit_text_handler));
  MOCK_METHOD0(UnsetCommitTextHandler, void());
  MOCK_METHOD0(UnsetForwardKeyEventHandler, void());
  MOCK_METHOD0(UnsetUpdatePreeditTextHandler, void());
  MOCK_METHOD0(UnsetShowPreeditTextHandler, void());
  MOCK_METHOD0(UnsetHidePreeditTextHandler, void());
  MOCK_METHOD1(SetCapabilities, void(uint32 capabilities));
  MOCK_METHOD0(FocusIn, void());
  MOCK_METHOD0(FocusOut, void());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD4(SetCursorLocation, void(int32 x, int32 y, int32 w, int32 h));
  MOCK_METHOD4(ProcessKeyEvent, void(uint32 keyval,
                                     uint32 keycode,
                                     uint32 state,
                                     const ProcessKeyEventCallback& callback));
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_INPUT_CONTEXT_CLIENT_H_
