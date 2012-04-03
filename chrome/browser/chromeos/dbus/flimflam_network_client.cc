// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/flimflam_network_client.h"

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The FlimflamNetworkClient implementation.
class FlimflamNetworkClientImpl : public FlimflamNetworkClient {
 public:
  explicit FlimflamNetworkClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
          flimflam::kFlimflamServiceName,
          dbus::ObjectPath(flimflam::kFlimflamServicePath))),
        helper_(proxy_) {
    helper_.MonitorPropertyChanged(flimflam::kFlimflamNetworkInterface);
  }

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {
    helper_.SetPropertyChangedHandler(handler);
  }

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {
    helper_.ResetPropertyChangedHandler();
  }

  // FlimflamNetworkClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    helper_.CallDictionaryValueMethod(&method_call, callback);
  }

 private:
  dbus::ObjectProxy* proxy_;
  FlimflamClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientImpl);
};

// A stub implementation of FlimflamNetworkClient.
class FlimflamNetworkClientStubImpl : public FlimflamNetworkClient {
 public:
  FlimflamNetworkClientStubImpl() {}

  virtual ~FlimflamNetworkClientStubImpl() {}

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientStubImpl);
};

}  // namespace

FlimflamNetworkClient::FlimflamNetworkClient() {}

FlimflamNetworkClient::~FlimflamNetworkClient() {}

// static
FlimflamNetworkClient* FlimflamNetworkClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new FlimflamNetworkClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FlimflamNetworkClientStubImpl();
}

}  // namespace chromeos
