// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

// Used to hold the result of a callback from the ContentHashFetcher.
struct ContentHashFetcherResult {
  std::string extension_id;
  bool success;
  bool force;
  std::set<base::FilePath> mismatch_paths;
};

// Allows waiting for the callback from a ContentHashFetcher, returning the
// data that was passed to that callback.
class ContentHashFetcherWaiter {
 public:
  ContentHashFetcherWaiter() : weak_factory_(this) {}

  ContentHashFetcher::FetchCallback GetCallback() {
    return base::Bind(&ContentHashFetcherWaiter::Callback,
                      weak_factory_.GetWeakPtr());
  }

  std::unique_ptr<ContentHashFetcherResult> WaitForCallback() {
    if (!result_) {
      base::RunLoop run_loop;
      run_loop_quit_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return std::move(result_);
  }

 private:
  // Matches signature of ContentHashFetcher::FetchCallback.
  void Callback(const std::string& extension_id,
                bool success,
                bool force,
                const std::set<base::FilePath>& mismatch_paths) {
    result_ = std::make_unique<ContentHashFetcherResult>();
    result_->extension_id = extension_id;
    result_->success = success;
    result_->force = force;
    result_->mismatch_paths = mismatch_paths;
    if (run_loop_quit_)
      std::move(run_loop_quit_).Run();
  }

  base::OnceClosure run_loop_quit_;
  std::unique_ptr<ContentHashFetcherResult> result_;
  base::WeakPtrFactory<ContentHashFetcherWaiter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcherWaiter);
};

class ContentHashFetcherTest : public ExtensionsTest {
 public:
  ContentHashFetcherTest()
      // We need a real IO thread to be able to intercept the network request
      // for the missing verified_contents.json file.
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>(
            content::TestBrowserThreadBundle::REAL_IO_THREAD)) {
    request_context_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
  }
  ~ContentHashFetcherTest() override {}

  bool LoadTestExtension() {
    test_dir_base_ = GetTestPath(
        base::FilePath(FILE_PATH_LITERAL("missing_verified_contents")));

    // We unzip the extension source to a temp directory to simulate it being
    // installed there, because the ContentHashFetcher will create the
    // _metadata/ directory within the extension install dir and write the
    // fetched verified_contents.json file there.
    extension_ =
        UnzipToTempDirAndLoad(test_dir_base_.AppendASCII("source.zip"));
    if (!extension_.get())
      return false;

    // Make sure there isn't already a verified_contents.json file there.
    EXPECT_FALSE(VerifiedContentsFileExists());
    delegate_ = std::make_unique<MockContentVerifierDelegate>();
    fetch_url_ = delegate_->GetSignatureFetchUrl(extension_->id(),
                                                 extension_->version());
    return true;
  }

  std::unique_ptr<ContentHashFetcherResult> DoHashFetch() {
    if (!extension_.get() || !delegate_.get()) {
      ADD_FAILURE() << "No valid extension_ or delegate_, "
                       "did you forget to call LoadTestExtension()?";
      return nullptr;
    }

    ContentHashFetcherWaiter waiter;
    ContentHashFetcher fetcher(request_context(), delegate_.get(),
                               waiter.GetCallback());
    fetcher.DoFetch(extension_.get(), true /* force */);
    return waiter.WaitForCallback();
  }
  const GURL& fetch_url() { return fetch_url_; }

  const base::FilePath& extension_root() { return extension_->path(); }

  bool VerifiedContentsFileExists() const {
    return base::PathExists(
        file_util::GetVerifiedContentsPath(extension_->path()));
  }

  base::FilePath GetResourcePath(const std::string& resource_filename) const {
    return test_dir_base_.AppendASCII(resource_filename);
  }

  // Registers interception of requests for |url| to respond with the contents
  // of the file at |response_path|.
  void RegisterInterception(const GURL& url,
                            const base::FilePath& response_path) {
    ASSERT_TRUE(base::PathExists(response_path));
    ASSERT_FALSE(interceptor_);
    interceptor_ = std::make_unique<net::TestURLRequestInterceptor>(
        url.scheme(), url.host(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO),
        base::CreateTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND}));
    interceptor_->SetResponse(url, response_path);
  }

 private:
  net::URLRequestContextGetter* request_context() {
    return request_context_.get();
  }

  // Helper to get files from our subdirectory in the general extensions test
  // data dir.
  base::FilePath GetTestPath(const base::FilePath& relative_path) {
    base::FilePath base_path;
    EXPECT_TRUE(PathService::Get(extensions::DIR_TEST_DATA, &base_path));
    base_path = base_path.AppendASCII("content_hash_fetcher");
    return base_path.Append(relative_path);
  }

  // Unzips the extension source from |extension_zip| into a temporary
  // directory and loads it, returning the resuling Extension object.
  scoped_refptr<Extension> UnzipToTempDirAndLoad(
      const base::FilePath& extension_zip) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath destination = temp_dir_.GetPath();
    EXPECT_TRUE(zip::Unzip(extension_zip, destination));

    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        destination, Manifest::INTERNAL, 0 /* flags */, &error);
    EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
    return extension;
  }

  std::unique_ptr<net::TestURLRequestInterceptor> interceptor_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  base::ScopedTempDir temp_dir_;

  GURL fetch_url_;
  base::FilePath test_dir_base_;
  std::unique_ptr<MockContentVerifierDelegate> delegate_;
  scoped_refptr<Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcherTest);
};

