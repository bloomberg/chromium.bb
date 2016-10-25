// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_provider.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;
using std::string;
using std::unique_ptr;

namespace chromeos {
namespace printing {
namespace {

const char kTestQuirksServer[] = "bogusserver.bogus.com";
const char kTestAPIKey[] = "BOGUSAPIKEY";
const char kLocalPpdUrl[] = "/some/path";
const char kTestManufacturer[] = "Bogus Printer Corp";
const char kTestModel[] = "MegaPrint 9000";
const char kQuirksResponse[] =
    "{\n"
    "  \"compressedPpd\": \"This is the quirks ppd\",\n"
    "  \"lastUpdatedTime\": \"1\"\n"
    "}\n";
const char kQuirksPpd[] = "This is the quirks ppd";

class PpdProviderTest : public ::testing::Test {
 public:
  PpdProviderTest()
      : loop_(base::MessageLoop::TYPE_IO),
        request_context_getter_(
            new net::TestURLRequestContextGetter(loop_.task_runner().get())) {}

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
    auto provider_options = PpdProvider::Options();
    provider_options.quirks_server = kTestQuirksServer;
    ppd_provider_ = PpdProvider::Create(
        kTestAPIKey, request_context_getter_,
        PpdCache::Create(ppd_cache_temp_dir_.GetPath()), provider_options);
  }

 protected:
  base::ScopedTempDir ppd_cache_temp_dir_;

  // Provider to be used in the test.
  unique_ptr<PpdProvider> ppd_provider_;

  // Misc extra stuff needed for the test environment to function.
  base::MessageLoop loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

// Struct that just captures the callback result for a PpdProvider lookup and
// saves it for inspection by the test.
struct CapturedResolveResult {
  bool initialized = false;
  PpdProvider::ResolveResult result;
  FilePath file;
};

// Callback for saving a resolve callback.
void CaptureResolveResultCallback(CapturedResolveResult* capture,
                                  PpdProvider::ResolveResult result,
                                  FilePath file) {
  capture->initialized = true;
  capture->result = result;
  capture->file = file;
}

// For a resolve result that should end up successful, check that it is
// successful and the contents are expected_contents.
void CheckResolveSuccessful(const CapturedResolveResult& captured,
                            const string& expected_contents) {
  ASSERT_TRUE(captured.initialized);
  EXPECT_EQ(PpdProvider::SUCCESS, captured.result);

  string contents;
  ASSERT_TRUE(base::ReadFileToString(captured.file, &contents));
  EXPECT_EQ(expected_contents, contents);
}

// Resolve a PPD via the quirks server.
TEST_F(PpdProviderTest, QuirksServerResolve) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());

  Printer::PpdReference ppd_reference;
  ppd_reference.effective_manufacturer = kTestManufacturer;
  ppd_reference.effective_model = kTestModel;

  {
    net::TestURLRequestInterceptor interceptor(
        "https", kTestQuirksServer, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());

    GURL expected_url(base::StringPrintf(
        "https://%s/v2/printer/manufacturers/%s/models/%s?key=%s",
        kTestQuirksServer, kTestManufacturer, kTestModel, kTestAPIKey));

    FilePath contents_path = temp_dir.GetPath().Append("response");
    string contents = kQuirksResponse;
    int bytes_written = base::WriteFile(contents_path, kQuirksResponse,
                                        strlen(kQuirksResponse));
    ASSERT_EQ(bytes_written, static_cast<int>(strlen(kQuirksResponse)));

    interceptor.SetResponse(expected_url, contents_path);

    CapturedResolveResult captured;
    ppd_provider_->Resolve(ppd_reference,
                           base::Bind(CaptureResolveResultCallback, &captured));
    base::RunLoop().RunUntilIdle();
    CheckResolveSuccessful(captured, kQuirksPpd);
  }

  // Now that the interceptor is out of scope, re-run the query.  We should
  // hit in the cache, and thus *not* re-run the query.
  CapturedResolveResult captured;
  ppd_provider_->Resolve(ppd_reference,
                         base::Bind(CaptureResolveResultCallback, &captured));
  base::RunLoop().RunUntilIdle();
  CheckResolveSuccessful(captured, kQuirksPpd);
}

// Test storage and retrieval of PPDs that are added manually.  Even though we
// supply a manufacturer and model, we should *not* hit the network for this
// resolution since we should find the stored version already cached.
TEST_F(PpdProviderTest, LocalResolve) {
  Printer::PpdReference ppd_reference;
  ppd_reference.user_supplied_ppd_url = kLocalPpdUrl;
  ppd_reference.effective_manufacturer = kTestManufacturer;
  ppd_reference.effective_model = kTestModel;

  // Initially, should not resolve.
  {
    CapturedResolveResult captured;
    ppd_provider_->Resolve(ppd_reference,
                           base::Bind(CaptureResolveResultCallback, &captured));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(captured.initialized);
    EXPECT_EQ(PpdProvider::NOT_FOUND, captured.result);
  }

  // Store a local ppd.
  const string kLocalPpdContents("My local ppd contents");
  {
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());

    FilePath local_ppd_path = temp_dir.GetPath().Append("local_ppd");
    ASSERT_EQ(base::WriteFile(local_ppd_path, kLocalPpdContents.data(),
                              kLocalPpdContents.size()),
              static_cast<int>(kLocalPpdContents.size()));
    ASSERT_TRUE(ppd_provider_->CachePpd(ppd_reference, local_ppd_path));
  }
  // temp_dir should now be deleted, which helps make sure we actually latched a
  // copy, not a reference.

  // Retry the resove, should get the PPD back now.
  {
    CapturedResolveResult captured;

    ppd_provider_->Resolve(ppd_reference,
                           base::Bind(CaptureResolveResultCallback, &captured));
    base::RunLoop().RunUntilIdle();
    CheckResolveSuccessful(captured, kLocalPpdContents);
  }
}

}  // namespace
}  // namespace printing
}  // namespace chromeos
