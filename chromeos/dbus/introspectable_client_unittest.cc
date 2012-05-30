// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/introspectable_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kXmlData[] =
"<!DOCTYPE node PUBLIC "
"\"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg type=\"s\" direction=\"out\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.bluez.Device\">\n"
"    <method name=\"GetProperties\">\n"
"      <arg type=\"a{sv}\" direction=\"out\"/>\n"
"    </method>\n"
"    <method name=\"SetProperty\">\n"
"      <arg type=\"s\" direction=\"in\"/>\n"
"      <arg type=\"v\" direction=\"in\"/>\n"
"    </method>\n"
"    <method name=\"DiscoverServices\">\n"
"      <arg type=\"s\" direction=\"in\"/>\n"
"      <arg type=\"a{us}\" direction=\"out\"/>\n"
"    </method>\n"
"    <method name=\"CancelDiscovery\"/>\n"
"    <method name=\"Disconnect\"/>\n"
"    <signal name=\"PropertyChanged\">\n"
"      <arg type=\"s\"/>\n"
"      <arg type=\"v\"/>\n"
"    </signal>\n"
"    <signal name=\"DisconnectRequested\"/>\n"
"  </interface>\n"
"  <interface name=\"org.bluez.Input\">\n"
"    <method name=\"Connect\"/>\n"
"    <method name=\"Disconnect\"/>\n"
"    <method name=\"GetProperties\">\n"
"      <arg type=\"a{sv}\" direction=\"out\"/>\n"
"    </method>\n"
"    <signal name=\"PropertyChanged\">\n"
"      <arg type=\"s\"/>\n"
"      <arg type=\"v\"/>\n"
"    </signal>\n"
"  </interface>\n"
"</node>";

}  // namespace

namespace chromeos {

TEST(IntrospectableClientTest, GetInterfacesFromIntrospectResult) {
  std::vector<std::string> interfaces =
      IntrospectableClient::GetInterfacesFromIntrospectResult(kXmlData);

  ASSERT_EQ(3U, interfaces.size());
  EXPECT_EQ("org.freedesktop.DBus.Introspectable", interfaces[0]);
  EXPECT_EQ("org.bluez.Device", interfaces[1]);
  EXPECT_EQ("org.bluez.Input", interfaces[2]);
}

}  // namespace chromeos
