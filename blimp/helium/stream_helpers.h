// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_STREAM_HELPERS_H_
#define BLIMP_HELIUM_STREAM_HELPERS_H_

#include <map>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;
class CodedOutputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace blimp {
namespace helium {

class VersionVector;

void BLIMP_HELIUM_EXPORT
WriteToStream(const int32_t& value,
              google::protobuf::io::CodedOutputStream* output_stream);
bool BLIMP_HELIUM_EXPORT
ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
               int32_t* value);

void BLIMP_HELIUM_EXPORT
WriteToStream(const std::string& value,
              google::protobuf::io::CodedOutputStream* output_stream);
bool BLIMP_HELIUM_EXPORT
ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
               std::string* value);

void BLIMP_HELIUM_EXPORT
WriteToStream(const VersionVector& value,
              google::protobuf::io::CodedOutputStream* output_stream);
bool BLIMP_HELIUM_EXPORT
ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
               VersionVector* value);

void BLIMP_HELIUM_EXPORT
WriteToStream(const uint64_t& value,
              google::protobuf::io::CodedOutputStream* output_stream);
bool BLIMP_HELIUM_EXPORT
ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
               uint64_t* value);

template <typename KeyType, typename ValueType>
void WriteToStream(const std::map<KeyType, ValueType> value_map,
                   google::protobuf::io::CodedOutputStream* output_stream);

template <typename KeyType, typename ValueType>
bool ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
                    std::map<KeyType, ValueType>* value_map);

template <typename KeyType, typename ValueType>
void WriteToStream(const std::map<KeyType, ValueType> value_map,
                   google::protobuf::io::CodedOutputStream* output_stream) {
  output_stream->WriteVarint32(value_map.size());
  for (const auto& kv_pair : value_map) {
    WriteToStream(kv_pair.first, output_stream);
    WriteToStream(kv_pair.second, output_stream);
  }
}

template <typename KeyType, typename ValueType>
bool ReadFromStream(google::protobuf::io::CodedInputStream* input_stream,
                    std::map<KeyType, ValueType>* value_map) {
  std::map<KeyType, ValueType> output;

  uint32_t num_entries;
  if (!input_stream->ReadVarint32(&num_entries)) {
    return false;
  }

  output.clear();
  for (uint32_t i = 0; i < num_entries; ++i) {
    KeyType key;
    ValueType value;

    if (!ReadFromStream(input_stream, &key) ||
        !ReadFromStream(input_stream, &value) ||
        base::ContainsKey(output, key)) {
      return false;
    }

    output.insert(std::make_pair(std::move(key), std::move(value)));
  }

  *value_map = std::move(output);
  return true;
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_STREAM_HELPERS_H_