// This tests our ability to successfully fetch, parse, and validate a missing
// verified_contents.json file for an extension.
TEST_F(ContentHashFetcherTest, MissingVerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  RegisterInterception(fetch_url(), GetResourcePath("verified_contents.json"));

  // Make sure the fetch was successful.
  std::unique_ptr<ContentHashFetcherResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(VerifiedContentsFileExists());
}

// Tests that if the network fetches invalid verified_contents.json, failure
// happens correctly.
TEST_F(ContentHashFetcherTest, FetchInvalidVerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  // Simulate invalid verified_contents.json fetch by providing a modified and
  // incorrect json file.
  // invalid_verified_contents.json is a modified version of
  // verified_contents.json, with one hash character garbled.
  RegisterInterception(fetch_url(),
                       GetResourcePath("invalid_verified_contents.json"));

  std::unique_ptr<ContentHashFetcherResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_FALSE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // TODO(lazyboy): This should be EXPECT_FALSE, we shouldn't be writing
  // verified_contents.json file if it didn't succeed.
  //// Make sure the verified_contents.json file was *not* written into the
  //// extension's install dir.
  // EXPECT_FALSE(VerifiedContentsFileExists());
  EXPECT_TRUE(VerifiedContentsFileExists());
}

// Tests that if the verified_contents.json network request 404s, failure
// happens as expected.
TEST_F(ContentHashFetcherTest, Fetch404VerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  // NOTE: No RegisterInterception(), hash fetch will result in 404.

  // Make sure the fetch was *not* successful.
  std::unique_ptr<ContentHashFetcherResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_FALSE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // Make sure the verified_contents.json file was *not* written into the
  // extension's install dir.
  EXPECT_FALSE(VerifiedContentsFileExists());
}

// Similar to MissingVerifiedContents, but tests the case where the extension
// actually has corruption.
TEST_F(ContentHashFetcherTest, MissingVerifiedContentsAndCorrupt) {
  ASSERT_TRUE(LoadTestExtension());

  // Tamper with a file in the extension.
  base::FilePath script_path = extension_root().AppendASCII("script.js");
  std::string addition = "//hello world";
  ASSERT_TRUE(
      base::AppendToFile(script_path, addition.c_str(), addition.size()));

  RegisterInterception(fetch_url(), GetResourcePath("verified_contents.json"));

  // Make sure the fetch was *not* successful.
  std::unique_ptr<ContentHashFetcherResult> result = DoHashFetch();
  ASSERT_NE(nullptr, result.get());
  EXPECT_TRUE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(
      base::ContainsKey(result->mismatch_paths, script_path.BaseName()));

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(VerifiedContentsFileExists());
}

}  // namespace extensions
