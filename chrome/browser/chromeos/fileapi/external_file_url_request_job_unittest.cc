// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_request_job.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/fake_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/drive/test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_options.h"
#include "google_apis/drive/test_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// A simple URLRequestJobFactory implementation to create
// ExternalFileURLRequestJob.
class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  explicit TestURLRequestJobFactory(void* profile_id)
      : profile_id_(profile_id) {}

  virtual ~TestURLRequestJobFactory() {}

  // net::URLRequestJobFactory override:
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new ExternalFileURLRequestJob(
        profile_id_, request, network_delegate);
  }

  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return scheme == chrome::kExternalFileScheme;
  }

  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return url.is_valid() && IsHandledProtocol(url.scheme());
  }

  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
    return true;
  }

 private:
  void* const profile_id_;
  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

class TestDelegate : public net::TestDelegate {
 public:
  TestDelegate() {}

  const GURL& redirect_url() const { return redirect_url_; }

  // net::TestDelegate override.
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const net::RedirectInfo& redirect_info,
                                  bool* defer_redirect) OVERRIDE {
    redirect_url_ = redirect_info.new_url;
    net::TestDelegate::OnReceivedRedirect(
        request, redirect_info, defer_redirect);
  }

 private:
  GURL redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class ExternalFileURLRequestJobTest : public testing::Test {
 protected:
  ExternalFileURLRequestJobTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        integration_service_factory_callback_(base::Bind(
            &ExternalFileURLRequestJobTest::CreateDriveIntegrationService,
            base::Unretained(this))),
        fake_file_system_(NULL) {}

  virtual ~ExternalFileURLRequestJobTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a testing profile.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* const profile =
        profile_manager_->CreateTestingProfile("test-user");

    // Create the drive integration service for the profile.
    integration_service_factory_scope_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &integration_service_factory_callback_));
    drive::DriveIntegrationServiceFactory::GetForProfile(profile);

    // Create the URL request job factory.
    test_network_delegate_.reset(new net::TestNetworkDelegate);
    test_url_request_job_factory_.reset(new TestURLRequestJobFactory(profile));
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_job_factory(test_url_request_job_factory_.get());
    url_request_context_->set_network_delegate(test_network_delegate_.get());
    test_delegate_.reset(new TestDelegate);
  }

  virtual void TearDown() { profile_manager_.reset(); }

  bool ReadDriveFileSync(const base::FilePath& file_path,
                         std::string* out_content) {
    scoped_ptr<base::Thread> worker_thread(
        new base::Thread("ReadDriveFileSync"));
    if (!worker_thread->Start())
      return false;

    scoped_ptr<drive::DriveFileStreamReader> reader(
        new drive::DriveFileStreamReader(
            base::Bind(&ExternalFileURLRequestJobTest::GetFileSystem,
                       base::Unretained(this)),
            worker_thread->message_loop_proxy().get()));
    int error = net::ERR_FAILED;
    scoped_ptr<drive::ResourceEntry> entry;
    {
      base::RunLoop run_loop;
      reader->Initialize(file_path,
                         net::HttpByteRange(),
                         google_apis::test_util::CreateQuitCallback(
                             &run_loop,
                             google_apis::test_util::CreateCopyResultCallback(
                                 &error, &entry)));
      run_loop.Run();
    }
    if (error != net::OK || !entry)
      return false;

    // Read data from the reader.
    std::string content;
    if (drive::test_util::ReadAllData(reader.get(), &content) != net::OK)
      return false;

    if (static_cast<size_t>(entry->file_info().size()) != content.size())
      return false;

    *out_content = content;
    return true;
  }

  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<TestDelegate> test_delegate_;

 private:
  // Create the drive integration service for the |profile|
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    drive::FakeDriveService* const drive_service = new drive::FakeDriveService;
    if (!drive::test_util::SetUpTestEntries(drive_service))
      return NULL;

    const std::string& drive_mount_name =
        drive::util::GetDriveMountPointPath(profile).BaseName().AsUTF8Unsafe();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        drive_mount_name,
        storage::kFileSystemTypeDrive,
        storage::FileSystemMountOption(),
        drive::util::GetDriveMountPointPath(profile));
    DCHECK(!fake_file_system_);
    fake_file_system_ = new drive::test_util::FakeFileSystem(drive_service);
    if (!drive_cache_dir_.CreateUniqueTempDir())
      return NULL;
    return new drive::DriveIntegrationService(profile,
                                              NULL,
                                              drive_service,
                                              drive_mount_name,
                                              drive_cache_dir_.path(),
                                              fake_file_system_);
  }

  drive::FileSystemInterface* GetFileSystem() { return fake_file_system_; }

  content::TestBrowserThreadBundle thread_bundle_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      integration_service_factory_callback_;
  scoped_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      integration_service_factory_scope_;
  scoped_ptr<drive::DriveIntegrationService> integration_service_;
  drive::test_util::FakeFileSystem* fake_file_system_;

  scoped_ptr<net::TestNetworkDelegate> test_network_delegate_;
  scoped_ptr<TestURLRequestJobFactory> test_url_request_job_factory_;

  scoped_ptr<TestingProfileManager> profile_manager_;
  base::ScopedTempDir drive_cache_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(ExternalFileURLRequestJobTest, NonGetMethod) {
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root/File 1.txt"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));
  request->set_method("POST");  // Set non "GET" method.
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, request->status().error());
}

