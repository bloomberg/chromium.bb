// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/introspectable_client.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

// D-Bus specification constants.
const char kIntrospectableInterface[] = "org.freedesktop.DBus.Introspectable";
const char kIntrospect[] = "Introspect";

// String constants used for parsing D-Bus Introspection XML data.
const char kInterfaceNode[] = "interface";
const char kInterfaceNameAttribute[] = "name";

}  // namespace

namespace chromeos {

// The IntrospectableClient implementation used in production.
class IntrospectableClientImpl : public IntrospectableClient {
 public:
  IntrospectableClientImpl() : bus_(NULL), weak_ptr_factory_(this) {}

  virtual ~IntrospectableClientImpl() {
  }

  // IntrospectableClient override.
  virtual void Introspect(const std::string& service_name,
                          const dbus::ObjectPath& object_path,
                          const IntrospectCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(kIntrospectableInterface, kIntrospect);

    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(service_name,
                                                           object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IntrospectableClientImpl::OnIntrospect,
                   weak_ptr_factory_.GetWeakPtr(),
                   service_name, object_path, callback));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE { bus_ = bus; }

 private:
  // Called by dbus:: when a response for Introspect() is recieved.
  void OnIntrospect(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const IntrospectCallback& callback,
                    dbus::Response* response) {
    // Parse response.
    bool success = false;
    std::string xml_data;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopString(&xml_data)) {
        LOG(WARNING) << "Introspect response has incorrect paramters: "
                     << response->ToString();
      } else {
        success = true;
      }
    }

    // Notify client.
    callback.Run(service_name, object_path, xml_data, success);
  }

  dbus::Bus* bus_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<IntrospectableClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IntrospectableClientImpl);
};

IntrospectableClient::IntrospectableClient() {
}

IntrospectableClient::~IntrospectableClient() {
}

// static
std::vector<std::string>
IntrospectableClient::GetInterfacesFromIntrospectResult(
    const std::string& xml_data) {
  std::vector<std::string> interfaces;

  XmlReader reader;
  if (!reader.Load(xml_data))
    return interfaces;

  do {
    // Skip to the next open tag, exit when done.
    while (!reader.SkipToElement()) {
      if (!reader.Read()) {
        return interfaces;
      }
    }

    // Only look at interface nodes.
    if (reader.NodeName() != kInterfaceNode)
      continue;

    // Skip if missing the interface name.
    std::string interface_name;
    if (!reader.NodeAttribute(kInterfaceNameAttribute, &interface_name))
      continue;

    interfaces.push_back(interface_name);
  } while (reader.Read());

  return interfaces;
}

// static
IntrospectableClient* IntrospectableClient::Create() {
  return new IntrospectableClientImpl();
}

}  // namespace chromeos
