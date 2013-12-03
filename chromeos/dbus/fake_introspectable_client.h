// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_INTROSPECTABLE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_INTROSPECTABLE_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/introspectable_client.h"

namespace chromeos {

// The IntrospectableClient implementation used on Linux desktop, which does
// nothing.
class FakeIntrospectableClient: public IntrospectableClient {
 public:
  FakeIntrospectableClient();
  virtual ~FakeIntrospectableClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void Introspect(const std::string& service_name,
                          const dbus::ObjectPath& object_path,
                          const IntrospectCallback& callback) OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_INTROSPECTABLE_CLIENT_H_
