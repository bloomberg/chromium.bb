// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MOCK_OBJECT_PROXY_H_
#define DBUS_MOCK_OBJECT_PROXY_H_
#pragma once

#include <string>

#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

// Mock for ObjectProxy.
class MockObjectProxy : public ObjectProxy {
 public:
  MockObjectProxy(Bus* bus,
                  const std::string& service_name,
                  const std::string& object_path);
  virtual ~MockObjectProxy();

  MOCK_METHOD2(CallMethodAndBlock, Response*(MethodCall* method_call,
                                             int timeout_ms));
  MOCK_METHOD3(CallMethod, void(MethodCall* method_call,
                                int timeout_ms,
                                ResponseCallback callback));
  MOCK_METHOD4(ConnectToSignal,
               void(const std::string& interface_name,
                    const std::string& signal_name,
                    SignalCallback signal_callback,
                    OnConnectedCallback on_connected_callback));
  MOCK_METHOD0(Detach, void());
};

}  // namespace dbus

#endif  // DBUS_MOCK_OBJECT_PROXY_H_
