// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_APDU_COMMAND_H_
#define DEVICE_U2F_U2F_APDU_COMMAND_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"

namespace device {

// APDU commands are defined as part of ISO 7816-4. Commands can be serialized
// into either short length encodings, where the maximum data length is 255
// bytes, or an extended length encoding, where the maximum data length is 65536
// bytes. This class implements only the extended length encoding. Serialized
// commands consist of a CLA byte, denoting the class of instruction, an INS
// byte, denoting the instruction code, P1 and P2, each one byte denoting
// instruction parameters, a length field (Lc), a data field of length Lc, and
// a maximum expected response length (Le).
class U2fApduCommand : public base::RefCountedThreadSafe<U2fApduCommand> {
 public:
  // Construct an apdu command from the serialized message data
  static scoped_refptr<U2fApduCommand> CreateFromMessage(
      const std::vector<uint8_t>& data);
  // Create an empty apdu command object
  static scoped_refptr<U2fApduCommand> Create();
  // Returns serialized message data
  std::vector<uint8_t> GetEncodedCommand() const;
  void set_cla(uint8_t cla) { cla_ = cla; }
  void set_ins(uint8_t ins) { ins_ = ins; }
  void set_p1(uint8_t p1) { p1_ = p1; }
  void set_p2(uint8_t p2) { p2_ = p2; }
  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void set_response_length(size_t response_length) {
    response_length_ = response_length;
  }

 private:
  friend class base::RefCountedThreadSafe<U2fApduCommand>;
  friend class U2fApduBuilder;
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestDeserializeBasic);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestDeserializeComplex);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestSerializeEdgeCases);

  static constexpr size_t kApduMinHeader = 4;
  static constexpr size_t kApduMaxHeader = 7;
  // As defined in ISO7816-4, extended length APDU request data is limited to
  // 16 bits in length with a maximum value of 65535. Response data length is
  // also limited to 16 bits in length with a value of 0x0000 corresponding to
  // a length of 65536
  static constexpr size_t kApduMaxDataLength = 65535;
  static constexpr size_t kApduMaxResponseLength = 65536;
  static constexpr size_t kApduMaxLength =
      kApduMaxDataLength + kApduMaxHeader + 2;

  U2fApduCommand();
  U2fApduCommand(uint8_t cla,
                 uint8_t ins,
                 uint8_t p1,
                 uint8_t p2,
                 size_t response_length,
                 std::vector<uint8_t> data);
  ~U2fApduCommand();

  uint8_t cla_;
  uint8_t ins_;
  uint8_t p1_;
  uint8_t p2_;
  size_t response_length_;
  std::vector<uint8_t> data_;
};
}  // namespace device

#endif  // DEVICE_U2F_U2F_APDU_COMMAND_H_
