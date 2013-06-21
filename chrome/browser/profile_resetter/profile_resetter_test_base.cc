// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"

#include "chrome/browser/profile_resetter/profile_resetter.h"

ProfileResetterMockObject::ProfileResetterMockObject() {}

ProfileResetterMockObject::~ProfileResetterMockObject() {}

void ProfileResetterMockObject::RunLoop() {
  EXPECT_CALL(*this, Callback());
  runner_ = new content::MessageLoopRunner;
  runner_->Run();
}

void ProfileResetterMockObject::StopLoop() {
  DCHECK(runner_.get());
  Callback();
  runner_->Quit();
}

ProfileResetterTestBase::ProfileResetterTestBase() {}

ProfileResetterTestBase::~ProfileResetterTestBase() {}
