// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate_test_utils.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"

namespace data_reduction_proxy {

TestDataReductionProxyEventStorageDelegate::
    TestDataReductionProxyEventStorageDelegate()
    : delegate_(nullptr) {
}

TestDataReductionProxyEventStorageDelegate::
    ~TestDataReductionProxyEventStorageDelegate() {
}

void TestDataReductionProxyEventStorageDelegate::SetStorageDelegate(
    DataReductionProxyEventStorageDelegate* delegate) {
  delegate_ = delegate;
}

void TestDataReductionProxyEventStorageDelegate::AddEvent(
    scoped_ptr<base::Value> event) {
  if (delegate_)
    delegate_->AddEvent(event.Pass());
}

void TestDataReductionProxyEventStorageDelegate::AddEnabledEvent(
    scoped_ptr<base::Value> event,
    bool enabled) {
  if (delegate_)
    delegate_->AddEnabledEvent(event.Pass(), enabled);
}

void TestDataReductionProxyEventStorageDelegate::AddAndSetLastBypassEvent(
    scoped_ptr<base::Value> event,
    int64 expiration_ticks) {
  if (delegate_)
    delegate_->AddAndSetLastBypassEvent(event.Pass(), expiration_ticks);
}

void TestDataReductionProxyEventStorageDelegate::
    AddEventAndSecureProxyCheckState(scoped_ptr<base::Value> event,
                                     SecureProxyCheckState state) {
  if (delegate_)
    delegate_->AddEventAndSecureProxyCheckState(event.Pass(), state);
}

}  // namespace data_reduction_proxy
