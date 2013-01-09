// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_data_directory.h"

#include "base/base_paths.h"
#include "base/path_service.h"

namespace net {

namespace {
const FilePath::CharType kCertificateRelativePath[] =
    FILE_PATH_LITERAL("net/data/ssl/certificates");
}  // namespace

FilePath GetTestCertsDirectory() {
  FilePath src_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  return src_root.Append(kCertificateRelativePath);
}

FilePath GetTestCertsDirectoryRelative() {
  return FilePath(kCertificateRelativePath);
}

FilePath GetWebSocketTestDataDirectory() {
  FilePath data_dir(FILE_PATH_LITERAL("net/data/websocket"));
  return data_dir;
}

}  // namespace net
