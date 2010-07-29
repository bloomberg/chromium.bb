// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/http_server.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";

void ChromeFrameHTTPServer::SetUp() {
  std::wstring document_root(kDocRoot);
  server_ = net::HTTPTestServer::CreateServer(document_root);
  ASSERT_TRUE(server_ != NULL);

  // copy CFInstance.js into the test directory
  FilePath cf_source_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &cf_source_path);
  cf_source_path = cf_source_path.Append(FILE_PATH_LITERAL("chrome_frame"));

  file_util::CopyFile(cf_source_path.Append(FILE_PATH_LITERAL("CFInstance.js")),
      cf_source_path.Append(
          FILE_PATH_LITERAL("test")).Append(
              FILE_PATH_LITERAL("data")).Append(
                  FILE_PATH_LITERAL("CFInstance.js")));  // NOLINT

  file_util::CopyFile(cf_source_path.Append(FILE_PATH_LITERAL("CFInstall.js")),
      cf_source_path.Append(
          FILE_PATH_LITERAL("test")).Append(
              FILE_PATH_LITERAL("data")).Append(
                  FILE_PATH_LITERAL("CFInstall.js")));  // NOLINT
}

void ChromeFrameHTTPServer::TearDown() {
  if (server_) {
    server_ = NULL;
  }

  // clobber CFInstance.js
  FilePath cfi_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &cfi_path);
  cfi_path = cfi_path
      .Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("CFInstance.js"));

  file_util::Delete(cfi_path, false);

  cfi_path.empty();
  PathService::Get(base::DIR_SOURCE_ROOT, &cfi_path);
  cfi_path = cfi_path
      .Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("CFInstall.js"));

  file_util::Delete(cfi_path, false);
}

bool ChromeFrameHTTPServer::WaitToFinish(int milliseconds) {
  if (!server_)
    return true;

  return server_->WaitToFinish(milliseconds);
}

// TODO(phajdan.jr): Change wchar_t* to std::string& and fix callers.
GURL ChromeFrameHTTPServer::Resolve(const wchar_t* relative_url) {
  return server_->TestServerPage(WideToUTF8(relative_url));
}

FilePath ChromeFrameHTTPServer::GetDataDir() {
  return server_->GetDataDirectory();
}
