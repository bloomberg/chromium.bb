// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/test_utils.h"

#include "base/strings/stringprintf.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

TestContentVerifyJobObserver::TestContentVerifyJobObserver(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path)
    : extension_id_(extension_id), relative_path_(relative_path) {
  ContentVerifyJob::SetObserverForTests(this);
}

TestContentVerifyJobObserver::~TestContentVerifyJobObserver() {
  ContentVerifyJob::SetObserverForTests(nullptr);
}

void TestContentVerifyJobObserver::JobFinished(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    ContentVerifyJob::FailureReason reason) {
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(failure_reason_.has_value());
  failure_reason_ = reason;
  job_finished_run_loop_.Quit();
}

void TestContentVerifyJobObserver::OnHashesReady(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    bool success) {
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(seen_on_hashes_ready_);
  seen_on_hashes_ready_ = true;
  on_hashes_ready_run_loop_.Quit();
}

ContentVerifyJob::FailureReason
TestContentVerifyJobObserver::WaitForJobFinished() {
  // Run() returns immediately if Quit() has already been called.
  job_finished_run_loop_.Run();
  EXPECT_TRUE(failure_reason_.has_value());
  return failure_reason_.value_or(ContentVerifyJob::FAILURE_REASON_MAX);
}

void TestContentVerifyJobObserver::WaitForOnHashesReady() {
  // Run() returns immediately if Quit() has already been called.
  on_hashes_ready_run_loop_.Run();
}

MockContentVerifierDelegate::MockContentVerifierDelegate() = default;
MockContentVerifierDelegate::~MockContentVerifierDelegate() = default;

ContentVerifierDelegate::Mode MockContentVerifierDelegate::ShouldBeVerified(
    const Extension& extension) {
  return ContentVerifierDelegate::ENFORCE_STRICT;
}

ContentVerifierKey MockContentVerifierDelegate::GetPublicKey() {
  return ContentVerifierKey(kWebstoreSignaturesPublicKey,
                            kWebstoreSignaturesPublicKeySize);
}

GURL MockContentVerifierDelegate::GetSignatureFetchUrl(
    const ExtensionId& extension_id,
    const base::Version& version) {
  std::string url =
      base::StringPrintf("http://localhost/getsignature?id=%s&version=%s",
                         extension_id.c_str(), version.GetString().c_str());
  return GURL(url);
}

std::set<base::FilePath> MockContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  ADD_FAILURE() << "Unexpected call for this test";
  return std::set<base::FilePath>();
}

void MockContentVerifierDelegate::VerifyFailed(
    const ExtensionId& extension_id,
    ContentVerifyJob::FailureReason reason) {
  ADD_FAILURE() << "Unexpected call for this test";
}

void MockContentVerifierDelegate::Shutdown() {}

namespace content_verifier_test_utils {

scoped_refptr<Extension> UnzipToDirAndLoadExtension(
    const base::FilePath& extension_zip,
    const base::FilePath& unzip_dir) {
  if (!zip::Unzip(extension_zip, unzip_dir)) {
    ADD_FAILURE() << "Failed to unzip path.";
    return nullptr;
  }
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      unzip_dir, Manifest::INTERNAL, 0 /* flags */, &error);
  EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
  return extension;
}

}  // namespace content_verifier_test_utils

}  // namespace extensions
