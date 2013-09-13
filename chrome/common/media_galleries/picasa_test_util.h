// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_PICASA_TEST_UTIL_H_
#define CHROME_COMMON_MEDIA_GALLERIES_PICASA_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace base {
class FilePath;
}

namespace picasa {

void WriteAlbumTable(const base::FilePath& column_file_destination,
                     const std::vector<uint32>& category_vector,
                     const std::vector<double>& date_vector,
                     const std::vector<std::string>& filename_vector,
                     const std::vector<std::string>& name_vector,
                     const std::vector<std::string>& token_vector,
                     const std::vector<std::string>& uid_vector);

// Writes a whole test table with two folders and two albums.
void WriteTestAlbumTable(const base::FilePath& column_file_destination,
                         const base::FilePath& test_folder_1_path,
                         const base::FilePath& test_folder_2_path);

void WriteTestAlbumsImagesIndex(const base::FilePath& test_folder_1_path,
                                const base::FilePath& test_folder_2_path);

}  // namespace picasa

#endif  // CHROME_COMMON_MEDIA_GALLERIES_PICASA_TEST_UTIL_H_
