// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"

#include "base/string_number_conversions.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/exported_object.h"

namespace chromeos {

class IBusEngineFactoryServiceImpl : public IBusEngineFactoryService {
 public:
  explicit IBusEngineFactoryServiceImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {

    exported_object_ = bus_->GetExportedObject(
        dbus::ObjectPath(ibus::engine_factory::kServicePath));

    exported_object_->ExportMethod(
        ibus::engine_factory::kServiceInterface,
        ibus::engine_factory::kCreateEngineMethod,
        base::Bind(&IBusEngineFactoryServiceImpl::CreateEngine,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineFactoryServiceImpl::CreateEngineExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~IBusEngineFactoryServiceImpl() {
    bus_->UnregisterExportedObject(dbus::ObjectPath(
        ibus::engine_factory::kServicePath));
  }

  // IBusEngineFactoryService override.
  virtual void SetCreateEngineHandler(
      const CreateEngineHandler& create_engine_handler) OVERRIDE {
    DCHECK(!create_engine_handler.is_null());
    create_engine_handler_ = create_engine_handler;
  }

  // IBusEngineFactoryService override.
  virtual void UnsetCreateEngineHandler() OVERRIDE {
    create_engine_handler_.Reset();
  }

 private:
  // Called when the ibus-daemon requires new engine instance.
  void CreateEngine(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender) {
    if (!method_call) {
      LOG(ERROR) << "method call does not have any arguments.";
      return;
    }
    dbus::MessageReader reader(method_call);
    std::string engine_name;

    if (!reader.PopString(&engine_name)) {
      LOG(ERROR) << "Expected argument is string.";
      return;
    }
    if(create_engine_handler_.is_null()) {
      LOG(WARNING) << "The CreateEngine handler is NULL.";
    } else {
      const dbus::ObjectPath path = create_engine_handler_.Run(engine_name);
      scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
      dbus::MessageWriter writer(response.get());
      writer.AppendObjectPath(path);
      response_sender.Run(response.get());
    }
  }

  // Called when the CreateEngine method is exported.
  void CreateEngineExported(const std::string& interface_name,
                            const std::string& method_name,
                            bool success) {
    DCHECK(success) << "Failed to export: "
                    << interface_name << "." << method_name;
  }

  // CreateEngine method call handler.
  CreateEngineHandler create_engine_handler_;

  dbus::Bus* bus_;
  scoped_refptr<dbus::ExportedObject> exported_object_;
  base::WeakPtrFactory<IBusEngineFactoryServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusEngineFactoryServiceImpl);
};

class IBusEngineFactoryServiceStubImpl : public IBusEngineFactoryService {
 public:
  IBusEngineFactoryServiceStubImpl() {}
  virtual ~IBusEngineFactoryServiceStubImpl() {}

  // IBusEngineFactoryService overrides.
  virtual void SetCreateEngineHandler(
      const CreateEngineHandler& create_engine_handler) OVERRIDE {}
  virtual void UnsetCreateEngineHandler() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineFactoryServiceStubImpl);
};

IBusEngineFactoryService::IBusEngineFactoryService() {
}

IBusEngineFactoryService::~IBusEngineFactoryService() {
}

// static
IBusEngineFactoryService* IBusEngineFactoryService::Create(
    dbus::Bus* bus,
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new IBusEngineFactoryServiceImpl(bus);
  else
    return new IBusEngineFactoryServiceStubImpl();
}

}  // namespace chromeos
