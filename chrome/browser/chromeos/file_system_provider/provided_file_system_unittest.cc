// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(mtomasz): Move these test cases to operations/unmount_unittest.cc.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {

namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const int kExpectedRequestId = 1;
const char kFileSystemId[] = "camera-pictures";
const char kDisplayName[] = "Camera Pictures";

class FakeEventRouter : public extensions::EventRouter {
 public:
  explicit FakeEventRouter(Profile* profile) : EventRouter(profile, NULL) {}
  virtual ~FakeEventRouter() {}

  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        scoped_ptr<extensions::Event> event)
      OVERRIDE {
    extension_id_ = extension_id;
    event_ = event.Pass();
  }

  const std::string& extension_id() const { return extension_id_; }

  const extensions::Event* event() const { return event_.get(); }

 private:
  std::string extension_id_;
  scoped_ptr<extensions::Event> event_;

  DISALLOW_COPY_AND_ASSIGN(FakeEventRouter);
};

class EventLogger {
 public:
  EventLogger() {}
  virtual ~EventLogger() {}

  void OnStatusCallback(base::File::Error error) {
    error_.reset(new base::File::Error(error));
  }

  base::File::Error* error() { return error_.get(); }

 private:
  scoped_ptr<base::File::Error> error_;
  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

}  // namespace

class FileSystemProviderProvidedFileSystemTest : public testing::Test {
 protected:
  FileSystemProviderProvidedFileSystemTest() {}
  virtual ~FileSystemProviderProvidedFileSystemTest() {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    event_router_.reset(new FakeEventRouter(profile_.get()));
    event_router_->AddEventListener(
        extensions::api::file_system_provider::OnUnmountRequested::kEventName,
        NULL,
        kExtensionId);

    const base::FilePath mount_path =
        util::GetMountPath(profile_.get(), kExtensionId, kFileSystemId);
    file_system_info_.reset(new ProvidedFileSystemInfo(
        kExtensionId, kFileSystemId, kDisplayName, mount_path));
    provided_file_system_.reset(
        new ProvidedFileSystem(event_router_.get(), *file_system_info_.get()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FakeEventRouter> event_router_;
  scoped_ptr<ProvidedFileSystemInfo> file_system_info_;
  scoped_ptr<ProvidedFileSystemInterface> provided_file_system_;
};

TEST_F(FileSystemProviderProvidedFileSystemTest, RequestUnmount_Success) {
  EventLogger logger;

  provided_file_system_->RequestUnmount(
      base::Bind(&EventLogger::OnStatusCallback, base::Unretained(logger)));
  base::RunLoop().RunUntilIdle();

  // Verify that the event has been sent to the providing extension.
  EXPECT_EQ(kExtensionId, event_router_->extension_id());
  const extensions::Event* event = event_router_->event();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->event_args);
  base::ListValue* event_args = event->event_args.get();
  EXPECT_EQ(2u, event_args->GetSize());
  std::string file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  // Remember the request id, and verify it is valid.
  int request_id = 0;
  EXPECT_TRUE(event_args->GetInteger(1, &request_id));
  EXPECT_EQ(kExpectedRequestId, request_id);

  // Callback should not be called, yet.
  EXPECT_FALSE(logger.error());

  // Simulate sending a success response from the providing extension.
  RequestManager* request_manager = provided_file_system_->GetRequestManager();
  ASSERT_TRUE(request_manager);
  scoped_ptr<RequestValue> response;
  bool reply_result = request_manager->FulfillRequest(
      request_id, response.Pass(), false /* has_more */);
  EXPECT_TRUE(reply_result);

  // Callback should be called. Verify the error code.
  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_OK, *logger.error());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, RequestUnmount_Error) {
  EventLogger logger;

  provided_file_system_->RequestUnmount(
      base::Bind(&EventLogger::OnStatusCallback, base::Unretained(logger)));
  base::RunLoop().RunUntilIdle();

  // Verify that the event has been sent to the providing extension.
  EXPECT_EQ(kExtensionId, event_router_->extension_id());
  const extensions::Event* event = event_router_->event();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->event_args);
  base::ListValue* event_args = event->event_args.get();
  EXPECT_EQ(2u, event_args->GetSize());
  std::string file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  // Remember the request id, and verify it is valid.
  int request_id = 0;
  EXPECT_TRUE(event_args->GetInteger(1, &request_id));
  EXPECT_EQ(kExpectedRequestId, request_id);

  // Simulate sending an error response from the providing extension.
  RequestManager* request_manager = provided_file_system_->GetRequestManager();
  ASSERT_TRUE(request_manager);
  bool reply_result = request_manager->RejectRequest(
      request_id, base::File::FILE_ERROR_NOT_FOUND);
  EXPECT_TRUE(reply_result);

  // Callback should be called. Verify the error code.
  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, *logger.error());
}

}  // namespace file_system_provider
}  // namespace chromeos
