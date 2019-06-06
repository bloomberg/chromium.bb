// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class NativeFileSystemHandleBaseTest : public testing::Test {
 public:
  NativeFileSystemHandleBaseTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_);
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  base::test::ScopedFeatureList scoped_feature_list_;
  TestBrowserThreadBundle scoped_task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;
};

TEST_F(NativeFileSystemHandleBaseTest, InitialPermissionStatus_TestURL) {
  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("test"));
  NativeFileSystemHandleBase handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url,
                                    storage::IsolatedContext::ScopedFSHandle());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetReadPermissionStatus());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetWritePermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest, InitialPermissionStatus_SandboxedURL) {
  auto url = FileSystemURL::CreateForTest(
      kTestOrigin, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test"));

  NativeFileSystemHandleBase handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url,
                                    storage::IsolatedContext::ScopedFSHandle());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetReadPermissionStatus());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetWritePermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest, InitialPermissionStatus_NativeFSURL) {
  auto url = FileSystemURL::CreateForTest(
      kTestOrigin, storage::kFileSystemTypeNativeLocal,
      base::FilePath::FromUTF8Unsafe("test"));

  NativeFileSystemHandleBase handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url,
                                    storage::IsolatedContext::ScopedFSHandle());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetReadPermissionStatus());
  EXPECT_EQ(PermissionStatus::ASK, handle.GetWritePermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest, RequestWritePermission_NativeFSURL) {
  auto url = FileSystemURL::CreateForTest(
      kTestOrigin, storage::kFileSystemTypeNativeLocal,
      base::FilePath::FromUTF8Unsafe("test"));

  NativeFileSystemHandleBase handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url,
                                    storage::IsolatedContext::ScopedFSHandle());

  base::RunLoop loop;
  handle.DoRequestPermission(
      /*writable=*/true,
      base::BindLambdaForTesting([&](PermissionStatus result) {
        EXPECT_EQ(PermissionStatus::DENIED, result);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_EQ(PermissionStatus::DENIED, handle.GetWritePermissionStatus());
}

}  // namespace content
