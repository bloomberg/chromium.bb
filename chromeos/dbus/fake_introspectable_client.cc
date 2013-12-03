// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_introspectable_client.h"

#include "base/callback.h"
#include "base/logging.h"
#include "dbus/object_path.h"

namespace chromeos {

FakeIntrospectableClient::FakeIntrospectableClient() {}

FakeIntrospectableClient::~FakeIntrospectableClient() {}

void FakeIntrospectableClient::Init(dbus::Bus* bus) {}

void FakeIntrospectableClient::Introspect(const std::string& service_name,
                                          const dbus::ObjectPath& object_path,
                                          const IntrospectCallback& callback) {
  VLOG(1) << "Introspect: " << service_name << " " << object_path.value();
  callback.Run(service_name, object_path, "", false);
}

}  // namespace chromeos
