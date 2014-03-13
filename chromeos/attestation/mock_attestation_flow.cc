// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/mock_attestation_flow.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DefaultValue;
using testing::Invoke;

namespace chromeos {
namespace attestation {

FakeServerProxy::FakeServerProxy() : result_(true) {}

FakeServerProxy::~FakeServerProxy() {}

void FakeServerProxy::SendEnrollRequest(const std::string& request,
                                        const DataCallback& callback) {
  callback.Run(result_, request + "_response");
}

void FakeServerProxy::SendCertificateRequest(const std::string& request,
                                             const DataCallback& callback) {
  callback.Run(result_, request + "_response");
}

MockServerProxy::MockServerProxy() {
  DefaultValue<PrivacyCAType>::Set(DEFAULT_PCA);
}

MockServerProxy::~MockServerProxy() {}

void MockServerProxy::DeferToFake(bool success) {
  fake_.set_result(success);
  ON_CALL(*this, SendEnrollRequest(_, _))
      .WillByDefault(Invoke(&fake_, &FakeServerProxy::SendEnrollRequest));
  ON_CALL(*this, SendCertificateRequest(_, _))
      .WillByDefault(Invoke(&fake_, &FakeServerProxy::SendCertificateRequest));
}

MockObserver::MockObserver() {}

MockObserver::~MockObserver() {}

MockAttestationFlow::MockAttestationFlow()
    : AttestationFlow(NULL, NULL, scoped_ptr<ServerProxy>()) {}

MockAttestationFlow::~MockAttestationFlow() {}

}  // namespace attestation
}  // namespace chromeos
