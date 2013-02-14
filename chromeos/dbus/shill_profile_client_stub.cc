// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_profile_client_stub.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ShillProfileClientStub::ShillProfileClientStub() : weak_ptr_factory_(this) {
}

ShillProfileClientStub::~ShillProfileClientStub() {
}

void ShillProfileClientStub::AddPropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillProfileClientStub::RemovePropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillProfileClientStub::GetProperties(
    const dbus::ObjectPath& profile_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillProfileClientStub::PassEmptyDictionaryValue,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ShillProfileClientStub::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillProfileClientStub::PassEmptyDictionaryValue,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ShillProfileClientStub::DeleteEntry(const dbus::ObjectPath& profile_path,
                                         const std::string& entry_path,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillProfileClientStub::PassEmptyDictionaryValue(
    const DictionaryValueCallbackWithoutStatus& callback) const {
  base::DictionaryValue dictionary;
  callback.Run(dictionary);
}

}  // namespace chromeos
