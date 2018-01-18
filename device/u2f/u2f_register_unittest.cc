// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_register.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/cbor/cbor_writer.h"
#include "device/u2f/attestation_object.h"
#include "device/u2f/attested_credential_data.h"
#include "device/u2f/authenticator_data.h"
#include "device/u2f/ec_public_key.h"
#include "device/u2f/fido_attestation_statement.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/register_response_data.h"
#include "device/u2f/u2f_parsing_utils.h"
#include "device/u2f/u2f_response_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;

namespace {

constexpr char kTestRelyingPartyId[] = "google.com";

// EC public key encoded in COSE_Key format.
// x : F868CE3869605224CE1059C0047EF01B830F2AD93BE27A3211F44E894560E695
// y : 4E11538CABA2DF1CC1A6F250ED9F0C8B28B39DA44539DFABD46B589CD0E202E5
constexpr uint8_t kTestECPublicKeyCOSE[] = {
    // clang-format off
    0xA5,  // map(5)
      0x01, 0x02,  // kty: EC key type
      0x03, 0x26,  // alg: EC256 signature algorithm
      0x20, 0x01,  // crv: P-256 curve
      0x21,  // x-coordinate
      0x58, 0x20,  // bytes(32)
        0xF8, 0x68, 0xCE, 0x38, 0x69, 0x60, 0x52, 0x24, 0xCE, 0x10, 0x59, 0xC0,
        0x04, 0x7E, 0xF0, 0x1B, 0x83, 0x0F, 0x2A, 0xD9, 0x3B, 0xE2, 0x7A, 0x32,
        0x11, 0xF4, 0x4E, 0x89, 0x45, 0x60, 0xE6, 0x95,
      0x22,  // y-coordinate
      0x58, 0x20,  // bytes(32)
      0x4E, 0x11, 0x53, 0x8C, 0xAB, 0xA2, 0xDF, 0x1C, 0xC1, 0xA6, 0xF2, 0x50,
      0xED, 0x9F, 0x0C, 0x8B, 0x28, 0xB3, 0x9D, 0xA4, 0x45, 0x39, 0xDF, 0xAB,
      0xD4, 0x6B, 0x58, 0x9C, 0xD0, 0xE2, 0x02, 0xE5,
    // clang-format on
};

// The attested credential data, excluding the public key bytes. Append
// with kTestECPublicKeyCOSE to get the complete attestation data.
constexpr uint8_t kTestAttestedCredentialDataPrefix[] = {
    // clang-format off
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,  // 16-byte aaguid
    0x00, 0x40,  // 2-byte length
    0x89, 0xAF, 0xB5, 0x24, 0x91, 0x1C, 0x40, 0x2B, 0x7F, 0x74, 0x59, 0xC9,
    0xF2, 0x21, 0xAF, 0xE6, 0xE5, 0x56, 0x65, 0x85, 0x04, 0xE8, 0x5B, 0x49,
    0x4D, 0x07, 0x55, 0x55, 0xF4, 0x6A, 0xBC, 0x44, 0x7B, 0x15, 0xFC, 0x62,
    0x61, 0x90, 0xA5, 0xFE, 0xEB, 0xE5, 0x9F, 0x5E, 0xDC, 0x75, 0x32, 0x98,
    0x6F, 0x44, 0x69, 0xD7, 0xF6, 0x13, 0xEB, 0xAA, 0xEA, 0x33, 0xFB, 0xD5,
    0x8E, 0xBF, 0xC6, 0x09  // 64-byte key handle
    // clang-format on
};

// The authenticator data, excluding the attested credential data bytes. Append
// with attested credential data to get the complete authenticator data.
constexpr uint8_t kTestAuthenticatorDataPrefix[] = {
    // clang-format off
    // sha256 hash of kTestRelyingPartyId
    0xD4, 0xC9, 0xD9, 0x02, 0x73, 0x26, 0x27, 0x1A, 0x89, 0xCE, 0x51,
    0xFC, 0xAF, 0x32, 0x8E, 0xD6, 0x73, 0xF1, 0x7B, 0xE3, 0x34, 0x69,
    0xFF, 0x97, 0x9E, 0x8A, 0xB8, 0xDD, 0x50, 0x1E, 0x66, 0x4F,
    0x41,  // flags (TUP and AT bits set)
    0x00, 0x00, 0x00, 0x00  // counter
    // clang-format on
};

// The attestation statement, a CBOR-encoded byte array.
// Diagnostic notation:
// {"sig":
// h'3044022008C3F8DB6E29FD8B14D9DE1BD98E84072CB813385989AA2CA289395E0009B8B70 \
// 2202607B4F9AD05DE26F56F48B82569EAD8231A5A6C3A1448DEAAAF15C0EF29631A',
// "x5c": [h'3082024A30820132A0030201020204046C8822300D06092A864886F70D01010B0 \
// 500302E312C302A0603550403132359756269636F2055324620526F6F742043412053657269 \
// 616C203435373230303633313020170D3134303830313030303030305A180F3230353030393 \
// 0343030303030305A302C312A302806035504030C2159756269636F20553246204545205365 \
// 7269616C203234393138323332343737303059301306072A8648CE3D020106082A8648CE3D0 \
// 30107034200043CCAB92CCB97287EE8E639437E21FCD6B6F165B2D5A3F3DB131D31C16B742B \
// B476D8D1E99080EB546C9BBDF556E6210FD42785899E78CC589EBE310F6CDB9FF4A33B30393 \
// 02206092B0601040182C40A020415312E332E362E312E342E312E34313438322E312E323013 \
// 060B2B0601040182E51C020101040403020430300D06092A864886F70D01010B05000382010 \
// 1009F9B052248BC4CF42CC5991FCAABAC9B651BBE5BDCDC8EF0AD2C1C1FFB36D18715D42E78 \
// B249224F92C7E6E7A05C49F0E7E4C881BF2E94F45E4A21833D7456851D0F6C145A29540C874 \
// F3092C934B43D222B8962C0F410CEF1DB75892AF116B44A96F5D35ADEA3822FC7146F600438 \
// 5BCB69B65C99E7EB6919786703C0D8CD41E8F75CCA44AA8AB725AD8E799FF3A8696A6F1B265 \
// 6E631B1E40183C08FDA53FA4A8F85A05693944AE179A1339D002D15CABD810090EC722EF5DE \
// F9965A371D415D624B68A2707CAD97BCDD1785AF97E258F33DF56A031AA0356D8E8D5EBCADC \
// 74E071636C6B110ACE5CC9B90DFEACAE640FF1BB0F1FE5DB4EFF7A95F060733F5']}
constexpr uint8_t kU2fAttestationStatementCBOR[] = {
    // clang-format off
    0xA2,  // map(2)
      0x63,  // text(3)
        0x73, 0x69, 0x67,  // "sig"
      0x58, 0x46,  // bytes(70)
        0x30, 0x44, 0x02, 0x20, 0x08, 0xC3, 0xF8, 0xDB, 0x6E, 0x29, 0xFD, 0x8B,
        0x14, 0xD9, 0xDE, 0x1B, 0xD9, 0x8E, 0x84, 0x07, 0x2C, 0xB8, 0x13, 0x38,
        0x59, 0x89, 0xAA, 0x2C, 0xA2, 0x89, 0x39, 0x5E, 0x00, 0x09, 0xB8, 0xB7,
        0x02, 0x20, 0x26, 0x07, 0xB4, 0xF9, 0xAD, 0x05, 0xDE, 0x26, 0xF5, 0x6F,
        0x48, 0xB8, 0x25, 0x69, 0xEA, 0xD8, 0x23, 0x1A, 0x5A, 0x6C, 0x3A, 0x14,
        0x48, 0xDE, 0xAA, 0xAF, 0x15, 0xC0, 0xEF, 0x29, 0x63, 0x1A,
      0x63,  // text(3)
        0x78, 0x35, 0x63,  // "x5c"
      0x81,  // array(1)
        0x59, 0x02, 0x4E,  // bytes(590)
          0x30, 0x82, 0x02, 0x4A, 0x30, 0x82, 0x01, 0x32, 0xA0, 0x03, 0x02,
          0x01, 0x02, 0x02, 0x04, 0x04, 0x6C, 0x88, 0x22, 0x30, 0x0D, 0x06,
          0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05,
          0x00, 0x30, 0x2E, 0x31, 0x2C, 0x30, 0x2A, 0x06, 0x03, 0x55, 0x04,
          0x03, 0x13, 0x23, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6F, 0x20, 0x55,
          0x32, 0x46, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41, 0x20,
          0x53, 0x65, 0x72, 0x69, 0x61, 0x6C, 0x20, 0x34, 0x35, 0x37, 0x32,
          0x30, 0x30, 0x36, 0x33, 0x31, 0x30, 0x20, 0x17, 0x0D, 0x31, 0x34,
          0x30, 0x38, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5A,
          0x18, 0x0F, 0x32, 0x30, 0x35, 0x30, 0x30, 0x39, 0x30, 0x34, 0x30,
          0x30, 0x30, 0x30, 0x30, 0x30, 0x5A, 0x30, 0x2C, 0x31, 0x2A, 0x30,
          0x28, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x21, 0x59, 0x75, 0x62,
          0x69, 0x63, 0x6F, 0x20, 0x55, 0x32, 0x46, 0x20, 0x45, 0x45, 0x20,
          0x53, 0x65, 0x72, 0x69, 0x61, 0x6C, 0x20, 0x32, 0x34, 0x39, 0x31,
          0x38, 0x32, 0x33, 0x32, 0x34, 0x37, 0x37, 0x30, 0x30, 0x59, 0x30,
          0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06,
          0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42,
          0x00, 0x04, 0x3C, 0xCA, 0xB9, 0x2C, 0xCB, 0x97, 0x28, 0x7E, 0xE8,
          0xE6, 0x39, 0x43, 0x7E, 0x21, 0xFC, 0xD6, 0xB6, 0xF1, 0x65, 0xB2,
          0xD5, 0xA3, 0xF3, 0xDB, 0x13, 0x1D, 0x31, 0xC1, 0x6B, 0x74, 0x2B,
          0xB4, 0x76, 0xD8, 0xD1, 0xE9, 0x90, 0x80, 0xEB, 0x54, 0x6C, 0x9B,
          0xBD, 0xF5, 0x56, 0xE6, 0x21, 0x0F, 0xD4, 0x27, 0x85, 0x89, 0x9E,
          0x78, 0xCC, 0x58, 0x9E, 0xBE, 0x31, 0x0F, 0x6C, 0xDB, 0x9F, 0xF4,
          0xA3, 0x3B, 0x30, 0x39, 0x30, 0x22, 0x06, 0x09, 0x2B, 0x06, 0x01,
          0x04, 0x01, 0x82, 0xC4, 0x0A, 0x02, 0x04, 0x15, 0x31, 0x2E, 0x33,
          0x2E, 0x36, 0x2E, 0x31, 0x2E, 0x34, 0x2E, 0x31, 0x2E, 0x34, 0x31,
          0x34, 0x38, 0x32, 0x2E, 0x31, 0x2E, 0x32, 0x30, 0x13, 0x06, 0x0B,
          0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0xE5, 0x1C, 0x02, 0x01, 0x01,
          0x04, 0x04, 0x03, 0x02, 0x04, 0x30, 0x30, 0x0D, 0x06, 0x09, 0x2A,
          0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05, 0x00, 0x03,
          0x82, 0x01, 0x01, 0x00, 0x9F, 0x9B, 0x05, 0x22, 0x48, 0xBC, 0x4C,
          0xF4, 0x2C, 0xC5, 0x99, 0x1F, 0xCA, 0xAB, 0xAC, 0x9B, 0x65, 0x1B,
          0xBE, 0x5B, 0xDC, 0xDC, 0x8E, 0xF0, 0xAD, 0x2C, 0x1C, 0x1F, 0xFB,
          0x36, 0xD1, 0x87, 0x15, 0xD4, 0x2E, 0x78, 0xB2, 0x49, 0x22, 0x4F,
          0x92, 0xC7, 0xE6, 0xE7, 0xA0, 0x5C, 0x49, 0xF0, 0xE7, 0xE4, 0xC8,
          0x81, 0xBF, 0x2E, 0x94, 0xF4, 0x5E, 0x4A, 0x21, 0x83, 0x3D, 0x74,
          0x56, 0x85, 0x1D, 0x0F, 0x6C, 0x14, 0x5A, 0x29, 0x54, 0x0C, 0x87,
          0x4F, 0x30, 0x92, 0xC9, 0x34, 0xB4, 0x3D, 0x22, 0x2B, 0x89, 0x62,
          0xC0, 0xF4, 0x10, 0xCE, 0xF1, 0xDB, 0x75, 0x89, 0x2A, 0xF1, 0x16,
          0xB4, 0x4A, 0x96, 0xF5, 0xD3, 0x5A, 0xDE, 0xA3, 0x82, 0x2F, 0xC7,
          0x14, 0x6F, 0x60, 0x04, 0x38, 0x5B, 0xCB, 0x69, 0xB6, 0x5C, 0x99,
          0xE7, 0xEB, 0x69, 0x19, 0x78, 0x67, 0x03, 0xC0, 0xD8, 0xCD, 0x41,
          0xE8, 0xF7, 0x5C, 0xCA, 0x44, 0xAA, 0x8A, 0xB7, 0x25, 0xAD, 0x8E,
          0x79, 0x9F, 0xF3, 0xA8, 0x69, 0x6A, 0x6F, 0x1B, 0x26, 0x56, 0xE6,
          0x31, 0xB1, 0xE4, 0x01, 0x83, 0xC0, 0x8F, 0xDA, 0x53, 0xFA, 0x4A,
          0x8F, 0x85, 0xA0, 0x56, 0x93, 0x94, 0x4A, 0xE1, 0x79, 0xA1, 0x33,
          0x9D, 0x00, 0x2D, 0x15, 0xCA, 0xBD, 0x81, 0x00, 0x90, 0xEC, 0x72,
          0x2E, 0xF5, 0xDE, 0xF9, 0x96, 0x5A, 0x37, 0x1D, 0x41, 0x5D, 0x62,
          0x4B, 0x68, 0xA2, 0x70, 0x7C, 0xAD, 0x97, 0xBC, 0xDD, 0x17, 0x85,
          0xAF, 0x97, 0xE2, 0x58, 0xF3, 0x3D, 0xF5, 0x6A, 0x03, 0x1A, 0xA0,
          0x35, 0x6D, 0x8E, 0x8D, 0x5E, 0xBC, 0xAD, 0xC7, 0x4E, 0x07, 0x16,
          0x36, 0xC6, 0xB1, 0x10, 0xAC, 0xE5, 0xCC, 0x9B, 0x90, 0xDF, 0xEA,
          0xCA, 0xE6, 0x40, 0xFF, 0x1B, 0xB0, 0xF1, 0xFE, 0x5D, 0xB4, 0xEF,
          0xF7, 0xA9, 0x5F, 0x06, 0x07, 0x33, 0xF5
    // clang-format on
};

// Components of the CBOR needed to form an authenticator object.
// Combined diagnostic notation:
// {"fmt": "fido-u2f", "attStmt": {"sig": h'30...}, "authData": h'D4C9D9...'}
constexpr uint8_t kFormatFidoU2fCBOR[] = {
    // clang-format off
    0xA3,  // map(3)
      0x63,  // text(3)
        0x66, 0x6D, 0x74,  // "fmt"
      0x68,  // text(8)
        0x66, 0x69, 0x64, 0x6F, 0x2D, 0x75, 0x32, 0x66  // "fido-u2f"
    // clang-format on
};

constexpr uint8_t kAttStmtCBOR[] = {
    // clang-format off
      0x67,  // text(7)
        0x61, 0x74, 0x74, 0x53, 0x74, 0x6D, 0x74  // "attStmt"
    // clang-format on
};

constexpr uint8_t kAuthDataCBOR[] = {
    // clang-format off
    0x68,  // text(8)
      0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61,  // "authData"
    0x58, 0xC4  // bytes(196). i.e.,the authenticator_data bytearray
    // clang-format on
};

// Helpers for testing U2f register responses.
std::vector<uint8_t> GetTestECPublicKeyCOSE() {
  return std::vector<uint8_t>(std::begin(kTestECPublicKeyCOSE),
                              std::end(kTestECPublicKeyCOSE));
}

std::vector<uint8_t> GetTestRegisterResponse() {
  return std::vector<uint8_t>(std::begin(test_data::kTestU2fRegisterResponse),
                              std::end(test_data::kTestU2fRegisterResponse));
}

std::vector<uint8_t> GetTestCredentialRawIdBytes() {
  return std::vector<uint8_t>(std::begin(test_data::kTestCredentialRawIdBytes),
                              std::end(test_data::kTestCredentialRawIdBytes));
}

std::vector<uint8_t> GetU2fAttestationStatementCBOR() {
  return std::vector<uint8_t>(std::begin(kU2fAttestationStatementCBOR),
                              std::end(kU2fAttestationStatementCBOR));
}

std::vector<uint8_t> GetTestAttestedCredentialDataBytes() {
  // Combine kTestAttestedCredentialDataPrefix and kTestECPublicKeyCOSE.
  std::vector<uint8_t> test_attested_data(
      std::begin(kTestAttestedCredentialDataPrefix),
      std::end(kTestAttestedCredentialDataPrefix));
  test_attested_data.insert(test_attested_data.end(),
                            std::begin(kTestECPublicKeyCOSE),
                            std::end(kTestECPublicKeyCOSE));
  return test_attested_data;
}

std::vector<uint8_t> GetTestAuthenticatorDataBytes() {
  // Build the test authenticator data.
  std::vector<uint8_t> test_authenticator_data(
      std::begin(kTestAuthenticatorDataPrefix),
      std::end(kTestAuthenticatorDataPrefix));
  std::vector<uint8_t> test_attested_data =
      GetTestAttestedCredentialDataBytes();
  test_authenticator_data.insert(test_authenticator_data.end(),
                                 test_attested_data.begin(),
                                 test_attested_data.end());
  return test_authenticator_data;
}

std::vector<uint8_t> GetTestAttestationObjectBytes() {
  std::vector<uint8_t> test_authenticator_object(std::begin(kFormatFidoU2fCBOR),
                                                 std::end(kFormatFidoU2fCBOR));
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   std::begin(kAttStmtCBOR),
                                   std::end(kAttStmtCBOR));
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   std::begin(kU2fAttestationStatementCBOR),
                                   std::end(kU2fAttestationStatementCBOR));
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   std::begin(kAuthDataCBOR),
                                   std::end(kAuthDataCBOR));
  std::vector<uint8_t> test_authenticator_data =
      GetTestAuthenticatorDataBytes();
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   test_authenticator_data.begin(),
                                   test_authenticator_data.end());
  return test_authenticator_object;
}

}  // namespace

