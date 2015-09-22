// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zip file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_RESULTS_H_
#define CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_RESULTS_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/common/safe_browsing/csd.pb.h"

namespace safe_browsing {
namespace zip_analyzer {

struct Results {
  bool success;
  bool has_executable;
  bool has_archive;
  google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>
      archived_binary;
  std::vector<base::FilePath> archived_archive_filenames;
  Results();
  ~Results();
};

}  // namespace zip_analyzer
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_RESULTS_H_
