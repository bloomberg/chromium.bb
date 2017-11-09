// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_APDU_COMMAND_H_
#define DEVICE_U2F_U2F_APDU_COMMAND_H_

#include <cinttypes>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"

namespace device {

// APDU commands are defined as part of ISO 7816-4. Commands can be serialized
// into either short length encodings, where the maximum data length is 256
// bytes, or an extended length encoding, where the maximum data length is 65536
// bytes. This class implements only the extended length encoding. Serialized
// commands consist of a CLA byte, denoting the class of instruction, an INS
// byte, denoting the instruction code, P1 and P2, each one byte denoting
// instruction parameters, a length field (Lc), a data field of length Lc, and
// a maximum expected response length (Le).
class U2fApduCommand {
 public:
  U2fApduCommand();
  U2fApduCommand(uint8_t cla,
                 uint8_t ins,
                 uint8_t p1,
                 uint8_t p2,
                 size_t response_length,
                 std::vector<uint8_t> data,
                 std::vector<uint8_t> suffix);
  ~U2fApduCommand();

  // Constructs an APDU command from the serialized message data.
  static std::unique_ptr<U2fApduCommand> CreateFromMessage(
      const std::vector<uint8_t>& data);
  // Returns serialized message data.
  std::vector<uint8_t> GetEncodedCommand() const;
  void set_cla(uint8_t cla) { cla_ = cla; }
  void set_ins(uint8_t ins) { ins_ = ins; }
  void set_p1(uint8_t p1) { p1_ = p1; }
  void set_p2(uint8_t p2) { p2_ = p2; }
  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void set_response_length(size_t response_length) {
    response_length_ = response_length;
  }
  void set_suffix(const std::vector<uint8_t>& suffix) { suffix_ = suffix; }
  static std::unique_ptr<U2fApduCommand> CreateRegister(
      const std::vector<uint8_t>& appid_digest,
      const std::vector<uint8_t>& challenge_digest);
  static std::unique_ptr<U2fApduCommand> CreateVersion();
  // Early U2F drafts defined a non-ISO 7816-4 conforming layout.
  static std::unique_ptr<U2fApduCommand> CreateLegacyVersion();

  // Returns an APDU command for sign(). If optional parameter |check_only| is
  // set to true, then control byte is set to 0X07.
  static std::unique_ptr<U2fApduCommand> CreateSign(
      const std::vector<uint8_t>& appid_digest,
      const std::vector<uint8_t>& challenge_digest,
      const std::vector<uint8_t>& key_handle,
      bool check_only = false);

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestDeserializeBasic);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestDeserializeComplex);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestSerializeEdgeCases);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestCreateSign);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestCreateRegister);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestCreateVersion);
  FRIEND_TEST_ALL_PREFIXES(U2fApduTest, TestCreateLegacyVersion);

  static constexpr size_t kApduMinHeader = 4;
  static constexpr size_t kApduMaxHeader = 7;
  // As defined in ISO7816-4, extended length APDU request data is limited to
  // 16 bits in length with a maximum value of 65535. Response data length is
  // also limited to 16 bits in length with a value of 0x0000 corresponding to
  // a length of 65536.
  static constexpr size_t kApduMaxDataLength = 65535;
  static constexpr size_t kApduMaxResponseLength = 65536;
  static constexpr size_t kApduMaxLength =
      kApduMaxDataLength + kApduMaxHeader + 2;

  // APDU instructions.
  static constexpr uint8_t kInsU2fEnroll = 0x01;
  static constexpr uint8_t kInsU2fSign = 0x02;
  static constexpr uint8_t kInsU2fVersion = 0x03;

  // P1 instructions.
  static constexpr uint8_t kP1TupRequired = 0x01;
  static constexpr uint8_t kP1TupConsumed = 0x02;
  static constexpr uint8_t kP1TupRequiredConsumed =
      kP1TupRequired | kP1TupConsumed;
  // Control byte used for check-only setting. The check-only command is used to
  // determine if the provided key handle was originally created by this token
  // and whether it was created for the provided application parameter.
  static constexpr uint8_t kP1CheckOnly = 0x07;
  static constexpr size_t kMaxKeyHandleLength = 255;
  static constexpr size_t kChallengeDigestLen = 32;
  static constexpr size_t kAppIdDigestLen = 32;

  uint8_t cla_;
  uint8_t ins_;
  uint8_t p1_;
  uint8_t p2_;
  size_t response_length_;
  std::vector<uint8_t> data_;
  std::vector<uint8_t> suffix_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_APDU_COMMAND_H_
