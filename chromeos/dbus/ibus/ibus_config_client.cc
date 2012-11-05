// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_config_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_component.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// The IBusConfigClient implementation.
class IBusConfigClientImpl : public IBusConfigClient {
 public:
  explicit IBusConfigClientImpl(dbus::Bus* bus) {
  }

  virtual ~IBusConfigClientImpl() {}

  // IBusConfigClient override.
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE {
  }

  // IBusConfigClient override.
  virtual void SetIntValue(const std::string& section,
                           const std::string& key,
                           int value,
                           const ErrorCallback& error_callback) OVERRIDE {
  }

  // IBusConfigClient override.
  virtual void SetBoolValue(const std::string& section,
                            const std::string& key,
                            bool value,
                            const ErrorCallback& error_callback) OVERRIDE {
  }

  // IBusConfigClient override.
  virtual void SetStringListValue(
      const std::string& section,
      const std::string& key,
      const std::vector<std::string>& value,
      const ErrorCallback& error_callback) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusConfigClientImpl);
};

// A stub implementation of IBusConfigClient.
class IBusConfigClientStubImpl : public IBusConfigClient {
 public:
  IBusConfigClientStubImpl() {}
  virtual ~IBusConfigClientStubImpl() {}
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetIntValue(const std::string& section,
                           const std::string& key,
                           int value,
                           const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetBoolValue(const std::string& section,
                            const std::string& key,
                            bool value,
                            const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetStringListValue(
      const std::string& section,
      const std::string& key,
      const std::vector<std::string>& value,
      const ErrorCallback& error_callback) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusConfigClientStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusConfigClient

IBusConfigClient::IBusConfigClient() {}

IBusConfigClient::~IBusConfigClient() {}

// static
IBusConfigClient* IBusConfigClient::Create(DBusClientImplementationType type,
                                           dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusConfigClientImpl(bus);
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new IBusConfigClientStubImpl();
}

}  // namespace chromeos
