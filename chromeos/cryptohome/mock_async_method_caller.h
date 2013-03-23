// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_
#define CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cryptohome {

class MockAsyncMethodCaller : public AsyncMethodCaller {
 public:
  static const char kFakeAttestationEnrollRequest[];
  static const char kFakeAttestationCertRequest[];
  static const char kFakeAttestationCert[];
  static const char kFakeSanitizedUsername[];

  MockAsyncMethodCaller();
  virtual ~MockAsyncMethodCaller();

  void SetUp(bool success, MountError return_code);

  MOCK_METHOD3(AsyncCheckKey, void(const std::string& user_email,
                                   const std::string& passhash,
                                   Callback callback));
  MOCK_METHOD4(AsyncMigrateKey, void(const std::string& user_email,
                                     const std::string& old_hash,
                                     const std::string& new_hash,
                                     Callback callback));
  MOCK_METHOD4(AsyncMount, void(const std::string& user_email,
                                const std::string& passhash,
                                int flags,
                                Callback callback));
  MOCK_METHOD1(AsyncMountGuest, void(Callback callback));
  MOCK_METHOD2(AsyncRemove, void(const std::string& user_email,
                                 Callback callback));
  MOCK_METHOD1(AsyncTpmAttestationCreateEnrollRequest,
               void(const DataCallback& callback));
  MOCK_METHOD2(AsyncTpmAttestationEnroll,
               void(const std::string& pca_response, const Callback& callback));
  MOCK_METHOD2(AsyncTpmAttestationCreateCertRequest,
               void(bool is_cert_for_owner, const DataCallback& callback));
  MOCK_METHOD2(AsyncTpmAttestationFinishCertRequest,
               void(const std::string& pca_response,
                    const DataCallback& callback));
  MOCK_METHOD2(AsyncGetSanitizedUsername,
               void(const std::string& user,
                    const DataCallback& callback));

 private:
  bool success_;
  MountError return_code_;

  void DoCallback(Callback callback);
  // Default fakes for attestation calls.
  void FakeCreateEnrollRequest(const DataCallback& callback);
  void FakeCreateCertRequest(const DataCallback& callback);
  void FakeFinishCertRequest(const DataCallback& callback);
  void FakeGetSanitizedUsername(const DataCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(MockAsyncMethodCaller);
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_
