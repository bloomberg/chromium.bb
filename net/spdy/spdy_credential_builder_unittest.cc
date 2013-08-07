// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_credential_builder.h"

#include "base/threading/sequenced_worker_pool.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "net/cert/asn1_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/default_server_bound_cert_store.h"
#include "net/ssl/server_bound_cert_service.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const static size_t kSlot = 2;
const static char kSecretPrefix[] =
    "SPDY CREDENTIAL ChannelID\0client -> server";

void CreateCertAndKey(std::string* cert, std::string* key) {
  // TODO(rch): Share this code with ServerBoundCertServiceTest.
  scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool =
      new base::SequencedWorkerPool(1, "CreateCertAndKey");
  scoped_ptr<ServerBoundCertService> server_bound_cert_service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL),
                                 sequenced_worker_pool));

  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;
  int rv = server_bound_cert_service->GetDomainBoundCert(
      "www.google.com", key, cert,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  sequenced_worker_pool->Shutdown();
}

}  // namespace

class SpdyCredentialBuilderTest : public testing::Test {
 public:
  SpdyCredentialBuilderTest() {
    CreateCertAndKey(&cert_, &key_);
  }

 protected:
  int Build() {
    return SpdyCredentialBuilder::Build(
        MockClientSocket::kTlsUnique, key_, cert_, kSlot, &credential_);
  }

  std::string GetCredentialSecret() {
    return SpdyCredentialBuilder::GetCredentialSecret(
        MockClientSocket::kTlsUnique);
  }

  std::string cert_;
  std::string key_;
  SpdyCredential credential_;
  MockECSignatureCreatorFactory ec_signature_creator_factory_;
};

// http://crbug.com/142833, http://crbug.com/140991. The following tests fail
// with OpenSSL due to the unimplemented ec_private_key_openssl.cc.
#if defined(USE_OPENSSL)
#define MAYBE_GetCredentialSecret DISABLED_GetCredentialSecret
#else
#define MAYBE_GetCredentialSecret GetCredentialSecret
#endif

TEST_F(SpdyCredentialBuilderTest, MAYBE_GetCredentialSecret) {
  std::string secret_str(kSecretPrefix, arraysize(kSecretPrefix));
  secret_str.append(MockClientSocket::kTlsUnique);

  EXPECT_EQ(secret_str, GetCredentialSecret());
}

#if defined(USE_OPENSSL)
#define MAYBE_Succeeds DISABLED_Succeeds
#else
#define MAYBE_Succeeds Succeeds
#endif

TEST_F(SpdyCredentialBuilderTest, MAYBE_Succeeds) {
  EXPECT_EQ(OK, Build());
}

#if defined(USE_OPENSSL)
#define MAYBE_SetsSlotCorrectly DISABLED_SetsSlotCorrectly
#else
#define MAYBE_SetsSlotCorrectly SetsSlotCorrectly
#endif

TEST_F(SpdyCredentialBuilderTest, MAYBE_SetsSlotCorrectly) {
  ASSERT_EQ(OK, Build());
  EXPECT_EQ(kSlot, credential_.slot);
}

#if defined(USE_OPENSSL)
#define MAYBE_SetsCertCorrectly DISABLED_SetsCertCorrectly
#else
#define MAYBE_SetsCertCorrectly SetsCertCorrectly
#endif

TEST_F(SpdyCredentialBuilderTest, MAYBE_SetsCertCorrectly) {
  ASSERT_EQ(OK, Build());
  base::StringPiece spki;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(cert_, &spki));
  base::StringPiece spk;
  ASSERT_TRUE(asn1::ExtractSubjectPublicKeyFromSPKI(spki, &spk));
  EXPECT_EQ(1u, credential_.certs.size());
  EXPECT_EQ(0, (int)spk[0]);
  EXPECT_EQ(4, (int)spk[1]);
  EXPECT_EQ(spk.substr(2, spk.length()).as_string(), credential_.certs[0]);
}

#if defined(USE_OPENSSL)
#define MAYBE_SetsProofCorrectly DISABLED_SetsProofCorrectly
#else
#define MAYBE_SetsProofCorrectly SetsProofCorrectly
#endif

TEST_F(SpdyCredentialBuilderTest, MAYBE_SetsProofCorrectly) {
  ASSERT_EQ(OK, Build());
  base::StringPiece spki;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(cert_, &spki));
  std::vector<uint8> spki_data(spki.data(),
                               spki.data() + spki.size());
  std::vector<uint8> key_data(key_.data(),
                              key_.data() + key_.length());
  std::vector<uint8> proof_data;
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          ServerBoundCertService::kEPKIPassword, key_data, spki_data));
  scoped_ptr<crypto::ECSignatureCreator> creator(
      crypto::ECSignatureCreator::Create(private_key.get()));
  std::string secret = GetCredentialSecret();
  creator->Sign(reinterpret_cast<const unsigned char *>(secret.data()),
                secret.length(), &proof_data);

  std::string proof(proof_data.begin(), proof_data.end());
  EXPECT_EQ(proof, credential_.proof);
}

}  // namespace net
