// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_IMPL_H_
#define IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_IMPL_H_

#include "base/files/file_path.h"
#import "ios/web/public/test/response_providers/response_provider.h"

class GURL;

namespace web {

// FileBasedMockResponseProvider tries to resolve URL as if it were a path
// relative to |path| on the filesystem. Use |BuildTargetPath| to resolve URLs
// to paths on the filesystem.
class FileBasedResponseProviderImpl {
 public:
  explicit FileBasedResponseProviderImpl(const base::FilePath& path);
  virtual ~FileBasedResponseProviderImpl();

  // Returns true if the request's URL when converted to a path on the app
  // bundle has a file present.
  bool CanHandleRequest(const ResponseProvider::Request& request);
  // Converts |url| to a path on the app bundle. Used to resolve URLs on to the
  // app bundle.
  base::FilePath BuildTargetPath(const GURL& url);

 private:
  // The path according to which URLs are resolved from.
  base::FilePath path_;
};
}

#endif  // IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_FILE_BASED_RESPONSE_PROVIDER_IMPL_H_
