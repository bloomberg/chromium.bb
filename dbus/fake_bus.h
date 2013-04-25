// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_FAKE_BUS_H_
#define DBUS_FAKE_BUS_H_

#include "dbus/bus.h"

namespace dbus {

// A fake implementation of Bus, which does nothing.
class FakeBus : public Bus {
 public:
  FakeBus(const Bus::Options& options);

 protected:
  virtual ~FakeBus();
};

}  // namespace dbus

#endif  // DBUS_FAKE_BUS_H_
