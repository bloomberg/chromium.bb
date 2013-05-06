// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/pmp_table_reader.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_column_reader.h"

namespace picasa {

namespace {

COMPILE_ASSERT(sizeof(double) == 8, double_must_be_8_bytes_long);

}  // namespace

PmpTableReader::PmpTableReader(const std::string& table_name,
                               const base::FilePath& directory_path)
    : initialized_(false),
      table_name_(table_name),
      directory_path_(directory_path),
      max_row_count_(0) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!file_util::DirectoryExists(directory_path_))
    return;

  std::string indicator_file_name = table_name_ + "_0";

  base::FilePath indicator_file = directory_path_.Append(
      base::FilePath::FromUTF8Unsafe(indicator_file_name));

  // Look for the indicator_file file, indicating table existence.
  initialized_ = file_util::PathExists(indicator_file) &&
                 !file_util::DirectoryExists(indicator_file);
}

PmpTableReader::~PmpTableReader() {}

const PmpColumnReader* PmpTableReader::AddColumn(
    const std::string& column_name, const PmpFieldType expected_type) {
  if (!initialized_)
    return NULL;

  std::string filename = table_name_ + "_" + column_name + "." + kPmpExtension;

  base::FilePath column_file_path = directory_path_.Append(
        base::FilePath::FromUTF8Unsafe(filename));
  scoped_ptr<PmpColumnReader> column_reader(new PmpColumnReader());

  uint32 row_count;
  if (!column_reader->Init(column_file_path, expected_type, &row_count))
    return NULL;

  column_readers_.push_back(column_reader.release());
  column_map_[column_name] = column_readers_.back();

  max_row_count_ = std::max(max_row_count_, row_count);

  return column_readers_.back();
}

uint32 PmpTableReader::RowCount() const {
  return max_row_count_;
}

std::map<std::string, const PmpColumnReader*> PmpTableReader::Columns() const {
  return column_map_;
}

}  // namespace picasa
