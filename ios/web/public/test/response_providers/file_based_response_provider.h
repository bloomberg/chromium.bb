// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#import "ios/web/public/test/response_providers/file_based_response_provider_impl.h"
#import "ios/web/public/test/response_providers/response_provider.h"

namespace base {
class FilePath;
}

namespace web {

// FileBasedResponseProvider tries to resolve URL as if it were a path relative
// to |path| on the filesystem.
class FileBasedResponseProvider : public ResponseProvider {
 public:
  explicit FileBasedResponseProvider(const base::FilePath& path);
  ~FileBasedResponseProvider() override;

  // web::ReponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  GCDWebServerResponse* GetGCDWebServerResponse(
      const Request& request) override;

 private:
  std::unique_ptr<FileBasedResponseProviderImpl> response_provider_impl_;
};
}

#endif  // IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_H_
