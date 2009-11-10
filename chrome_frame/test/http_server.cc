// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/http_server.h"

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";

void ChromeFrameHTTPServer::SetUp() {
  std::wstring document_root(kDocRoot);
  server_ = HTTPTestServer::CreateServer(document_root, NULL, 30, 1000);
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

GURL ChromeFrameHTTPServer::Resolve(const wchar_t* relative_url) {
  return server_->TestServerPageW(relative_url);
}

std::wstring ChromeFrameHTTPServer::GetDataDir() {
  return server_->GetDataDirectory().ToWStringHack();
}

