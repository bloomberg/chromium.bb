// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_mac.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/pem.h"
#include "net/cert/test_keychain_search_list_mac.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_mac.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAreArray;

namespace net {

namespace {

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";

// Parses a PEM encoded certificate from |file_name| and stores in |result|.
::testing::AssertionResult ReadTestCert(
    const std::string& file_name,
    scoped_refptr<ParsedCertificate>* result) {
  std::string der;
  const PemBlockMapping mappings[] = {
      {kCertificateHeader, &der},
  };

  ::testing::AssertionResult r = ReadTestDataFromPemFile(
      "net/data/ssl/certificates/" + file_name, mappings);
  if (!r)
    return r;

  CertErrors errors;
  *result = ParsedCertificate::Create(x509_util::CreateCryptoBuffer(der), {},
                                      &errors);
  if (!*result) {
    return ::testing::AssertionFailure()
           << "ParseCertificate::Create() failed:\n"
           << errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

// Returns the DER encodings of the SecCertificates in |array|.
std::vector<std::string> SecCertificateArrayAsDER(CFArrayRef array) {
  std::vector<std::string> result;

  for (CFIndex i = 0, item_count = CFArrayGetCount(array); i < item_count;
       ++i) {
    SecCertificateRef match_cert_handle = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(array, i)));
    if (!match_cert_handle) {
      ADD_FAILURE() << "null item " << i;
      continue;
    }
    base::ScopedCFTypeRef<CFDataRef> der_data(
        SecCertificateCopyData(match_cert_handle));
    if (!der_data) {
      ADD_FAILURE() << "SecCertificateCopyData error";
      continue;
    }
    result.push_back(std::string(
        reinterpret_cast<const char*>(CFDataGetBytePtr(der_data.get())),
        CFDataGetLength(der_data.get())));
  }

  return result;
}

// Returns the DER encodings of the ParsedCertificates in |list|.
std::vector<std::string> ParsedCertificateListAsDER(
    ParsedCertificateList list) {
  std::vector<std::string> result;
  for (const auto& it : list)
    result.push_back(it->der_cert().AsString());
  return result;
}

class DebugData : public base::SupportsUserData {
 public:
  ~DebugData() override = default;
};

}  // namespace

// Test the trust store using known test certificates in a keychain.  Tests
// that issuer searching returns the expected certificates, and that none of
// the certificates are trusted.
TEST(TrustStoreMacTest, MultiRootNotTrusted) {
  std::unique_ptr<TestKeychainSearchList> test_keychain_search_list(
      TestKeychainSearchList::Create());
  ASSERT_TRUE(test_keychain_search_list);
  base::FilePath keychain_path(
      GetTestCertsDirectory().AppendASCII("multi-root.keychain"));
  // SecKeychainOpen does not fail if the file doesn't exist, so assert it here
  // for easier debugging.
  ASSERT_TRUE(base::PathExists(keychain_path));
  base::ScopedCFTypeRef<SecKeychainRef> keychain;
  OSStatus status = SecKeychainOpen(keychain_path.MaybeAsASCII().c_str(),
                                    keychain.InitializeInto());
  ASSERT_EQ(errSecSuccess, status);
  ASSERT_TRUE(keychain);
  test_keychain_search_list->AddKeychain(keychain);

  TrustStoreMac trust_store(kSecPolicyAppleSSL);

  scoped_refptr<ParsedCertificate> a_by_b, b_by_c, b_by_f, c_by_d, c_by_e,
      f_by_e, d_by_d, e_by_e;
  ASSERT_TRUE(ReadTestCert("multi-root-A-by-B.pem", &a_by_b));
  ASSERT_TRUE(ReadTestCert("multi-root-B-by-C.pem", &b_by_c));
  ASSERT_TRUE(ReadTestCert("multi-root-B-by-F.pem", &b_by_f));
  ASSERT_TRUE(ReadTestCert("multi-root-C-by-D.pem", &c_by_d));
  ASSERT_TRUE(ReadTestCert("multi-root-C-by-E.pem", &c_by_e));
  ASSERT_TRUE(ReadTestCert("multi-root-F-by-E.pem", &f_by_e));
  ASSERT_TRUE(ReadTestCert("multi-root-D-by-D.pem", &d_by_d));
  ASSERT_TRUE(ReadTestCert("multi-root-E-by-E.pem", &e_by_e));

  base::ScopedCFTypeRef<CFDataRef> normalized_name_b =
      TrustStoreMac::GetMacNormalizedIssuer(a_by_b.get());
  ASSERT_TRUE(normalized_name_b);
  base::ScopedCFTypeRef<CFDataRef> normalized_name_c =
      TrustStoreMac::GetMacNormalizedIssuer(b_by_c.get());
  ASSERT_TRUE(normalized_name_c);
  base::ScopedCFTypeRef<CFDataRef> normalized_name_f =
      TrustStoreMac::GetMacNormalizedIssuer(b_by_f.get());
  ASSERT_TRUE(normalized_name_f);
  base::ScopedCFTypeRef<CFDataRef> normalized_name_d =
      TrustStoreMac::GetMacNormalizedIssuer(c_by_d.get());
  ASSERT_TRUE(normalized_name_d);
  base::ScopedCFTypeRef<CFDataRef> normalized_name_e =
      TrustStoreMac::GetMacNormalizedIssuer(f_by_e.get());
  ASSERT_TRUE(normalized_name_e);

  // Test that the matching keychain items are found, even though they aren't
  // trusted.
  // TODO(eroman): These tests could be using TrustStore::SyncGetIssuersOf().
  {
    base::ScopedCFTypeRef<CFArrayRef> scoped_matching_items =
        TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
            normalized_name_b.get());

    EXPECT_THAT(SecCertificateArrayAsDER(scoped_matching_items),
                UnorderedElementsAreArray(
                    ParsedCertificateListAsDER({b_by_c, b_by_f})));
  }

