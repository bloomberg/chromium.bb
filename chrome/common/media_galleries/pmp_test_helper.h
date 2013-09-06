// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_HELPER_H_
#define CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/common/media_galleries/pmp_constants.h"

namespace picasa {

class PmpColumnReader;

// A helper class used for tests only
class PmpTestHelper {
 public:
  enum ColumnFileDestination {
    DATABASE_DIRECTORY,
    TEMPORARY_DIRECTORY
  };

  explicit PmpTestHelper(const std::string& table_name);

  bool Init(ColumnFileDestination column_file_destination);

  base::FilePath GetDatabaseDirPath() const;
  base::FilePath GetTempDirPath() const;

  template<class T>
  bool WriteColumnFileFromVector(const std::string& column_name,
                                 const PmpFieldType field_type,
                                 const std::vector<T>& elements_vector) const;

  static std::vector<char> MakeHeader(const PmpFieldType field_type,
                                       const uint32 row_count);

  template<class T>
  static std::vector<char> MakeHeaderAndBody(const PmpFieldType field_type,
                                             const uint32 row_count,
                                             const std::vector<T>& elems);

 private:
  std::string table_name_;
  base::ScopedTempDir root_dir_;
  base::FilePath column_file_destination_directory_;
};

}  // namespace picasa

#endif  // CHROME_COMMON_MEDIA_GALLERIES_PMP_TEST_HELPER_H_
