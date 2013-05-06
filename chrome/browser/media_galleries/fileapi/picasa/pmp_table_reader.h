// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PMP_TABLE_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PMP_TABLE_READER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"

namespace base {
class FilePath;
}

namespace picasa {

class PmpColumnReader;

class PmpTableReader {
 public:
  PmpTableReader(const std::string& table_name,
                 const base::FilePath& directory_path);

  virtual ~PmpTableReader();

  // Returns NULL on failure.
  const PmpColumnReader* AddColumn(const std::string& column_name,
                                   const PmpFieldType expected_type);

  // Returns a const "view" of the successfully added columns.
  std::map<std::string, const PmpColumnReader*> Columns() const;

  // This value may change after calls to AddColumn().
  uint32 RowCount() const;

 private:
  bool initialized_;

  std::string table_name_;
  base::FilePath directory_path_;

  ScopedVector<PmpColumnReader> column_readers_;
  std::map<std::string, const PmpColumnReader*> column_map_;

  uint32 max_row_count_;

  DISALLOW_COPY_AND_ASSIGN(PmpTableReader);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PMP_TABLE_READER_H_
