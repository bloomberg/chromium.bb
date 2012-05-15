// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/introspect_util.h"

#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

// String constants used for parsing D-Bus Introspection XML data.
const char kInterfaceNode[] = "interface";
const char kInterfaceNameAttribute[] = "name";

}

namespace chromeos {

std::vector<std::string> GetInterfacesFromIntrospectResult(
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

}
