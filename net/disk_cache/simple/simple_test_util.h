// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
}

namespace disk_cache {

namespace simple_util {

// Creates a corrupt file to be used in tests.
bool CreateCorruptFileForTests(const std::string& key,
                               const base::FilePath& cache_path);

}  // namespace simple_backend

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_
