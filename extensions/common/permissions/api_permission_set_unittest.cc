// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(APIPermissionSetTest, General) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kBackground);
  apis.insert(APIPermission::kProxy);
  apis.insert(APIPermission::kClipboardWrite);
  apis.insert(APIPermission::kPlugin);

  EXPECT_EQ(apis.find(APIPermission::kProxy)->id(), APIPermission::kProxy);
  EXPECT_TRUE(apis.find(APIPermission::kSocket) == apis.end());

  EXPECT_EQ(apis.size(), 5u);

  EXPECT_EQ(apis.erase(APIPermission::kTab), 1u);
  EXPECT_EQ(apis.size(), 4u);

  EXPECT_EQ(apis.erase(APIPermission::kTab), 0u);
  EXPECT_EQ(apis.size(), 4u);
}

TEST(APIPermissionSetTest, CreateUnion) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }

  // Union with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  apis1.insert(permission->Clone());
  expected_apis.insert(APIPermission::kTab);
  expected_apis.insert(APIPermission::kBackground);
  expected_apis.insert(permission);

  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);

  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-send-to::8899"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kTab);
  expected_apis.insert(APIPermission::kProxy);
  expected_apis.insert(APIPermission::kClipboardWrite);
  expected_apis.insert(APIPermission::kPlugin);

  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    value->Append(new base::StringValue("udp-send-to::8899"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  // Insert a new socket permission which will replace the old one.
  expected_apis.insert(permission);

  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateIntersection) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Intersection with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis1.insert(permission);

  APIPermissionSet::Intersection(apis1, apis2, &result);
  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    value->Append(new base::StringValue("udp-send-to::8899"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kTab);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  expected_apis.insert(permission);

  APIPermissionSet::Intersection(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateDifference) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Difference with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis1.insert(permission);

  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_EQ(apis1, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-send-to::8899"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  expected_apis.insert(permission);

  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(result));

  EXPECT_EQ(expected_apis, result);

  // |result| = |apis1| - |apis2| --> |result| intersect |apis2| == empty_set
  APIPermissionSet result2;
  APIPermissionSet::Intersection(result, apis2, &result2);
  EXPECT_TRUE(result2.empty());
}

TEST(APIPermissionSetTest, IPC) {
  APIPermission* permission = NULL;

  APIPermissionSet apis;
  APIPermissionSet expected_apis;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(new base::StringValue("tcp-connect:*.example.com:80"));
    value->Append(new base::StringValue("udp-bind::8080"));
    value->Append(new base::StringValue("udp-send-to::8888"));
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis.insert(permission);

  EXPECT_NE(apis, expected_apis);

  IPC::Message m;
  WriteParam(&m, apis);
  PickleIterator iter(m);
  CHECK(ReadParam(&m, &iter, &expected_apis));
  EXPECT_EQ(apis, expected_apis);
}

TEST(APIPermissionSetTest, ImplicitPermissions) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kFileSystemWrite);
  apis.AddImpliedPermissions();

  EXPECT_EQ(apis.find(APIPermission::kFileSystemWrite)->id(),
            APIPermission::kFileSystemWrite);
  EXPECT_EQ(apis.size(), 1u);

  apis.erase(APIPermission::kFileSystemWrite);
  apis.insert(APIPermission::kFileSystemDirectory);
  apis.AddImpliedPermissions();

  EXPECT_EQ(apis.find(APIPermission::kFileSystemDirectory)->id(),
            APIPermission::kFileSystemDirectory);
  EXPECT_EQ(apis.size(), 1u);

  apis.insert(APIPermission::kFileSystemWrite);
  apis.AddImpliedPermissions();

  EXPECT_EQ(apis.find(APIPermission::kFileSystemWrite)->id(),
            APIPermission::kFileSystemWrite);
  EXPECT_EQ(apis.find(APIPermission::kFileSystemDirectory)->id(),
            APIPermission::kFileSystemDirectory);
  EXPECT_EQ(apis.find(APIPermission::kFileSystemWriteDirectory)->id(),
            APIPermission::kFileSystemWriteDirectory);
  EXPECT_EQ(apis.size(), 3u);
}

}  // namespace extensions
