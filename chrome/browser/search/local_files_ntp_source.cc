// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_files_ntp_source.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

namespace {

const char kBasePath[] = "chrome/browser/resources/local_ntp";

void CallbackWithLoadedResource(
    const std::string& origin,
    const content::URLDataSource::GotDataCallback& callback,
    const std::string& content) {
  std::string output = content;
  if (!origin.empty())
    base::ReplaceFirstSubstringAfterOffset(&output, 0, "{{ORIGIN}}", origin);

  scoped_refptr<base::RefCountedString> response(
      base::RefCountedString::TakeString(&output));
  callback.Run(response.get());
}

// Read a file to a string and return.
std::string ReadFileAndReturn(const base::FilePath& path) {
  std::string data;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string.
  base::ReadFileToString(path, &data);
  return data;
}

}  // namespace

namespace local_ntp {

void SendLocalFileResource(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  SendLocalFileResourceWithOrigin(path, std::string(), callback);
}

void SendLocalFileResourceWithOrigin(
    const std::string& path,
    const std::string& origin,
    const content::URLDataSource::GotDataCallback& callback) {
  base::FilePath fullpath;
  PathService::Get(base::DIR_SOURCE_ROOT, &fullpath);
  fullpath = fullpath.AppendASCII(kBasePath).AppendASCII(path);
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&ReadFileAndReturn, fullpath),
      base::Bind(&CallbackWithLoadedResource, origin, callback));
}

}  // namespace local_ntp
