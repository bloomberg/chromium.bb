// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_
#define CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_

#include <vector>

#include "content/public/common/cdm_info.h"

namespace chrome {

// Gets a list of CDM host file paths and put them in |cdm_host_file_paths|.
void AddCdmHostFilePaths(
    std::vector<content::CdmHostFilePath>* cdm_host_file_paths);

}  // namespace chrome

#endif  // CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_
