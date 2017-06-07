// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/guid.h"
#include "components/download/internal/test/download_params_utils.h"

namespace download {
namespace test {

DownloadParams BuildBasicDownloadParams() {
  DownloadParams params;
  params.client = DownloadClient::TEST;
  params.guid = base::GenerateGUID();
  return params;
}

}  // namespace test
}  // namespace download
