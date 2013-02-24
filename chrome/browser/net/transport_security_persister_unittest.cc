// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/transport_security_persister.h"

#include <map>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/transport_security_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::TransportSecurityState;

class TransportSecurityPersisterTest : public testing::Test {
 public:
  TransportSecurityPersisterTest()
      : message_loop_(MessageLoop::TYPE_IO),
        test_file_thread_(content::BrowserThread::FILE, &message_loop_),
        test_io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  virtual ~TransportSecurityPersisterTest() {
    message_loop_.RunUntilIdle();
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    persister_.reset(
        new TransportSecurityPersister(&state_, temp_dir_.path(), false));
  }

 protected:
  // Ordering is important here. If member variables are not destroyed in the
  // right order, then DCHECKs will fail all over the place.
  MessageLoop message_loop_;

  // Needed for ImportantFileWriter, which TransportSecurityPersister uses.
  content::TestBrowserThread test_file_thread_;

  // TransportSecurityPersister runs on the IO thread.
  content::TestBrowserThread test_io_thread_;

  base::ScopedTempDir temp_dir_;
  TransportSecurityState state_;
  scoped_ptr<TransportSecurityPersister> persister_;
};

TEST_F(TransportSecurityPersisterTest, SerializeData1) {
  std::string output;
  bool dirty;

  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityPersisterTest, SerializeData2) {
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_.GetDomainState(kYahooDomain, true, &domain_state));
  domain_state.upgrade_mode =
      TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
  domain_state.upgrade_expiry = expiry;
  domain_state.include_subdomains = true;
  state_.EnableHost(kYahooDomain, domain_state);

  std::string output;
  bool dirty;
  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));

  EXPECT_TRUE(state_.GetDomainState(kYahooDomain, true, &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDomainState("foo.yahoo.com", true, &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDomainState("foo.bar.yahoo.com", true, &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDomainState("foo.bar.baz.yahoo.com", true,
                                   &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(state_.GetDomainState("com", true, &domain_state));
}

TEST_F(TransportSecurityPersisterTest, SerializeData3) {
  // Add an entry.
  net::HashValue fp1(net::HASH_VALUE_SHA1);
  memset(fp1.data(), 0, fp1.size());
  net::HashValue fp2(net::HASH_VALUE_SHA1);
  memset(fp2.data(), 1, fp2.size());
  TransportSecurityState::DomainState example_state;
  example_state.upgrade_expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  example_state.upgrade_mode =
      TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
  example_state.dynamic_spki_hashes_expiry = example_state.upgrade_expiry;
  example_state.dynamic_spki_hashes.push_back(fp1);
  example_state.dynamic_spki_hashes.push_back(fp2);
  state_.EnableHost("www.example.com", example_state);

  // Add another entry.
  memset(fp1.data(), 2, fp1.size());
  memset(fp2.data(), 3, fp2.size());
  example_state.upgrade_expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(3000);
  example_state.upgrade_mode =
      TransportSecurityState::DomainState::MODE_DEFAULT;
  example_state.dynamic_spki_hashes_expiry = example_state.upgrade_expiry;
  example_state.dynamic_spki_hashes.push_back(fp1);
  example_state.dynamic_spki_hashes.push_back(fp2);
  state_.EnableHost("www.example.net", example_state);

  // Save a copy of everything.
  std::map<std::string, TransportSecurityState::DomainState> saved;
  TransportSecurityState::Iterator i(state_);
  while (i.HasNext()) {
    saved[i.hostname()] = i.domain_state();
    i.Advance();
  }

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));

  // Persist the data to the file. For the test to be fast and not flaky, we
  // just do it directly rather than call persister_->StateIsDirty. (That uses
  // ImportantFileWriter, which has an asynchronous commit interval rather
  // than block.) Use a different basename just for cleanliness.
  base::FilePath path =
      temp_dir_.path().AppendASCII("TransportSecurityPersisterTest");
  EXPECT_TRUE(file_util::WriteFile(path, serialized.c_str(),
                                   serialized.size()));

  // Read the data back.
  std::string persisted;
  EXPECT_TRUE(file_util::ReadFileToString(path, &persisted));
  EXPECT_EQ(persisted, serialized);
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(persisted, &dirty));
  EXPECT_FALSE(dirty);

  // Check that states are the same as saved.
  size_t count = 0;
  TransportSecurityState::Iterator j(state_);
  while (j.HasNext()) {
    EXPECT_TRUE(saved[j.hostname()].Equals(j.domain_state()));
    count++;
    j.Advance();
  }
  EXPECT_EQ(count, saved.size());
}

TEST_F(TransportSecurityPersisterTest, SerializeDataOld) {
  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  std::string output =
      "{ "
      "\"NiyD+3J1r6z1wjl2n1ALBu94Zj9OsEAMo0kCN8js0Uk=\": {"
      "\"expiry\": 1266815027.983453, "
      "\"include_subdomains\": false, "
      "\"mode\": \"strict\" "
      "}"
      "}";
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_TRUE(dirty);
}

TEST_F(TransportSecurityPersisterTest, PublicKeyHashes) {
  TransportSecurityState::DomainState domain_state;
  static const char kTestDomain[] = "example.com";
  EXPECT_FALSE(state_.GetDomainState(kTestDomain, false, &domain_state));
  net::HashValueVector hashes;
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes));

  net::HashValue sha1(net::HASH_VALUE_SHA1);
  memset(sha1.data(), '1', sha1.size());
  domain_state.static_spki_hashes.push_back(sha1);

  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes));

  hashes.push_back(sha1);
  EXPECT_TRUE(domain_state.CheckPublicKeyPins(hashes));

  hashes[0].data()[0] = '2';
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.upgrade_expiry = expiry;
  state_.EnableHost(kTestDomain, domain_state);
  std::string ser;
  EXPECT_TRUE(persister_->SerializeData(&ser));
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(ser, &dirty));
  EXPECT_TRUE(state_.GetDomainState(kTestDomain, false, &domain_state));
  EXPECT_EQ(1u, domain_state.static_spki_hashes.size());
  EXPECT_EQ(sha1.tag, domain_state.static_spki_hashes[0].tag);
  EXPECT_EQ(0, memcmp(domain_state.static_spki_hashes[0].data(), sha1.data(),
                      sha1.size()));
}

TEST_F(TransportSecurityPersisterTest, ForcePreloads) {
  // The static state for docs.google.com, defined in
  // net/base/transport_security_state_static.h, has pins and mode strict.
  // This new policy overrides that with no pins and a weaker mode. We apply
  // this new policy with |DeserializeFromCommandLine| and expect that the
  // new policy is in effect, overriding the static policy.
  std::string preload("{"
                      "\"4AGT3lHihuMSd5rUj7B4u6At0jlSH3HFePovjPR+oLE=\": {"
                      "\"created\": 0.0,"
                      "\"expiry\": 2000000000.0,"
                      "\"include_subdomains\": false,"
                      "\"mode\": \"pinning-only\""
                      "}}");

  EXPECT_TRUE(persister_->DeserializeFromCommandLine(preload));

  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state_.GetDomainState("docs.google.com", true, &domain_state));
  EXPECT_FALSE(domain_state.HasPublicKeyPins());
  EXPECT_FALSE(domain_state.ShouldUpgradeToSSL());
}
