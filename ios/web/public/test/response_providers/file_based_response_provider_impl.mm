// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/response_providers/file_based_response_provider_impl.h"

#include "base/files/file_util.h"
#include "ios/web/public/test/response_providers/response_provider.h"
#include "url/gurl.h"

namespace web {

FileBasedResponseProviderImpl::FileBasedResponseProviderImpl(
    const base::FilePath& path)
    : path_(path) {}

FileBasedResponseProviderImpl::~FileBasedResponseProviderImpl() {}

bool FileBasedResponseProviderImpl::CanHandleRequest(
    const ResponseProvider::Request& request) {
  return base::PathExists(BuildTargetPath(request.url));
}

base::FilePath FileBasedResponseProviderImpl::BuildTargetPath(const GURL& url) {
  base::FilePath result = path_;
  const std::string kLocalhostHost = "localhost";
  if (url.host() != kLocalhostHost) {
    result = result.Append(url.host());
  }
  std::string url_path = url.path();
  // Remove the leading slash in url path.
  if (url_path.length() > 0 && url_path[0] == '/') {
    url_path.erase(0, 1);
  }
  if (!url_path.empty())
    result = result.Append(url_path);
  return result;
}

}  // namespace web
