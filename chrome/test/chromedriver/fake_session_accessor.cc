// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/fake_session_accessor.h"

FakeSessionAccessor::FakeSessionAccessor(Session* session)
    : session_(session) {}

Session* FakeSessionAccessor::Access(
    scoped_ptr<base::AutoLock>* lock) {
  return session_;
}

FakeSessionAccessor::~FakeSessionAccessor() {}
