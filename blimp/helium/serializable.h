// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SERIALIZABLE_H_
#define BLIMP_HELIUM_SERIALIZABLE_H_

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

// Defines methods needed to serialize and deserialize |this| from a byte
// stream.
class Serializable {
 public:
  virtual void Serialize(
      google::protobuf::io::CodedOutputStream* output_stream) const = 0;
  virtual bool Parse(google::protobuf::io::CodedInputStream* input_stream) = 0;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SERIALIZABLE_H_
