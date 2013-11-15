// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/pickle.h"
#include "base/values.h"
#include "chrome/common/extensions/api/sockets/sockets_manifest_permission.h"
#include "chrome/common/extensions/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

static void AssertEmptyPermission(const SocketsManifestPermission* permission) {
  EXPECT_TRUE(permission);
  EXPECT_EQ(std::string(extensions::manifest_keys::kSockets), permission->id());
  EXPECT_EQ(permission->id(), permission->name());
  EXPECT_FALSE(permission->HasMessages());
  EXPECT_EQ(0u, permission->entries().size());
}

}  // namespace

TEST(SocketsManifestPermissionTest, Empty) {
  // Construction
  scoped_ptr<SocketsManifestPermission> permission(
      new SocketsManifestPermission());
  AssertEmptyPermission(permission.get());

  // Clone()/Equal()
  scoped_ptr<SocketsManifestPermission> clone(
      static_cast<SocketsManifestPermission*>(permission->Clone()));
  AssertEmptyPermission(clone.get());

  EXPECT_TRUE(permission->Equal(clone.get()));

  // ToValue()/FromValue()
  scoped_ptr<const base::Value> value(permission->ToValue());
  EXPECT_TRUE(value.get());

  scoped_ptr<SocketsManifestPermission> permission2(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2->FromValue(value.get()));
  AssertEmptyPermission(permission2.get());

  // Union/Diff/Intersection
  scoped_ptr<SocketsManifestPermission> diff_perm(
      static_cast<SocketsManifestPermission*>(permission->Diff(clone.get())));
  AssertEmptyPermission(diff_perm.get());

  scoped_ptr<SocketsManifestPermission> union_perm(
      static_cast<SocketsManifestPermission*>(permission->Union(clone.get())));
  AssertEmptyPermission(union_perm.get());

  scoped_ptr<SocketsManifestPermission> intersect_perm(
      static_cast<SocketsManifestPermission*>(
          permission->Intersect(clone.get())));
  AssertEmptyPermission(intersect_perm.get());

  // IPC
  scoped_ptr<SocketsManifestPermission> ipc_perm(
      new SocketsManifestPermission());
  scoped_ptr<SocketsManifestPermission> ipc_perm2(
      new SocketsManifestPermission());

  IPC::Message m;
  ipc_perm->Write(&m);
  PickleIterator iter(m);
  EXPECT_TRUE(ipc_perm2->Read(&m, &iter));
  AssertEmptyPermission(ipc_perm2.get());
}

TEST(SocketsManifestPermissionTest, General) {
  std::string udp_send_string = "{ \"udp\": { \"send\": \"\" } }";
  scoped_ptr<base::Value> udp_send(base::JSONReader::Read(udp_send_string));
  EXPECT_TRUE(udp_send);

  std::string udp_bind_string = "{ \"udp\": { \"bind\": \"127.0.0.1:3007\" } }";
  scoped_ptr<base::Value> udp_bind(base::JSONReader::Read(udp_bind_string));
  EXPECT_TRUE(udp_bind);

  std::string tcp_connect_string =
      "{ \"tcp\": { \"connect\": \"127.0.0.1:80\" } }";
  scoped_ptr<base::Value> tcp_connect(
      base::JSONReader::Read(tcp_connect_string));
  EXPECT_TRUE(tcp_connect);

  std::string tcp_server_listen_string =
      "{ \"tcpServer\": { \"listen\": \"127.0.0.1:80\" } }";
  scoped_ptr<base::Value> tcp_server_listen(
      base::JSONReader::Read(tcp_server_listen_string));
  EXPECT_TRUE(tcp_server_listen);

  // FromValue()
  scoped_ptr<SocketsManifestPermission> permission1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission1->FromValue(udp_send.get()));
  EXPECT_EQ(1u, permission1->entries().size());

  scoped_ptr<SocketsManifestPermission> permission2(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2->FromValue(udp_bind.get()));
  EXPECT_EQ(1u, permission2->entries().size());

  scoped_ptr<SocketsManifestPermission> permission3(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission3->FromValue(tcp_connect.get()));
  EXPECT_EQ(1u, permission3->entries().size());

  scoped_ptr<SocketsManifestPermission> permission4(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission4->FromValue(tcp_server_listen.get()));
  EXPECT_EQ(1u, permission4->entries().size());

  // ToValue()
  scoped_ptr<base::Value> value1 = permission1->ToValue();
  EXPECT_TRUE(value1);
  scoped_ptr<SocketsManifestPermission> permission1_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission1_1->FromValue(value1.get()));
  EXPECT_TRUE(permission1->Equal(permission1_1.get()));

  scoped_ptr<base::Value> value2 = permission2->ToValue();
  EXPECT_TRUE(value2);
  scoped_ptr<SocketsManifestPermission> permission2_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2_1->FromValue(value2.get()));
  EXPECT_TRUE(permission2->Equal(permission2_1.get()));

  scoped_ptr<base::Value> value3 = permission3->ToValue();
  EXPECT_TRUE(value3);
  scoped_ptr<SocketsManifestPermission> permission3_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission3_1->FromValue(value3.get()));
  EXPECT_TRUE(permission3->Equal(permission3_1.get()));

  scoped_ptr<base::Value> value4 = permission4->ToValue();
  EXPECT_TRUE(value4);
  scoped_ptr<SocketsManifestPermission> permission4_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission4_1->FromValue(value4.get()));
  EXPECT_TRUE(permission4->Equal(permission4_1.get()));

  // Union/Diff/Intersection
  scoped_ptr<SocketsManifestPermission> union_perm(
      static_cast<SocketsManifestPermission*>(
          permission1->Union(permission2.get())));
  EXPECT_TRUE(union_perm);
  EXPECT_EQ(2u, union_perm->entries().size());

  EXPECT_TRUE(union_perm->Contains(permission1.get()));
  EXPECT_TRUE(union_perm->Contains(permission2.get()));
  EXPECT_FALSE(union_perm->Contains(permission3.get()));
  EXPECT_FALSE(union_perm->Contains(permission4.get()));

  scoped_ptr<SocketsManifestPermission> diff_perm1(
      static_cast<SocketsManifestPermission*>(
          permission1->Diff(permission2.get())));
  EXPECT_TRUE(diff_perm1);
  EXPECT_EQ(1u, diff_perm1->entries().size());

  EXPECT_TRUE(permission1->Equal(diff_perm1.get()));
  EXPECT_TRUE(diff_perm1->Equal(permission1.get()));

  scoped_ptr<SocketsManifestPermission> diff_perm2(
      static_cast<SocketsManifestPermission*>(
          permission1->Diff(union_perm.get())));
  EXPECT_TRUE(diff_perm2);
  AssertEmptyPermission(diff_perm2.get());

  scoped_ptr<SocketsManifestPermission> intersect_perm1(
      static_cast<SocketsManifestPermission*>(
          union_perm->Intersect(permission1.get())));
  EXPECT_TRUE(intersect_perm1);
  EXPECT_EQ(1u, intersect_perm1->entries().size());

  EXPECT_TRUE(permission1->Equal(intersect_perm1.get()));
  EXPECT_TRUE(intersect_perm1->Equal(permission1.get()));

}

}  // namespace extensions
