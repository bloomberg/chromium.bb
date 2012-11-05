// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_panel_service.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class IBusPanelServiceImpl : public IBusPanelService {
 public:
  explicit IBusPanelServiceImpl(dbus::Bus* bus) {
  }

  virtual ~IBusPanelServiceImpl() {
  }

  // IBusPanelService override.
  virtual void Initialize(IBusPanelHandlerInterface* handler) OVERRIDE {
  }

  // IBusPanelService override.
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE {
  }

  // IBusPanelService override.
  virtual void CursorUp() OVERRIDE {
  }

  // IBusPanelService override.
  virtual void CursorDown() OVERRIDE {
  }

  // IBusPanelService override.
  virtual void PageUp() OVERRIDE {
  }

  // IBusPanelService override.
  virtual void PageDown() OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(IBusPanelServiceImpl);
};

class IBusPanelServiceStubImpl : public IBusPanelService {
 public:
  IBusPanelServiceStubImpl() {}
  virtual ~IBusPanelServiceStubImpl() {}
  // IBusPanelService overrides.
  virtual void Initialize(IBusPanelHandlerInterface* handler) OVERRIDE {}
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE {}
  virtual void CursorUp() OVERRIDE {}
  virtual void CursorDown() OVERRIDE {}
  virtual void PageUp() OVERRIDE {}
  virtual void PageDown() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusPanelServiceStubImpl);
};

IBusPanelService::IBusPanelService() {
}

IBusPanelService::~IBusPanelService() {
}

// static
IBusPanelService* IBusPanelService::Create(DBusClientImplementationType type,
                                           dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusPanelServiceImpl(bus);
  } else {
    return new IBusPanelServiceStubImpl();
  }
}

}  // namespace chromeos
