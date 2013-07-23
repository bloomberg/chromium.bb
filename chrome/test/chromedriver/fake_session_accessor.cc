// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/fake_session_accessor.h"
#include "testing/gtest/include/gtest/gtest.h"

FakeSessionAccessor::FakeSessionAccessor(Session* session)
    : session_(session),
      is_accessed_(false),
      is_session_deleted_(false) {}

Session* FakeSessionAccessor::Access(
    scoped_ptr<base::AutoLock>* lock) {
  is_accessed_ = true;
  return session_;
}

bool FakeSessionAccessor::IsSessionDeleted() const {
  return is_session_deleted_;
}

void FakeSessionAccessor::DeleteSession() {
  ASSERT_TRUE(is_accessed_);
  is_session_deleted_ = true;
}

FakeSessionAccessor::~FakeSessionAccessor() {}
