// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#endif

namespace net {

namespace {

#if defined(USE_NSS)
class StubCryptoModuleDelegate : public crypto::NSSCryptoModuleDelegate {
  public:
   explicit StubCryptoModuleDelegate(crypto::ScopedPK11Slot slot)
       : slot_(slot.Pass()) {}

   virtual std::string RequestPassword(const std::string& slot_name,
                                       bool retry,
                                       bool* cancelled) OVERRIDE{
     return std::string();
   }

   virtual crypto::ScopedPK11Slot RequestSlot() OVERRIDE {
     return crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get()));
   }

  private:
   crypto::ScopedPK11Slot slot_;
};
#endif

class KeygenHandlerTest : public ::testing::Test {
 public:
  KeygenHandlerTest() {}
  virtual ~KeygenHandlerTest() {}

  scoped_ptr<KeygenHandler> CreateKeygenHandler() {
    scoped_ptr<KeygenHandler> handler(new KeygenHandler(
        768, "some challenge", GURL("http://www.example.com")));
#if defined(USE_NSS)
    handler->set_crypto_module_delegate(
        scoped_ptr<crypto::NSSCryptoModuleDelegate>(
            new StubCryptoModuleDelegate(
                crypto::ScopedPK11Slot(crypto::GetPersistentNSSKeySlot()))));
#endif
    return handler.Pass();
  }

 private:
#if defined(USE_NSS)
  crypto::ScopedTestNSSDB test_nss_db_;
#endif
};

// Assert that |result| is a valid output for KeygenHandler given challenge
// string of |challenge|.
void AssertValidSignedPublicKeyAndChallenge(const std::string& result,
                                            const std::string& challenge) {
  ASSERT_GT(result.length(), 0U);

  // Verify it's valid base64:
  std::string spkac;
  ASSERT_TRUE(base::Base64Decode(result, &spkac));
  // In lieu of actually parsing and validating the DER data,
  // just check that it exists and has a reasonable length.
  // (It's almost always 590 bytes, but the DER encoding of the random key
  // and signature could sometimes be a few bytes different.)
  ASSERT_GE(spkac.length(), 200U);
  ASSERT_LE(spkac.length(), 300U);

  // NOTE:
  // The value of |result| can be validated by prefixing 'SPKAC=' to it
  // and piping it through
  //   openssl spkac -verify
  // whose output should look like:
  //   Netscape SPKI:
  //     Public Key Algorithm: rsaEncryption
  //     RSA Public Key: (2048 bit)
  //     Modulus (2048 bit):
  //         00:b6:cc:14:c9:43:b5:2d:51:65:7e:11:8b:80:9e: .....
  //     Exponent: 65537 (0x10001)
  //     Challenge String: some challenge
  //     Signature Algorithm: md5WithRSAEncryption
  //         92:f3:cc:ff:0b:d3:d0:4a:3a:4c:ba:ff:d6:38:7f:a5:4b:b5: .....
  //   Signature OK
  //
  // The value of |spkac| can be ASN.1-parsed with:
  //    openssl asn1parse -inform DER
}

TEST_F(KeygenHandlerTest, SmokeTest) {
  scoped_ptr<KeygenHandler> handler(CreateKeygenHandler());
  handler->set_stores_key(false);  // Don't leave the key-pair behind
  std::string result = handler->GenKeyAndSignChallenge();
  VLOG(1) << "KeygenHandler produced: " << result;
  AssertValidSignedPublicKeyAndChallenge(result, "some challenge");
}

void ConcurrencyTestCallback(const std::string& challenge,
                             base::WaitableEvent* event,
                             scoped_ptr<KeygenHandler> handler,
                             std::string* result) {
  // We allow Singleton use on the worker thread here since we use a
  // WaitableEvent to synchronize, so it's safe.
  base::ThreadRestrictions::ScopedAllowSingleton scoped_allow_singleton;
  handler->set_stores_key(false);  // Don't leave the key-pair behind.
  *result = handler->GenKeyAndSignChallenge();
  event->Signal();
#if defined(USE_NSS)
  // Detach the thread from NSPR.
  // Calling NSS functions attaches the thread to NSPR, which stores
  // the NSPR thread ID in thread-specific data.
  // The threads in our thread pool terminate after we have called
  // PR_Cleanup.  Unless we detach them from NSPR, net_unittests gets
  // segfaults on shutdown when the threads' thread-specific data
  // destructors run.
  PR_DetachThread();
#endif
}

// We asynchronously generate the keys so as not to hang up the IO thread. This
// test tries to catch concurrency problems in the keygen implementation.
TEST_F(KeygenHandlerTest, ConcurrencyTest) {
  const int NUM_HANDLERS = 5;
  base::WaitableEvent* events[NUM_HANDLERS] = { NULL };
  std::string results[NUM_HANDLERS];
  for (int i = 0; i < NUM_HANDLERS; i++) {
    scoped_ptr<KeygenHandler> handler(CreateKeygenHandler());
    events[i] = new base::WaitableEvent(false, false);
    base::WorkerPool::PostTask(FROM_HERE,
                               base::Bind(ConcurrencyTestCallback,
                                          "some challenge",
                                          events[i],
                                          base::Passed(&handler),
                                          &results[i]),
                               true);
  }

  for (int i = 0; i < NUM_HANDLERS; i++) {
    // Make sure the job completed
    events[i]->Wait();
    delete events[i];
    events[i] = NULL;

    VLOG(1) << "KeygenHandler " << i << " produced: " << results[i];
    AssertValidSignedPublicKeyAndChallenge(results[i], "some challenge");
  }
}

}  // namespace

}  // namespace net
