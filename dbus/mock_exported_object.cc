// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/mock_exported_object.h"

namespace dbus {

MockExportedObject::MockExportedObject(Bus* bus,
                                       const std::string& service_name,
                                       const std::string& object_path)
    : ExportedObject(bus, service_name, object_path) {
}

MockExportedObject::~MockExportedObject() {
}

}  // namespace dbus
