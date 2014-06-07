/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_manager.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/test/athena_test_base.h"

typedef athena::test::AthenaTestBase ActivityManagerTest;

TEST_F(ActivityManagerTest, Basic) {
  athena::ActivityManager::Get()->AddActivity(
      athena::ActivityFactory::Get()->CreateWebActivity(NULL, GURL()));
}
