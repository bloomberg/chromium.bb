// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_policy_manager.h"

#include <iterator>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_message_loop.h"
#include "base/values.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

class CTPolicyManagerTest : public ::testing::Test {
 public:
  CTPolicyManagerTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  void SetUp() override {
    cert_ = net::CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "ok_cert.pem",
        net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    ASSERT_TRUE(cert_);
    hashes_.push_back(net::HashValue(
        net::X509Certificate::CalculateFingerprint256(cert_->cert_buffer())));
  }

 protected:
  base::TestMessageLoop message_loop_;
  scoped_refptr<net::X509Certificate> cert_;
  net::HashValueVector hashes_;
};

// Treat the preferences as a black box as far as naming, but ensure that
// preferences get registered.
TEST_F(CTPolicyManagerTest, RegistersPrefs) {
  TestingPrefServiceSimple pref_service;
  auto registered_prefs = std::distance(pref_service.registry()->begin(),
                                        pref_service.registry()->end());
  certificate_transparency::prefs::RegisterPrefs(pref_service.registry());
  auto newly_registered_prefs = std::distance(pref_service.registry()->begin(),
                                              pref_service.registry()->end());
  EXPECT_NE(registered_prefs, newly_registered_prefs);
}

TEST_F(CTPolicyManagerTest, DelegateChecksRequired) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No required host set yet.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));

  // Add a required host
  manager.UpdateCTPolicies(
      std::vector<std::string>{"google.com"}, std::vector<std::string>(),
      std::vector<std::string>(), std::vector<std::string>());

  // The new setting should take effect.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));
}

TEST_F(CTPolicyManagerTest, DelegateChecksExcluded) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No setting should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));

  // Add a excluded host
  manager.UpdateCTPolicies(
      std::vector<std::string>(), std::vector<std::string>{"google.com"},
      std::vector<std::string>(), std::vector<std::string>());

  // The new setting should take effect.
  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));
}

TEST_F(CTPolicyManagerTest, IgnoresInvalidEntries) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No setting should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));

  // Now setup invalid state (that is, that fail to be parsable as
  // URLs).
  manager.UpdateCTPolicies(
      std::vector<std::string>{
          "file:///etc/fstab", "file://withahost/etc/fstab",
          "file:///c|/Windows", "*", "https://*", "example.com",
          "https://example.test:invalid_port"},
      std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>());

  // Wildcards are ignored (both * and https://*).
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));
  // File URL hosts are ignored.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("withahost", cert_.get(), hashes_));

  // While the partially parsed hosts should take effect.
  EXPECT_EQ(
      CTRequirementLevel::REQUIRED,
      delegate->IsCTRequiredForHost("example.test", cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("example.com", cert_.get(), hashes_));
}

// Make sure the various 'undocumented' priorities apply:
//   - non-wildcards beat wildcards
//   - more specific hosts beat less specific hosts
//   - requiring beats excluding
TEST_F(CTPolicyManagerTest, AppliesPriority) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No setting should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("example.com", cert_.get(), hashes_));
  EXPECT_EQ(
      CTRequirementLevel::DEFAULT,
      delegate->IsCTRequiredForHost("sub.example.com", cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("accounts.example.com", cert_.get(),
                                          hashes_));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("login.accounts.example.com",
                                          cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("sub.accounts.example.com",
                                          cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("login.sub.accounts.example.com",
                                          cert_.get(), hashes_));
  EXPECT_EQ(
      CTRequirementLevel::DEFAULT,
      delegate->IsCTRequiredForHost("test.example.com", cert_.get(), hashes_));

  // Set up policies that exclude it for a domain and all of its subdomains,
  // but then require it for a specific host.
  manager.UpdateCTPolicies(
      std::vector<std::string>{"sub.example.com", "accounts.example.com",
                               "test.example.com"},
      std::vector<std::string>{"example.com", ".sub.example.com",
                               ".sub.accounts.example.com", "test.example.com"},
      std::vector<std::string>(), std::vector<std::string>());

  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("example.com", cert_.get(), hashes_));
  // Non-wildcarding (.sub.example.com) beats wildcarding (sub.example.com).
  EXPECT_EQ(
      CTRequirementLevel::NOT_REQUIRED,
      delegate->IsCTRequiredForHost("sub.example.com", cert_.get(), hashes_));
  // More specific hosts (accounts.example.com) beat less specific hosts
  // (example.com + wildcard).
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("accounts.example.com", cert_.get(),
                                          hashes_));
  // More specific hosts (accounts.example.com) beat less specific hosts
  // (example.com).
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("login.accounts.example.com",
                                          cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("sub.accounts.example.com",
                                          cert_.get(), hashes_));
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("login.sub.accounts.example.com",
                                          cert_.get(), hashes_));
  // Requiring beats excluding.
  EXPECT_EQ(
      CTRequirementLevel::REQUIRED,
      delegate->IsCTRequiredForHost("test.example.com", cert_.get(), hashes_));
}

