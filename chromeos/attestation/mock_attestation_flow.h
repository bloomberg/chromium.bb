// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace attestation {

// A fake server proxy which just appends "_response" to every request.
class FakeServerProxy : public ServerProxy {
 public:
  FakeServerProxy() : result_(true) {}
  virtual ~FakeServerProxy() {}

  void set_result(bool result) {
    result_ = result;
  }
  void SendEnrollRequest(const std::string& request,
                         const DataCallback& callback) {
    callback.Run(result_, request + "_response");
  }
  void SendCertificateRequest(const std::string& request,
                              const DataCallback& callback) {
    callback.Run(result_, request + "_response");
  }

 private:
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerProxy);
};

class MockServerProxy : public ServerProxy {
 public:
  MockServerProxy();
  virtual ~MockServerProxy();

  void DeferToFake(bool result);
  MOCK_METHOD2(SendEnrollRequest,
               void(const std::string&, const DataCallback&));
  MOCK_METHOD2(SendCertificateRequest,
               void(const std::string&, const DataCallback&));

 private:
  FakeServerProxy fake_;
};

// This class can be used to mock AttestationFlow callbacks.
class MockObserver {
 public:
  MockObserver();
  virtual ~MockObserver();

  MOCK_METHOD2(MockCertificateCallback, void(bool, const std::string&));
};

}  // namespace attestation
}  // namespace chromeos
