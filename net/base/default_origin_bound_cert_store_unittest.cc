// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/default_origin_bound_cert_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
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
  virtual void Flush(const base::Closure& completion_task) OVERRIDE;

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

void MockPersistentStore::Flush(const base::Closure& completion_task) {
  NOTREACHED();
}

TEST(DefaultOriginBoundCertStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddOriginBoundCert(
      DefaultOriginBoundCertStore::OriginBoundCert(
          "https://encrypted.google.com/", CLIENT_CERT_RSA_SIGN, "a", "b"));
  persistent_store->AddOriginBoundCert(
      DefaultOriginBoundCertStore::OriginBoundCert(
          "https://www.verisign.com/", CLIENT_CERT_ECDSA_SIGN, "c", "d"));

  // Make sure certs load properly.
  DefaultOriginBoundCertStore store(persistent_store.get());
  EXPECT_EQ(2, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "e", "f");
  EXPECT_EQ(2, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.twitter.com/", CLIENT_CERT_RSA_SIGN, "g", "h");
  EXPECT_EQ(3, store.GetCertCount());
}

TEST(DefaultOriginBoundCertStoreTest, TestSettingAndGetting) {
  DefaultOriginBoundCertStore store(NULL);
  SSLClientCertType type;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_FALSE(store.GetOriginBoundCert("https://www.verisign.com/",
                                        &type,
                                        &private_key,
                                        &cert));
  EXPECT_TRUE(private_key.empty());
  EXPECT_TRUE(cert.empty());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "i", "j");
  EXPECT_TRUE(store.GetOriginBoundCert("https://www.verisign.com/",
                                       &type,
                                       &private_key,
                                       &cert));
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type);
  EXPECT_EQ("i", private_key);
  EXPECT_EQ("j", cert);
}

TEST(DefaultOriginBoundCertStoreTest, TestDuplicateCerts) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultOriginBoundCertStore store(persistent_store.get());

  SSLClientCertType type;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "a", "b");
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_ECDSA_SIGN, "c", "d");

  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_TRUE(store.GetOriginBoundCert("https://www.verisign.com/",
                                       &type,
                                       &private_key,
                                       &cert));
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type);
  EXPECT_EQ("c", private_key);
  EXPECT_EQ("d", cert);
}

TEST(DefaultOriginBoundCertStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultOriginBoundCertStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "a", "b");
  store.SetOriginBoundCert(
      "https://www.google.com/", CLIENT_CERT_RSA_SIGN, "c", "d");
  store.SetOriginBoundCert(
      "https://www.harvard.com/", CLIENT_CERT_RSA_SIGN, "e", "f");

  EXPECT_EQ(3, store.GetCertCount());
  store.DeleteAll();
  EXPECT_EQ(0, store.GetCertCount());
}

TEST(DefaultOriginBoundCertStoreTest, TestDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultOriginBoundCertStore store(persistent_store.get());

  SSLClientCertType type;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "a", "b");
  store.SetOriginBoundCert(
      "https://www.google.com/", CLIENT_CERT_ECDSA_SIGN, "c", "d");

  EXPECT_EQ(2, store.GetCertCount());
  store.DeleteOriginBoundCert("https://www.verisign.com/");
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_FALSE(store.GetOriginBoundCert("https://www.verisign.com/",
                                        &type,
                                        &private_key,
                                        &cert));
  EXPECT_TRUE(store.GetOriginBoundCert("https://www.google.com/",
                                       &type,
                                       &private_key,
                                       &cert));
  store.DeleteOriginBoundCert("https://www.google.com/");
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_FALSE(store.GetOriginBoundCert("https://www.google.com/",
                                        &type,
                                        &private_key,
                                        &cert));
}

TEST(DefaultOriginBoundCertStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultOriginBoundCertStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetCertCount());
  store.SetOriginBoundCert(
      "https://www.verisign.com/", CLIENT_CERT_RSA_SIGN, "a", "b");
  store.SetOriginBoundCert(
      "https://www.google.com/", CLIENT_CERT_ECDSA_SIGN, "c", "d");
  store.SetOriginBoundCert(
      "https://www.harvard.com/", CLIENT_CERT_RSA_SIGN, "e", "f");
  store.SetOriginBoundCert(
      "https://www.mit.com/", CLIENT_CERT_RSA_SIGN, "g", "h");

  EXPECT_EQ(4, store.GetCertCount());
  std::vector<OriginBoundCertStore::OriginBoundCert> certs;
  store.GetAllOriginBoundCerts(&certs);
  EXPECT_EQ(4u, certs.size());
}

}  // namespace net