  {
    base::ScopedCFTypeRef<CFArrayRef> scoped_matching_items =
        TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
            normalized_name_c.get());
    EXPECT_THAT(SecCertificateArrayAsDER(scoped_matching_items),
                UnorderedElementsAreArray(
                    ParsedCertificateListAsDER({c_by_d, c_by_e})));
  }

  {
    base::ScopedCFTypeRef<CFArrayRef> scoped_matching_items =
        TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
            normalized_name_f.get());
    EXPECT_THAT(
        SecCertificateArrayAsDER(scoped_matching_items),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({f_by_e})));
  }

  {
    base::ScopedCFTypeRef<CFArrayRef> scoped_matching_items =
        TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
            normalized_name_d.get());
    EXPECT_THAT(
        SecCertificateArrayAsDER(scoped_matching_items),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({d_by_d})));
  }

  {
    base::ScopedCFTypeRef<CFArrayRef> scoped_matching_items =
        TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
            normalized_name_e.get());
    EXPECT_THAT(
        SecCertificateArrayAsDER(scoped_matching_items),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({e_by_e})));
  }

  // Verify that none of the added certificates are considered trusted (since
  // the test certs in the keychain aren't trusted, unless someone manually
  // added and trusted the test certs on the machine the test is being run on).
  for (const auto& cert :
       {a_by_b, b_by_c, b_by_f, c_by_d, c_by_e, f_by_e, d_by_d, e_by_e}) {
    CertificateTrust trust = CertificateTrust::ForTrustAnchor();
    DebugData debug_data;
    trust_store.GetTrust(cert.get(), &trust, &debug_data);
    EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
    // Certs without trust settings should not add debug info to debug_data.
    EXPECT_FALSE(TrustStoreMac::ResultDebugData::Get(&debug_data));
  }
}

