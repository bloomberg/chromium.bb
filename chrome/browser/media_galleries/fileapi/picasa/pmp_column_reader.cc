// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/pmp_column_reader.h"

#include <cstring>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

namespace picasa {

namespace {

const size_t kPmpMaxFilesize = 50*1024*1024; // Maximum of 50 MB.

}  // namespace

PmpColumnReader::PmpColumnReader()
    : length_(0),
      field_type_(PMP_TYPE_INVALID),
      rows_(0) {}

PmpColumnReader::~PmpColumnReader() {}

bool PmpColumnReader::Init(const base::FilePath& filepath,
                           const PmpFieldType expected_type,
                           uint32* rows_read) {
  DCHECK(!data_.get());
  base::ThreadRestrictions::AssertIOAllowed();

  int64 length = 0; // Signed temporary.
  if (!file_util::GetFileSize(filepath, &length))
    return false;

  length_ = length;

  if (length_ < kPmpHeaderSize || length_ > kPmpMaxFilesize)
    return false;

  data_.reset(new uint8[length_]);

  char* data_begin = reinterpret_cast<char*>(data_.get());

  return file_util::ReadFile(filepath, data_begin, length_) &&
         ParseData(expected_type, rows_read);
}

bool PmpColumnReader::ReadString(const uint32 row, std::string* result) const {
  DCHECK(data_.get() != NULL);
  DCHECK_GT(length_, kPmpHeaderSize + row);

  if (field_type_ != PMP_TYPE_STRING || row >= rows_)
    return false;

  *result = strings_[row];
  return true;
}

bool PmpColumnReader::ReadUInt32(const uint32 row, uint32* result) const {
  DCHECK(data_.get() != NULL);
  DCHECK_GT(length_, kPmpHeaderSize + row * sizeof(uint32));

  if (field_type_ != PMP_TYPE_UINT32 || row >= rows_)
    return false;

  *result = reinterpret_cast<uint32*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadDouble64(const uint32 row, double* result) const {
  DCHECK(data_.get() != NULL);
  DCHECK_GT(length_, kPmpHeaderSize + row * sizeof(double));

  if (field_type_ != PMP_TYPE_DOUBLE64 || row >= rows_)
    return false;

  *result = reinterpret_cast<double*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadUInt8(const uint32 row, uint8* result) const {
  DCHECK(data_.get() != NULL);
  DCHECK_GT(length_, kPmpHeaderSize + row * sizeof(uint8));

  if (field_type_ != PMP_TYPE_UINT8 || row >= rows_)
    return false;

  *result = reinterpret_cast<uint8*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadUInt64(const uint32 row, uint64* result) const {
  DCHECK(data_.get() != NULL);
  DCHECK_GT(length_, kPmpHeaderSize + row * sizeof(uint64));

  if (field_type_ != PMP_TYPE_UINT64 || row >= rows_)
    return false;

  *result = reinterpret_cast<uint64*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ParseData(const PmpFieldType expected_type,
                                uint32* rows_read) {
  DCHECK(data_.get() != NULL);
  DCHECK_GE(length_, kPmpHeaderSize);

  // Check all magic bytes.
  if (memcmp(&kPmpMagic1, &data_[kPmpMagic1Offset], sizeof(kPmpMagic1)) != 0 ||
      memcmp(&kPmpMagic2, &data_[kPmpMagic2Offset], sizeof(kPmpMagic2)) != 0 ||
      memcmp(&kPmpMagic3, &data_[kPmpMagic3Offset], sizeof(kPmpMagic3)) != 0 ||
      memcmp(&kPmpMagic4, &data_[kPmpMagic4Offset], sizeof(kPmpMagic4)) != 0) {
    return false;
  }

  uint16 field_type_data =
      *(reinterpret_cast<uint16*>(&data_[kPmpFieldType1Offset]));

  // Verify if field type matches second declaration
  if (field_type_data !=
      *(reinterpret_cast<uint16*>(&data_[kPmpFieldType2Offset]))) {
    return false;
  }

  field_type_ = static_cast<PmpFieldType>(field_type_data);

  if (field_type_ != expected_type)
    return false;

  rows_ = *(reinterpret_cast<uint32*>(&data_[kPmpRowCountOffset]));

  size_t body_length = length_ - kPmpHeaderSize;
  size_t expected_body_length = 0;
  switch (field_type_) {
    case PMP_TYPE_STRING:
      expected_body_length = IndexStrings();
      break;
    case PMP_TYPE_UINT32:
      expected_body_length = rows_ * sizeof(uint32);
      break;
    case PMP_TYPE_DOUBLE64:
      expected_body_length = rows_ * sizeof(double);
      break;
    case PMP_TYPE_UINT8:
      expected_body_length = rows_ * sizeof(uint8);
      break;
    case PMP_TYPE_UINT64:
      expected_body_length = rows_ * sizeof(uint64);
      break;
    default:
      return false;
      break;
  }

  if (body_length == expected_body_length && rows_read)
    *rows_read = rows_;
  return body_length == expected_body_length;
}

long PmpColumnReader::IndexStrings() {
  DCHECK(data_.get() != NULL);
  DCHECK_GE(length_, kPmpHeaderSize);

  strings_.reserve(rows_);

  size_t bytes_parsed = kPmpHeaderSize;
  const uint8* data_cursor = data_.get() + kPmpHeaderSize;

  while (strings_.size() < rows_) {
    const uint8* string_end = static_cast<const uint8*>(
        memchr(data_cursor, '\0', length_ - bytes_parsed));

    // Fail if cannot find null termination. String runs on past file end.
    if (string_end == NULL)
      return -1;

    // Length of string. (+1 to include the termination character).
    ptrdiff_t length_in_bytes = string_end - data_cursor + 1;

    strings_.push_back(reinterpret_cast<const char*>(data_cursor));
    data_cursor += length_in_bytes;
    bytes_parsed += length_in_bytes;
  }

  return bytes_parsed - kPmpHeaderSize;
}

}  // namespace picasa