class U2fRegisterTest : public testing::Test {
 public:
  U2fRegisterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class TestRegisterCallback {
 public:
  TestRegisterCallback()
      : callback_(base::BindOnce(&TestRegisterCallback::ReceivedCallback,
                                 base::Unretained(this))) {}
  ~TestRegisterCallback() = default;

  void ReceivedCallback(U2fReturnCode status_code,
                        base::Optional<RegisterResponseData> response_data) {
    response_ = std::make_pair(status_code, std::move(response_data));
    closure_.Run();
  }

  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
  WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return response_;
  }

  U2fRegister::RegisterResponseCallback& callback() { return callback_; }

 private:
  std::pair<U2fReturnCode, base::Optional<RegisterResponseData>> response_;
  base::Closure closure_;
  U2fRegister::RegisterResponseCallback callback_;
  base::RunLoop run_loop_;
};

TEST_F(U2fRegisterTest, TestRegisterSuccess) {
  auto device = std::make_unique<MockU2fDevice>();

  MockU2fDiscovery discovery;

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::vector<uint8_t>> registration_keys;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, registration_keys,
      std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(cb.callback()));
  request->Start();
  discovery.AddDevice(std::move(device));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  EXPECT_EQ(GetTestCredentialRawIdBytes(), std::get<1>(response)->raw_id());
}

