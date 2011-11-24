// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace base {

CancelableCallback::CancelableCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

CancelableCallback::CancelableCallback(const base::Closure& callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      callback_(callback) {
  DCHECK(!callback.is_null());
  InitializeForwarder();
}

CancelableCallback::~CancelableCallback() {}

void CancelableCallback::Cancel() {
  weak_factory_.InvalidateWeakPtrs();
  callback_.Reset();
}

bool CancelableCallback::IsCancelled() const {
  return callback_.is_null();
}

void CancelableCallback::Reset(const base::Closure& callback) {
  DCHECK(!callback.is_null());

  // Outstanding tasks (e.g., posted to a message loop) must not be called.
  Cancel();

  // |forwarder_| is no longer valid after Cancel(), so re-bind.
  InitializeForwarder();

  callback_ = callback;
}

const base::Closure& CancelableCallback::callback() const {
  return forwarder_;
}

void CancelableCallback::RunCallback() {
  callback_.Run();
}

void CancelableCallback::InitializeForwarder() {
  forwarder_ = base::Bind(&CancelableCallback::RunCallback,
                          weak_factory_.GetWeakPtr());
}

}  // namespace bind