TEST_F(CTPolicyManagerTest, SupportsOrgRestrictions) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  base::FilePath test_directory = net::GetTestNetDataDirectory().Append(
      FILE_PATH_LITERAL("ov_name_constraints"));

  // As all the leaves and intermediates share SPKIs in their classes, load
  // known-good answers for the remaining test config.
  scoped_refptr<net::X509Certificate> tmp =
      net::ImportCertFromFile(test_directory, "leaf-o1.pem");
  ASSERT_TRUE(tmp);
  net::HashValue leaf_spki;
  ASSERT_TRUE(
      net::x509_util::CalculateSha256SpkiHash(tmp->cert_buffer(), &leaf_spki));
  tmp = net::ImportCertFromFile(test_directory, "int-o3.pem");
  ASSERT_TRUE(tmp);
  net::HashValue intermediate_spki;
  ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(tmp->cert_buffer(),
                                                      &intermediate_spki));

  struct {
    const char* const leaf_file;
    const char* const intermediate_file;
    const net::HashValue spki;
    CTRequirementLevel expected;
  } kTestCases[] = {
      // Positive cases
      //
      // Exact match on the leaf SPKI (leaf has O)
      {"leaf-o1.pem", nullptr, leaf_spki, CTRequirementLevel::NOT_REQUIRED},
      // Exact match on the leaf SPKI (leaf does not have O)
      {"leaf-no-o.pem", nullptr, leaf_spki, CTRequirementLevel::NOT_REQUIRED},
      // Exact match on the leaf SPKI (leaf has O), even when the
      // intermediate does not
      {"leaf-o1.pem", "int-cn.pem", leaf_spki,
       CTRequirementLevel::NOT_REQUIRED},
      // Matches (multiple) organization values in two SEQUENCEs+SETs
      {"leaf-o1-o2.pem", "int-o1-o2.pem", intermediate_spki,
       CTRequirementLevel::NOT_REQUIRED},
      // Matches (multiple) organization values in a single SEQUENCE+SET
      {"leaf-o1-o2.pem", "int-o1-plus-o2.pem", intermediate_spki,
       CTRequirementLevel::NOT_REQUIRED},
      // Matches nameConstrained O
      {"leaf-o1.pem", "nc-int-permit-o1.pem", intermediate_spki,
       CTRequirementLevel::NOT_REQUIRED},
      // Matches the second nameConstraint on the O, out of 3
      {"leaf-o1.pem", "nc-int-permit-o2-o1-o3.pem", intermediate_spki,
       CTRequirementLevel::NOT_REQUIRED},
      // Leaf is in different string type than issuer (BMPString), but it is
      // in the issuer O field, not the nameConstraint
      // TODO(rsleevi): Make this fail, because it's not byte-for-byte
      // identical
      {"leaf-o1.pem", "int-bmp-o1.pem", intermediate_spki,
       CTRequirementLevel::NOT_REQUIRED},

      // Negative cases
      // Leaf is missing O
      {"leaf-no-o.pem", "int-o1-o2.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Leaf is missing O
      {"leaf-no-o.pem", "int-cn.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Leaf doesn't match issuer O
      {"leaf-o1.pem", "int-o3.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Multiple identical organization values, but in different orders.
      {"leaf-o1-o2.pem", "int-o2-o1.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Intermediate is nameConstrained, with a dirName, but not an O
      {"leaf-o1.pem", "nc-int-permit-cn.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Intermediate is nameConstrained, but with a dNSName
      {"leaf-o1.pem", "nc-int-permit-dns.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Intermediate is nameConstrained, but with an excludedSubtrees that
      // has a dirName that matches the O.
      {"leaf-o1.pem", "nc-int-exclude-o1.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
      // Intermediate is nameConstrained, but the encoding of the
      // nameConstraint is different from the encoding of the leaf
      {"leaf-o1.pem", "nc-int-permit-bmp-o1.pem", intermediate_spki,
       CTRequirementLevel::DEFAULT},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "leaf=" << test.leaf_file
                 << ",intermediate=" << test.intermediate_file);

    scoped_refptr<net::X509Certificate> leaf =
        net::ImportCertFromFile(test_directory, test.leaf_file);
    ASSERT_TRUE(leaf);

    net::HashValueVector hashes;
    net::HashValue leaf_hash;
    ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(leaf->cert_buffer(),
                                                        &leaf_hash));
    hashes.push_back(std::move(leaf_hash));

    // Append the intermediate to |leaf|, if any.
    if (test.intermediate_file) {
      scoped_refptr<net::X509Certificate> intermediate =
          net::ImportCertFromFile(test_directory, test.intermediate_file);
      ASSERT_TRUE(intermediate);

      net::HashValue intermediate_hash;
      ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(
          intermediate->cert_buffer(), &intermediate_hash));
      hashes.push_back(std::move(intermediate_hash));

      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
      intermediates.push_back(
          net::x509_util::DupCryptoBuffer(intermediate->cert_buffer()));

      leaf = net::X509Certificate::CreateFromBuffer(
          net::x509_util::DupCryptoBuffer(leaf->cert_buffer()),
          std::move(intermediates));
    }
    manager.UpdateCTPolicies(
        std::vector<std::string>(), std::vector<std::string>(),
        std::vector<std::string>(), std::vector<std::string>());

    // There should be no existing settings.
    EXPECT_EQ(CTRequirementLevel::DEFAULT,
              delegate->IsCTRequiredForHost("google.com", leaf.get(), hashes));

    manager.UpdateCTPolicies(std::vector<std::string>(),
                             std::vector<std::string>(),
                             std::vector<std::string>{test.spki.ToString()},
                             std::vector<std::string>());

    // The new setting should take effect.
    EXPECT_EQ(test.expected,
              delegate->IsCTRequiredForHost("google.com", leaf.get(), hashes));
  }
}

TEST_F(CTPolicyManagerTest, SupportsLegacyCaRestrictions) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  CTPolicyManager manager;

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // The hash of a known legacy CA. See
  // //net/cert/root_cert_list_generated.h
  net::SHA256HashValue legacy_spki = {{
      0x00, 0x6C, 0xB2, 0x26, 0xA7, 0x72, 0xC7, 0x18, 0x2D, 0x77, 0x72,
      0x38, 0x3E, 0x37, 0x3F, 0x0F, 0x22, 0x9E, 0x7D, 0xFE, 0x34, 0x44,
      0x81, 0x0A, 0x8D, 0x6E, 0x50, 0x90, 0x5D, 0x20, 0xD6, 0x61,
  }};

  hashes_.push_back(net::HashValue(legacy_spki));

  // No setting should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));

  // Setting to a non-legacy CA should not work.
  std::string leaf_hash_string = hashes_.front().ToString();
  manager.UpdateCTPolicies(
      std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>(), std::vector<std::string>{leaf_hash_string});

  // This setting should have no effect, because the hash for |cert_|
  // is not a legacy CA hash.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));

  // Now set to a truly legacy CA, and create a chain that
  // contains that legacy CA hash.
  std::string legacy_ca_hash_string = hashes_.back().ToString();

  manager.UpdateCTPolicies(std::vector<std::string>(),
                           std::vector<std::string>(),
                           std::vector<std::string>(),
                           std::vector<std::string>{legacy_ca_hash_string});

  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("google.com", cert_.get(), hashes_));
}

}  // namespace

}  // namespace certificate_transparency