TEST_F(ExternalFileURLRequestJobTest, RegularFile) {
  const GURL kTestUrl("externalfile:drive-test-user-hash/root/File 1.txt");
  const base::FilePath kTestFilePath("drive/root/File 1.txt");

  // For the first time, the file should be fetched from the server.
  {
    scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
        kTestUrl, net::DEFAULT_PRIORITY, test_delegate_.get(), NULL));
    request->Start();

    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
    // It looks weird, but the mime type for the "File 1.txt" is "audio/mpeg"
    // on the server.
    std::string mime_type;
    request->GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);

    // Reading file must be done after |request| runs, otherwise
    // it'll create a local cache file, and we cannot test correctly.
    std::string expected_data;
    ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
    EXPECT_EQ(expected_data, test_delegate_->data_received());
  }

  // For the second time, the locally cached file should be used.
  // The caching emulation is done by FakeFileSystem.
  {
    test_delegate_.reset(new TestDelegate);
    scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
        GURL("externalfile:drive-test-user-hash/root/File 1.txt"),
        net::DEFAULT_PRIORITY,
        test_delegate_.get(),
        NULL));
    request->Start();

    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
    std::string mime_type;
    request->GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);

    std::string expected_data;
    ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
    EXPECT_EQ(expected_data, test_delegate_->data_received());
  }
}

TEST_F(ExternalFileURLRequestJobTest, HostedDocument) {
  // Open a gdoc file.
  test_delegate_->set_quit_on_redirect(true);
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL(
          "externalfile:drive-test-user-hash/root/Document 1 "
          "excludeDir-test.gdoc"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
  // Make sure that a hosted document triggers redirection.
  EXPECT_TRUE(request->is_redirecting());
  EXPECT_TRUE(test_delegate_->redirect_url().is_valid());
}

TEST_F(ExternalFileURLRequestJobTest, RootDirectory) {
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_FAILED, request->status().error());
}

TEST_F(ExternalFileURLRequestJobTest, Directory) {
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root/Directory 1"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_FAILED, request->status().error());
}

TEST_F(ExternalFileURLRequestJobTest, NonExistingFile) {
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root/non-existing-file.txt"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request->status().error());
}

TEST_F(ExternalFileURLRequestJobTest, WrongFormat) {
  scoped_ptr<net::URLRequest> request(
      url_request_context_->CreateRequest(GURL("externalfile:"),
                                          net::DEFAULT_PRIORITY,
                                          test_delegate_.get(),
                                          NULL));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_INVALID_URL, request->status().error());
}

TEST_F(ExternalFileURLRequestJobTest, Cancel) {
  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root/File 1.txt"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      NULL));

  // Start the request, and cancel it immediately after it.
  request->Start();
  request->Cancel();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::CANCELED, request->status().status());
}

TEST_F(ExternalFileURLRequestJobTest, RangeHeader) {
  const GURL kTestUrl("externalfile:drive-test-user-hash/root/File 1.txt");
  const base::FilePath kTestFilePath("drive/root/File 1.txt");

  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      kTestUrl, net::DEFAULT_PRIORITY, test_delegate_.get(), NULL));

  // Set range header.
  request->SetExtraRequestHeaderByName(
      "Range", "bytes=3-5", false /* overwrite */);
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());

  // Reading file must be done after |request| runs, otherwise
  // it'll create a local cache file, and we cannot test correctly.
  std::string expected_data;
  ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
  EXPECT_EQ(expected_data.substr(3, 3), test_delegate_->data_received());
}

TEST_F(ExternalFileURLRequestJobTest, WrongRangeHeader) {
  const GURL kTestUrl("externalfile:drive-test-user-hash/root/File 1.txt");

  scoped_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      kTestUrl, net::DEFAULT_PRIORITY, test_delegate_.get(), NULL));

  // Set range header.
  request->SetExtraRequestHeaderByName(
      "Range", "Wrong Range Header Value", false /* overwrite */);
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, request->status().error());
}

}  // namespace chromeos