// Test against all the certificates in the default keychains. Confirms that
// the computed trust value matches that of SecTrustEvaluate.
TEST(TrustStoreMacTest, SystemCerts) {
  // Get the list of all certificates in the user & system keychains.
  // This may include both trusted and untrusted certificates.
  //
  // The output contains zero or more repetitions of:
  // "SHA-1 hash: <hash>\n<PEM encoded cert>\n"
  // Starting with macOS 10.15, it includes both SHA-256 and SHA-1 hashes:
  // "SHA-256 hash: <hash>\nSHA-1 hash: <hash>\n<PEM encoded cert>\n"
  std::string find_certificate_default_search_list_output;
  ASSERT_TRUE(
      base::GetAppOutput({"security", "find-certificate", "-a", "-p", "-Z"},
                         &find_certificate_default_search_list_output));
  // Get the list of all certificates in the system roots keychain.
  // (Same details as above.)
  std::string find_certificate_system_roots_output;
  ASSERT_TRUE(base::GetAppOutput(
      {"security", "find-certificate", "-a", "-p", "-Z",
       "/System/Library/Keychains/SystemRootCertificates.keychain"},
      &find_certificate_system_roots_output));

  TrustStoreMac trust_store(kSecPolicyAppleX509Basic);

  base::ScopedCFTypeRef<SecPolicyRef> sec_policy(SecPolicyCreateBasicX509());
  ASSERT_TRUE(sec_policy);
  for (const std::string& hash_and_pem_partial : base::SplitStringUsingSubstr(
           find_certificate_system_roots_output +
               find_certificate_default_search_list_output,
           "-----END CERTIFICATE-----", base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    // Re-add the PEM ending mark, since SplitStringUsingSubstr eats it.
    const std::string hash_and_pem =
        hash_and_pem_partial + "\n-----END CERTIFICATE-----\n";

    // Use the first hash value found in the text. This might be SHA-256 or
    // SHA-1, but it's only for debugging purposes so it doesn't matter as long
    // as one exists.
    std::string::size_type hash_pos = hash_and_pem.find("hash: ");
    ASSERT_NE(std::string::npos, hash_pos);
    hash_pos += 6;
    std::string::size_type eol_pos = hash_and_pem.find_first_of("\r\n");
    ASSERT_NE(std::string::npos, eol_pos);
    // Extract the hash of the certificate. This isn't necessary for the
    // test, but is a convenient identifier to use in any error messages.
    std::string hash_text = hash_and_pem.substr(hash_pos, eol_pos - hash_pos);

    SCOPED_TRACE(hash_text);
    // TODO(mattm): The same cert might exist in both lists, could de-dupe
    // before testing?

    // Parse the PEM encoded text to DER bytes.
    PEMTokenizer pem_tokenizer(hash_and_pem, {kCertificateHeader});
    ASSERT_TRUE(pem_tokenizer.GetNext());
    std::string cert_der(pem_tokenizer.data());
    ASSERT_FALSE(pem_tokenizer.GetNext());

    CertErrors errors;
    // Note: don't actually need to make a ParsedCertificate here, just need
    // the DER bytes. But parsing it here ensures the test can skip any certs
    // that won't be returned due to parsing failures inside TrustStoreMac.
    // The parsing options set here need to match the ones used in
    // trust_store_mac.cc.
    ParseCertificateOptions options;
    // For https://crt.sh/?q=D3EEFBCBBCF49867838626E23BB59CA01E305DB7:
    options.allow_invalid_serial_numbers = true;
    scoped_refptr<ParsedCertificate> cert = ParsedCertificate::Create(
        x509_util::CreateCryptoBuffer(cert_der), options, &errors);
    if (!cert) {
      LOG(WARNING) << "ParseCertificate::Create " << hash_text << " failed:\n"
                   << errors.ToDebugString();
      continue;
    }
    // Check if this cert is considered a trust anchor by TrustStoreMac.
    CertificateTrust cert_trust;
    DebugData debug_data;
    trust_store.GetTrust(cert, &cert_trust, &debug_data);
    bool is_trust_anchor = cert_trust.IsTrustAnchor();

    // Check if this cert is considered a trust anchor by the OS.
    base::ScopedCFTypeRef<SecCertificateRef> cert_handle(
        x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                                 cert->der_cert().Length()));
    if (!cert_handle) {
      ADD_FAILURE() << "CreateCertBufferFromBytes " << hash_text;
      continue;
    }
    base::ScopedCFTypeRef<SecTrustRef> trust;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      ASSERT_EQ(noErr,
                SecTrustCreateWithCertificates(cert_handle, sec_policy,
                                               trust.InitializeInto()));
      ASSERT_EQ(noErr,
                SecTrustSetOptions(trust, kSecTrustOptionLeafIsCA |
                                              kSecTrustOptionAllowExpired |
                                              kSecTrustOptionAllowExpiredRoot));

      SecTrustResultType trust_result;
      ASSERT_EQ(noErr, SecTrustEvaluate(trust, &trust_result));
      bool expected_trust_anchor =
          ((trust_result == kSecTrustResultProceed) ||
           (trust_result == kSecTrustResultUnspecified)) &&
          (SecTrustGetCertificateCount(trust) == 1);
      EXPECT_EQ(expected_trust_anchor, is_trust_anchor);
      if (is_trust_anchor) {
        auto* trust_debug_data =
            TrustStoreMac::ResultDebugData::Get(&debug_data);
        ASSERT_TRUE(trust_debug_data);
        // Since this test queries the real trust store, can't know exactly
        // what bits should be set in the trust debug info, but it should at
        // least have something set.
        EXPECT_NE(0, trust_debug_data->combined_trust_debug_info());
      }
    }
  }
}

}  // namespace net
