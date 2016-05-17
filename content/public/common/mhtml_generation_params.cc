// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mhtml_generation_params.h"

#include "base/files/file_path.h"

namespace content {

MHTMLGenerationParams::MHTMLGenerationParams(const base::FilePath& file_path)
    : file_path(file_path) {}

}  // namespace content
