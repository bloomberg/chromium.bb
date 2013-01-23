// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/mock_attestation_flow.h"

#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

namespace chromeos {
namespace attestation {

MockServerProxy::MockServerProxy() {}

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

}  // namespace attestation
}  // namespace chromeos
