// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "content/common/bluetooth/bluetooth_error.h"
#include "content/public/browser/browser_message_filter.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace content {

// Dispatches and sends bluetooth related messages sent to/from a child
// process BluetoothDispatcher from/to the main browser process.
// Constructed on the main (UI) thread and receives IPC on the IO thread.
// Intended to be instantiated by the RenderProcessHost and installed as
// a filter on the channel. BrowserMessageFilter is refcounted and typically
// lives as long as it is installed on a channel.
class BluetoothDispatcherHost : public BrowserMessageFilter,
                                public device::BluetoothAdapter::Observer {
 public:
  // Creates a BluetoothDispatcherHost.
  static scoped_refptr<BluetoothDispatcherHost> Create();

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  BluetoothDispatcherHost();
  ~BluetoothDispatcherHost() override;

 private:
  // Set |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void set_adapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDevice(int thread_id, int request_id);
  void OnSetBluetoothMockDataSetForTesting(const std::string& name);

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  enum class MockData { NOT_MOCKING, REJECT, RESOLVE };
  MockData bluetooth_mock_data_set_;
  BluetoothError bluetooth_request_device_reject_type_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DISPATCHER_HOST_H_
