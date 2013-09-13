// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_UTIL_H_
#define CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/common/media_galleries/pmp_constants.h"

namespace picasa {

namespace PmpTestUtil {

bool WriteIndicatorFile(
    const base::FilePath& column_file_destination,
    const std::string& table_name);

template<class T>
bool WriteColumnFileFromVector(
    const base::FilePath& column_file_destination,
    const std::string& table_name,
    const std::string& column_name,
    const PmpFieldType field_type,
    const std::vector<T>& elements_vector);

std::vector<char> MakeHeader(const PmpFieldType field_type,
                             const uint32 row_count);

template<class T>
std::vector<char> MakeHeaderAndBody(const PmpFieldType field_type,
                                    const uint32 row_count,
                                    const std::vector<T>& elems);

}  // namespace PmpTestUtil

}  // namespace picasa

#endif  // CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_UTIL_H_
