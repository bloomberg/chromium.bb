// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_TEST_TEST_UTILS_H_
#define MOJO_TEST_TEST_UTILS_H_

#include <string>

#include "base/files/file_path.h"

namespace mojo {
namespace test {

// Returns the path to the mojom js bindings file.
base::FilePath GetFilePathForJSResource(const std::string& path);

}  // namespace test
}  // namespace mojo

#endif  // MOJO_TEST_TEST_UTILS_H_