TEST_F(U2fRegisterTest, TestDelayedSuccess) {
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  // Go through the state machine twice before success
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;

  std::vector<std::vector<uint8_t>> registration_keys;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, registration_keys,
      std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(cb.callback()));
  request->Start();
  discovery.AddDevice(std::move(device));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  EXPECT_EQ(GetTestCredentialRawIdBytes(), std::get<1>(response)->raw_id());
}

TEST_F(U2fRegisterTest, TestMultipleDevices) {
  // Second device will have a successful touch
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::vector<uint8_t>> registration_keys;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, registration_keys,
      std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(cb.callback()));
  request->Start();
  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            std::get<1>(response).value().raw_id());
}

// Tests a scenario where a single device is connected and registration call
// is received with three unknown key handles. We expect that three check only
// sign-in calls be processed before registration.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithExclusionList) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2};
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  // DeviceTransact() will be called four times including three check
  // only sign-in calls and one registration call. For the first three calls,
  // device will invoke MockU2fDevice::WrongData as the authenticator did not
  // create the three key handles provided in the exclude list. At the fourth
  // call, MockU2fDevice::NoErrorRegister will be invoked after registration.
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .Times(4)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  // TryWink() will be called twice. First during the check only sign-in. After
  // check only sign operation is complete, request state is changed to IDLE,
  // and TryWink() is called again before Register() is called.
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device));

  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  EXPECT_EQ(GetTestCredentialRawIdBytes(), std::get<1>(response)->raw_id());
}

