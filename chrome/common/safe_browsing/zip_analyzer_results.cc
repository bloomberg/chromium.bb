// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zip file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#include "chrome/common/safe_browsing/zip_analyzer_results.h"

namespace safe_browsing {
namespace zip_analyzer {

Results::Results() : success(false), has_executable(false), has_archive(false) {
}

Results::~Results() {
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
