// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_async_file_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kArcUrl[] = "content://org.chromium.foo/bar";
constexpr int64_t kSize = 123456;

class ArcIntentHelperInstanceTestImpl : public FakeIntentHelperInstance {
 public:
  void GetFileSizeDeprecated(
      const std::string& url,
      const GetFileSizeDeprecatedCallback& callback) override {
    EXPECT_EQ(kArcUrl, url);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, kSize));
  }
};

class ArcContentFileSystemAsyncFileUtilTest : public testing::Test {
 public:
  ArcContentFileSystemAsyncFileUtilTest() {
    fake_arc_bridge_service_.intent_helper()->SetInstance(&intent_helper_);
  }

  ~ArcContentFileSystemAsyncFileUtilTest() override = default;

 protected:
  storage::FileSystemURL ExternalFileURLToFileSystemURL(const GURL& url) {
    base::FilePath mount_point_virtual_path =
        base::FilePath::FromUTF8Unsafe(kMountPointName);
    base::FilePath virtual_path = chromeos::ExternalFileURLToVirtualPath(url);
    base::FilePath path(kMountPointPath);
    EXPECT_TRUE(
        mount_point_virtual_path.AppendRelativePath(virtual_path, &path));
    return storage::FileSystemURL::CreateForTest(
        GURL(),  // origin
        storage::kFileSystemTypeArcContent, path);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  FakeArcBridgeService fake_arc_bridge_service_;
  ArcIntentHelperInstanceTestImpl intent_helper_;
  ArcContentFileSystemAsyncFileUtil async_file_util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemAsyncFileUtilTest);
};

}  // namespace

TEST_F(ArcContentFileSystemAsyncFileUtilTest, GetFileInfo) {
  GURL externalfile_url = ArcUrlToExternalFileUrl(GURL(kArcUrl));

  base::RunLoop run_loop;
  async_file_util_.GetFileInfo(
      std::unique_ptr<storage::FileSystemOperationContext>(),
      ExternalFileURLToFileSystemURL(externalfile_url),
      -1,  // fields
      base::Bind(
          [](base::RunLoop* run_loop, base::File::Error error,
             const base::File::Info& info) {
            EXPECT_EQ(base::File::FILE_OK, error);
            EXPECT_EQ(kSize, info.size);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace arc