// Tests a scenario where two devices are connected and registration call is
// received with three unknown key handles. We assume that user will proceed the
// registration with second device, "device1".
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithExclusionList) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2};
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));

  // DeviceTransact() will be called four times: three times to check for
  // duplicate key handles and once for registration. Since user
  // will register using "device1", the fourth call will invoke
  // MockU2fDevice::NotSatisfied.
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // We assume that user registers with second device. Therefore, the fourth
  // DeviceTransact() will invoke MockU2fDevice::NoErrorRegister after
  // successful registration.
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  // TryWink() will be called twice on both devices -- during check only
  // sign-in operation and during registration attempt.
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));

  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  EXPECT_EQ(GetTestCredentialRawIdBytes(), std::get<1>(response)->raw_id());
}

// Tests a scenario where single device is connected and registration is called
// with a key in the exclude list that was created by this device. We assume
// that the duplicate key is the last key handle in the exclude list. Therefore,
// after duplicate key handle is found, the process is expected to terminate
// after calling bogus registration which checks for user presence.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithDuplicateHandle) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<uint8_t> duplicate_key(32, 0xA);

  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2, duplicate_key};
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));

  // For four keys in exclude list, the first three keys will invoke
  // MockU2fDevice::WrongData and the final duplicate key handle will invoke
  // MockU2fDevice::NoErrorSign. Once duplicate key handle is found, bogus
  // registration is called to confirm user presence. This invokes
  // MockU2fDevice::NoErrorRegister.
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .Times(5)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  // Since duplicate key handle is found, registration process is terminated
  // before actual Register() is called on the device. Therefore, TryWink() is
  // invoked once.
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED, std::get<0>(response));
  EXPECT_EQ(base::nullopt, std::get<1>(response));
}

