// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/pmp_test_util.h"

#include <algorithm>
#include <iterator>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace picasa {

namespace {

// Flatten a vector of elements into an array of bytes.
template<class T>
std::vector<char> Flatten(const std::vector<T>& elems) {
  if (elems.empty())
    return std::vector<char>();

  const uint8* elems0 = reinterpret_cast<const uint8*>(&elems[0]);
  std::vector<char> data_body(elems0, elems0 + sizeof(T) * elems.size());
  return data_body;
}

// Custom specialization for std::string.
template<>
std::vector<char> Flatten(const std::vector<std::string>& strings) {
  std::vector<char> totalchars;

  for (std::vector<std::string>::const_iterator it = strings.begin();
      it != strings.end(); ++it) {
    std::copy(it->begin(), it->end(), std::back_inserter(totalchars));
    totalchars.push_back('\0');  // Add the null termination too.
  }

  return totalchars;
}

// Returns a new vector with the concatenated contents of |a| and |b|.
std::vector<char> CombinedVectors(const std::vector<char>& a,
                                  const std::vector<char>& b) {
  std::vector<char> total;

  std::copy(a.begin(), a.end(), std::back_inserter(total));
  std::copy(b.begin(), b.end(), std::back_inserter(total));

  return total;
}

}  // namespace

bool PmpTestUtil::WriteIndicatorFile(
    const base::FilePath& column_file_destination,
    const std::string& table_name) {
  base::FilePath indicator_path = column_file_destination.Append(
      base::FilePath::FromUTF8Unsafe(table_name + "_0"));

  return base::WriteFile(indicator_path, NULL, 0) == 0;
}

template<class T>
bool PmpTestUtil::WriteColumnFileFromVector(
    const base::FilePath& column_file_destination,
    const std::string& table_name,
    const std::string& column_name,
    const PmpFieldType field_type,
    const std::vector<T>& elements_vector) {
  std::string file_name = table_name + "_" + column_name + "." + kPmpExtension;

  base::FilePath path = column_file_destination.AppendASCII(file_name);

  std::vector<char> data = PmpTestUtil::MakeHeaderAndBody(
      field_type, elements_vector.size(), elements_vector);

  size_t bytes_written = base::WriteFile(path, &data[0], data.size());
  return (bytes_written == data.size());
}

// Explicit Instantiation for all the valid types.
template bool PmpTestUtil::WriteColumnFileFromVector<std::string>(
    const base::FilePath&, const std::string&, const std::string&,
    const PmpFieldType, const std::vector<std::string>&);
template bool PmpTestUtil::WriteColumnFileFromVector<uint32>(
    const base::FilePath&, const std::string&, const std::string&,
    const PmpFieldType, const std::vector<uint32>&);
template bool PmpTestUtil::WriteColumnFileFromVector<double>(
    const base::FilePath&, const std::string&, const std::string&,
    const PmpFieldType, const std::vector<double>&);
template bool PmpTestUtil::WriteColumnFileFromVector<uint8>(
    const base::FilePath&, const std::string&, const std::string&,
    const PmpFieldType, const std::vector<uint8>&);
template bool PmpTestUtil::WriteColumnFileFromVector<uint64>(
    const base::FilePath&, const std::string&, const std::string&,
    const PmpFieldType, const std::vector<uint64>&);

// Return a vector so we don't have to worry about memory management.
std::vector<char> PmpTestUtil::MakeHeader(const PmpFieldType field_type,
                                            const uint32 row_count) {
  std::vector<char> header(picasa::kPmpHeaderSize);

  // Copy in magic bytes.
  memcpy(&header[picasa::kPmpMagic1Offset], &picasa::kPmpMagic1,
         sizeof(picasa::kPmpMagic1));
  memcpy(&header[picasa::kPmpMagic2Offset], &picasa::kPmpMagic2,
         sizeof(picasa::kPmpMagic2));
  memcpy(&header[picasa::kPmpMagic3Offset], &picasa::kPmpMagic3,
         sizeof(picasa::kPmpMagic3));
  memcpy(&header[picasa::kPmpMagic4Offset], &picasa::kPmpMagic4,
         sizeof(picasa::kPmpMagic4));

  // Copy in field type.
  uint16 field_type_short = static_cast<uint16>(field_type);
  memcpy(&header[picasa::kPmpFieldType1Offset], &field_type_short,
         sizeof(uint16));
  memcpy(&header[picasa::kPmpFieldType2Offset], &field_type_short,
         sizeof(uint16));

  // Copy in row count.
  memcpy(&header[picasa::kPmpRowCountOffset], &row_count, sizeof(uint32));

  return header;
}

template<class T>
std::vector<char> PmpTestUtil::MakeHeaderAndBody(
    const PmpFieldType field_type, const uint32 row_count,
    const std::vector<T>& elems) {
  return CombinedVectors(PmpTestUtil::MakeHeader(field_type, row_count),
                         Flatten(elems));
}

// Explicit Instantiation for all the valid types.
template std::vector<char> PmpTestUtil::MakeHeaderAndBody<std::string>(
    const PmpFieldType, const uint32, const std::vector<std::string>&);
template std::vector<char> PmpTestUtil::MakeHeaderAndBody<uint32>(
    const PmpFieldType, const uint32, const std::vector<uint32>&);
template std::vector<char> PmpTestUtil::MakeHeaderAndBody<double>(
    const PmpFieldType, const uint32, const std::vector<double>&);
template std::vector<char> PmpTestUtil::MakeHeaderAndBody<uint8>(
    const PmpFieldType, const uint32, const std::vector<uint8>&);
template std::vector<char> PmpTestUtil::MakeHeaderAndBody<uint64>(
    const PmpFieldType, const uint32, const std::vector<uint64>&);

}  // namespace picasa
