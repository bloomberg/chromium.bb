// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_INTROSPECTABLE_CLIENT_H_
#define CHROMEOS_DBUS_INTROSPECTABLE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// IntrospectableClient is used to retrieve the D-Bus introspection data
// from a remote object.
class CHROMEOS_EXPORT IntrospectableClient : public DBusClient {
 public:
  virtual ~IntrospectableClient();

  // The IntrospectCallback is used for the Introspect() method. It receives
  // four arguments, the first two are the |service_name| and |object_path|
  // of the remote object being introspected, the third is the |xml_data| of
  // the object as described in
  // http://dbus.freedesktop.org/doc/dbus-specification.html, the fourth
  // |success| indicates whether the request succeeded.
  typedef base::Callback<void(const std::string&, const dbus::ObjectPath&,
                              const std::string&, bool)> IntrospectCallback;

  // Retrieves introspection data from the remote object on service name
  // |service_name| with object path |object_path|, calling |callback| with
  // the XML-formatted data received.
  virtual void Introspect(const std::string& service_name,
                          const dbus::ObjectPath& object_path,
                          const IntrospectCallback& callback) = 0;

  // Parses XML-formatted introspection data returned by
  // org.freedesktop.DBus.Introspectable.Introspect and returns the list of
  // interface names declared within.
  static std::vector<std::string> GetInterfacesFromIntrospectResult(
      const std::string& xml_data);

  // Creates the instance
  static IntrospectableClient* Create();

 protected:
  IntrospectableClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(IntrospectableClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_INTROSPECTABLE_CLIENT_H_