// Tests a scenario where one(device1) of the two devices connected has created
// a key handle provided in exclude list. We assume that duplicate key is the
// fourth key handle provided in the exclude list.
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithDuplicateHandle) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<uint8_t> duplicate_key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2, duplicate_key};
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));

  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));

  // Since the first device did not create any of the key handles provided in
  // exclude list, we expect that check only sign() should be called
  // four times, and all the calls to DeviceTransact() invoke
  // MockU2fDevice::WrongData.
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .Times(4)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData));
  // Since the last key handle in exclude list is a duplicate key, we expect
  // that the first three calls to check only sign() invoke
  // MockU2fDevice::WrongData and that fourth sign() call invoke
  // MockU2fDevice::NoErrorSign. After duplicate key is found, process is
  // terminated after user presence is verified using bogus registration, which
  // invokes MockU2fDevice::NoErrorRegister.
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .Times(5)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));
  const std::pair<U2fReturnCode, base::Optional<RegisterResponseData>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED, std::get<0>(response));
  EXPECT_EQ(base::nullopt, std::get<1>(response));
}

// These test the parsing of the U2F raw bytes of the registration response.
// Test that an EC public key serializes to CBOR properly.
TEST_F(U2fRegisterTest, TestSerializedPublicKey) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  EXPECT_EQ(GetTestECPublicKeyCOSE(), public_key->EncodeAsCOSEKey());
}

