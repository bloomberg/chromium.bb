// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class IBusEngineServiceImpl : public IBusEngineService {
 public:
  IBusEngineServiceImpl(dbus::Bus* bus,
                        const dbus::ObjectPath& object_path)
      : bus_(bus),
        object_path_(object_path),
        weak_ptr_factory_(this) {
    exported_object_ = bus->GetExportedObject(object_path_);
    // TODO(nona): Export methods here.
  }

  virtual ~IBusEngineServiceImpl() {
    bus_->UnregisterExportedObject(object_path_);
  }

  // IBusEngineService override.
  virtual void Initialize(IBusEngineHandlerInterface* handler) OVERRIDE {
    if (engine_handler_.get() == NULL) {
      engine_handler_.reset(handler);
    } else {
      LOG(ERROR) << "Already initialized.";
    }
  }

  // IBusEngineService override.
  virtual void RegisterProperties(
      const ibus::IBusPropertyList& property_list) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void UpdatePreedit(const ibus::IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void UpdateAuxiliaryText(const ibus::IBusText& ibus_text,
                                   bool is_visible) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void UpdateLookupTable(const ibus::IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void UpdateProperty(const ibus::IBusProperty& property) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

  // IBusEngineService override.
  virtual void RequireSurroundingText() OVERRIDE {
    // TODO(nona): Implement this.
    NOTIMPLEMENTED();
  }

 private:
  // D-Bus bus object used for unregistering exported methods in dtor.
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the |engine_handler_|.
  scoped_ptr<IBusEngineHandlerInterface> engine_handler_;

  dbus::ObjectPath object_path_;
  scoped_refptr<dbus::ExportedObject> exported_object_;
  base::WeakPtrFactory<IBusEngineServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusEngineServiceImpl);
};

class IBusEngineServiceStubImpl : public IBusEngineService {
 public:
  IBusEngineServiceStubImpl() {}
  virtual ~IBusEngineServiceStubImpl() {}
  // IBusEngineService overrides.
  virtual void Initialize(IBusEngineHandlerInterface* handler) OVERRIDE {}
  virtual void RegisterProperties(
      const ibus::IBusPropertyList& property_list) OVERRIDE {}
  virtual void UpdatePreedit(const ibus::IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE {}
  virtual void UpdateAuxiliaryText(const ibus::IBusText& ibus_text,
                                   bool is_visible) OVERRIDE {}
  virtual void UpdateLookupTable(const ibus::IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE {}
  virtual void UpdateProperty(const ibus::IBusProperty& property) OVERRIDE {}
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE {}
  virtual void RequireSurroundingText() OVERRIDE {}
 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineServiceStubImpl);
};

IBusEngineService::IBusEngineService() {
}

IBusEngineService::~IBusEngineService() {
}

// static
IBusEngineService* IBusEngineService::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new IBusEngineServiceImpl(bus, object_path);
  else
    return new IBusEngineServiceStubImpl();
}

}  // namespace chromeos
