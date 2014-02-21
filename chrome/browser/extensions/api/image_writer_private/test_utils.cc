// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#endif

namespace extensions {
namespace image_writer {

#if defined(OS_CHROMEOS)
namespace {

class ImageWriterFakeImageBurnerClient
    : public chromeos::FakeImageBurnerClient {
 public:
  ImageWriterFakeImageBurnerClient() {}
  virtual ~ImageWriterFakeImageBurnerClient() {}

  virtual void SetEventHandlers(
      const BurnFinishedHandler& burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) OVERRIDE {
    burn_finished_handler_ = burn_finished_handler;
    burn_progress_update_handler_ = burn_progress_update_handler;
  }

  virtual void BurnImage(const std::string& from_path,
                         const std::string& to_path,
                         const ErrorCallback& error_callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(burn_progress_update_handler_, to_path, 0, 100));
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(burn_progress_update_handler_, to_path, 50, 100));
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(burn_progress_update_handler_, to_path, 100, 100));
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(burn_finished_handler_, to_path, true, ""));
  }

 private:
  BurnFinishedHandler burn_finished_handler_;
  BurnProgressUpdateHandler burn_progress_update_handler_;
};

} // namespace
#endif

MockOperationManager::MockOperationManager() : OperationManager(NULL) {}
MockOperationManager::MockOperationManager(Profile* profile)
    : OperationManager(profile) {}
MockOperationManager::~MockOperationManager() {}

FakeImageWriterClient::FakeImageWriterClient() {}
FakeImageWriterClient::~FakeImageWriterClient() {}

void FakeImageWriterClient::Write(const ProgressCallback& progress_callback,
                                  const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback,
                                  const base::FilePath& source,
                                  const base::FilePath& target) {
  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;
}

void FakeImageWriterClient::Verify(const ProgressCallback& progress_callback,
                                   const SuccessCallback& success_callback,
                                   const ErrorCallback& error_callback,
                                   const base::FilePath& source,
                                   const base::FilePath& target) {
  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;
}

void FakeImageWriterClient::Cancel(const CancelCallback& cancel_callback) {
  cancel_callback_ = cancel_callback;
}

void FakeImageWriterClient::Shutdown() {
  // Clear handlers to not hold any reference to the caller.
  success_callback_ = base::Closure();
  progress_callback_ = base::Callback<void(int64)>();
  error_callback_ = base::Callback<void(const std::string&)>();
  cancel_callback_ = base::Closure();
}

void FakeImageWriterClient::Progress(int64 progress) {
  progress_callback_.Run(progress);
}

void FakeImageWriterClient::Success() { success_callback_.Run(); }

void FakeImageWriterClient::Error(const std::string& message) {
  error_callback_.Run(message);
}

void FakeImageWriterClient::Cancel() { cancel_callback_.Run(); }

scoped_refptr<FakeImageWriterClient> FakeImageWriterClient::Create() {
  return scoped_refptr<FakeImageWriterClient>(new FakeImageWriterClient());
}

ImageWriterUnitTestBase::ImageWriterUnitTestBase()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
ImageWriterUnitTestBase::~ImageWriterUnitTestBase() {}

void ImageWriterUnitTestBase::SetUp() {
  testing::Test::SetUp();

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(),
                                             &test_image_path_));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(),
                                             &test_device_path_));

  ASSERT_TRUE(FillFile(test_image_path_, kImagePattern, kTestFileSize));
  ASSERT_TRUE(FillFile(test_device_path_, kDevicePattern, kTestFileSize));

#if defined(OS_CHROMEOS)
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::FakeDBusThreadManager* fake_dbus_thread_manager =
        new chromeos::FakeDBusThreadManager;
    scoped_ptr<chromeos::ImageBurnerClient>
        image_burner_fake(new ImageWriterFakeImageBurnerClient());
    fake_dbus_thread_manager->SetImageBurnerClient(image_burner_fake.Pass());
    chromeos::DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);
  }
#endif
}

void ImageWriterUnitTestBase::TearDown() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Shutdown();
#endif
}

bool ImageWriterUnitTestBase::ImageWrittenToDevice(
    const base::FilePath& image_path,
    const base::FilePath& device_path) {
  scoped_ptr<char[]> image_buffer(new char[kTestFileSize]);
  scoped_ptr<char[]> device_buffer(new char[kTestFileSize]);

  int image_bytes_read =
      ReadFile(image_path, image_buffer.get(), kTestFileSize);

  if (image_bytes_read < 0)
    return false;

  int device_bytes_read =
      ReadFile(device_path, device_buffer.get(), kTestFileSize);

  if (image_bytes_read != device_bytes_read)
    return false;

  return memcmp(image_buffer.get(), device_buffer.get(), image_bytes_read) == 0;
}

bool ImageWriterUnitTestBase::FillFile(const base::FilePath& file,
                                       const int pattern,
                                       const int length) {
  scoped_ptr<char[]> buffer(new char[length]);
  memset(buffer.get(), pattern, length);

  return file_util::WriteFile(file, buffer.get(), length) == length;
}

}  // namespace image_writer
}  // namespace extensions
