// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/default_origin_bound_cert_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockPersistentStore
    : public DefaultOriginBoundCertStore::PersistentStore {
 public:
  MockPersistentStore();
  virtual ~MockPersistentStore();

  // DefaultOriginBoundCertStore::PersistentStore implementation.
  virtual bool Load(
      std::vector<DefaultOriginBoundCertStore::OriginBoundCert*>* certs)
          OVERRIDE;
  virtual void AddOriginBoundCert(
      const DefaultOriginBoundCertStore::OriginBoundCert& cert) OVERRIDE;
  virtual void DeleteOriginBoundCert(
      const DefaultOriginBoundCertStore::OriginBoundCert& cert) OVERRIDE;
  virtual void SetClearLocalStateOnExit(bool clear_local_state) OVERRIDE;
  virtual void Flush(Task* completion_task) OVERRIDE;

 private:
  typedef std::map<std::string, DefaultOriginBoundCertStore::OriginBoundCert>
      OriginBoundCertMap;

  OriginBoundCertMap origin_certs_;
};

MockPersistentStore::MockPersistentStore() {}

MockPersistentStore::~MockPersistentStore() {}

bool MockPersistentStore::Load(
    std::vector<DefaultOriginBoundCertStore::OriginBoundCert*>* certs) {
  OriginBoundCertMap::iterator it;

  for (it = origin_certs_.begin(); it != origin_certs_.end(); ++it) {
    certs->push_back(
        new DefaultOriginBoundCertStore::OriginBoundCert(it->second));
  }

  return true;
}

void MockPersistentStore::AddOriginBoundCert(
    const DefaultOriginBoundCertStore::OriginBoundCert& cert) {
  origin_certs_[cert.origin()] = cert;
}

void MockPersistentStore::DeleteOriginBoundCert(
    const DefaultOriginBoundCertStore::OriginBoundCert& cert) {
  origin_certs_.erase(cert.origin());
}

void MockPersistentStore::SetClearLocalStateOnExit(bool clear_local_state) {}

void MockPersistentStore::Flush(Task* completion_task) {
  NOTREACHED();
}

TEST(DefaultOriginBoundCertStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddOriginBoundCert(
      DefaultOriginBoundCertStore::OriginBoundCert(
          "https://encrypted.google.com/", "a", "b"));
  persistent_store->AddOriginBoundCert(
      DefaultOriginBoundCertStore::OriginBoundCert(
          "https://www.verisign.com/", "c", "d"));

  // Make sure certs load properly.
  scoped_ptr<DefaultOriginBoundCertStore> store(
      new DefaultOriginBoundCertStore(persistent_store));
  EXPECT_EQ(2, store->GetCertCount());
  EXPECT_TRUE(store->SetOriginBoundCert("https://www.verisign.com/", "e", "f"));
  EXPECT_EQ(2, store->GetCertCount());
  EXPECT_TRUE(store->SetOriginBoundCert("https://www.twitter.com/", "g", "h"));
  EXPECT_EQ(3, store->GetCertCount());
}

TEST(DefaultOriginBoundCertStoreTest, TestSettingAndGetting) {
  scoped_ptr<DefaultOriginBoundCertStore> store(
      new DefaultOriginBoundCertStore(NULL));
  std::string private_key, cert;
  EXPECT_EQ(0, store->GetCertCount());
  EXPECT_FALSE(store->GetOriginBoundCert("https://www.verisign.com/",
                                         &private_key,
                                         &cert));
  EXPECT_TRUE(private_key.empty());
  EXPECT_TRUE(cert.empty());
  EXPECT_TRUE(store->SetOriginBoundCert("https://www.verisign.com/", "i", "j"));
  EXPECT_TRUE(store->GetOriginBoundCert("https://www.verisign.com/",
                                        &private_key,
                                        &cert));
  EXPECT_EQ("i", private_key);
  EXPECT_EQ("j", cert);
}

}  // namespace net
