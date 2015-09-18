// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/test_package_manager.h"

#include "base/logging.h"
#include "mojo/shell/fetcher.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

TestPackageManager::TestPackageManager() {}
TestPackageManager::~TestPackageManager() {}

void TestPackageManager::RegisterContentHandler(
    const std::string& mime_type,
    const GURL& content_handler_url) {
  DCHECK(content_handler_url.is_valid())
      << "Content handler URL is invalid for mime type " << mime_type;
  mime_type_to_url_[mime_type] = content_handler_url;
}

void TestPackageManager::SetApplicationManager(ApplicationManager* manager) {
}

GURL TestPackageManager::ResolveURL(const GURL& url) {
  return url;
}

void TestPackageManager::FetchRequest(
    URLRequestPtr request,
    const Fetcher::FetchCallback& loader_callback) {}

bool TestPackageManager::HandleWithContentHandler(
    Fetcher* fetcher,
    const GURL& unresolved_url,
    base::TaskRunner* task_runner,
    URLResponsePtr* new_response,
    GURL* content_handler_url,
    std::string* qualifier) {
  MimeTypeToURLMap::iterator iter = mime_type_to_url_.find(fetcher->MimeType());
  if (iter != mime_type_to_url_.end()) {
    *new_response = fetcher->AsURLResponse(task_runner, 0);
    *content_handler_url = iter->second;
    *qualifier = (*new_response)->site.To<std::string>();
    return true;
  }
  return false;
}

}  // namespace shell
}  // namespace mojo
