// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "content/common/bluetooth/bluetooth_error.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// Dispatches and sends bluetooth related messages sent to/from a child
// process BluetoothDispatcher from/to the main browser process.
// Constructed on the main (UI) thread and receives IPC on the IO thread.
// Intended to be instantiated by the RenderProcessHost and installed as
// a filter on the channel. BrowserMessageFilter is refcounted and typically
// lives as long as it is installed on a channel.
class BluetoothDispatcherHost : public BrowserMessageFilter {
 public:
  BluetoothDispatcherHost();

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~BluetoothDispatcherHost() override;

 private:
  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDevice(int thread_id, int request_id);
  void OnSetBluetoothMockDataSetForTesting(const std::string& name);

  enum class MockData { NOT_MOCKING, REJECT, RESOLVE };
  MockData bluetooth_mock_data_set_;
  BluetoothError bluetooth_request_device_reject_type_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