// Test that the attestation statement cbor map is constructed properly.
TEST_F(U2fRegisterTest, TestU2fAttestationStatementCBOR) {
  std::unique_ptr<FidoAttestationStatement> fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse());
  auto cbor = cbor::CBORWriter::Write(
      cbor::CBORValue(fido_attestation_statement->GetAsCBORMap()));
  ASSERT_TRUE(cbor);
  EXPECT_EQ(GetU2fAttestationStatementCBOR(), *cbor);
}

// Tests that well-formed attested credential data serializes properly.
TEST_F(U2fRegisterTest, TestAttestedCredentialData) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::vector<uint8_t>(16) /* aaguid */,
          std::move(public_key));

  EXPECT_EQ(GetTestAttestedCredentialDataBytes(),
            attested_data->SerializeAsBytes());
}

// Tests that well-formed authenticator data serializes properly.
TEST_F(U2fRegisterTest, TestAuthenticatorData) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::vector<uint8_t>(16) /* aaguid */,
          std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(kTestRelyingPartyId, flags,
                                       std::vector<uint8_t>(4) /* counter */,
                                       std::move(attested_data));

  EXPECT_EQ(GetTestAuthenticatorDataBytes(),
            authenticator_data.SerializeToByteArray());
}

// Tests that a U2F attestation object serializes properly.
TEST_F(U2fRegisterTest, TestU2fAttestationObject) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::vector<uint8_t>(16) /* aaguid */,
          std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  AuthenticatorData authenticator_data(kTestRelyingPartyId, flags,
                                       std::vector<uint8_t>(4) /* counter */,
                                       std::move(attested_data));

  // Construct the attestation statement.
  std::unique_ptr<FidoAttestationStatement> fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse());

  // Construct the attestation object.
  auto attestation_object = std::make_unique<AttestationObject>(
      std::move(authenticator_data), std::move(fido_attestation_statement));

  EXPECT_EQ(GetTestAttestationObjectBytes(),
            attestation_object->SerializeToCBOREncodedBytes());
}

// Test that a U2F register response is properly parsed.
TEST_F(U2fRegisterTest, TestRegisterResponseData) {
  base::Optional<RegisterResponseData> response =
      RegisterResponseData::CreateFromU2fRegisterResponse(
          kTestRelyingPartyId, GetTestRegisterResponse());
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->raw_id());
  EXPECT_EQ(GetTestAttestationObjectBytes(),
            response->GetCBOREncodedAttestationObject());
}

}  // namespace device
