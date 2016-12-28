// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/response_providers/file_based_response_provider.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Responses/GCDWebServerFileResponse.h"

namespace web {

FileBasedResponseProvider::FileBasedResponseProvider(const base::FilePath& path)
    : response_provider_impl_(new FileBasedResponseProviderImpl(path)) {
}

FileBasedResponseProvider::~FileBasedResponseProvider() {
}

bool FileBasedResponseProvider::CanHandleRequest(const Request& request) {
  return response_provider_impl_->CanHandleRequest(request);
}

GCDWebServerResponse* FileBasedResponseProvider::GetGCDWebServerResponse(
    const Request& request) {
  const base::FilePath file_path =
      response_provider_impl_->BuildTargetPath(request.url);
  NSString* path = base::SysUTF8ToNSString(file_path.value());
  return [GCDWebServerFileResponse responseWithFile:path];
}

}  // namespace web
