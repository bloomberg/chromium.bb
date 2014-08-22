// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_util.h"

#include "base/basictypes.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace util {

TEST(TaskManagerUtilTest, GetMessagePrefixID) {
  struct Configuration {
    bool is_app;
    bool is_extension;
    bool is_incognito;
    bool is_prerender;
    bool is_background;
    int expected_result;
  };
  const Configuration configs[] = {
      // Use implicit int->bool conversion to save space and keep alignment.
      {1, 0, 0, 0, 1, IDS_TASK_MANAGER_BACKGROUND_PREFIX},
      {1, 0, 1, 0, 0, IDS_TASK_MANAGER_APP_INCOGNITO_PREFIX},
      {1, 0, 0, 0, 0, IDS_TASK_MANAGER_APP_PREFIX},
      {0, 1, 1, 0, 0, IDS_TASK_MANAGER_EXTENSION_INCOGNITO_PREFIX},
      {0, 1, 0, 0, 0, IDS_TASK_MANAGER_EXTENSION_PREFIX},
      {0, 0, 0, 1, 0, IDS_TASK_MANAGER_PRERENDER_PREFIX},
      {0, 0, 1, 0, 0, IDS_TASK_MANAGER_TAB_INCOGNITO_PREFIX},
      {0, 0, 0, 0, 0, IDS_TASK_MANAGER_TAB_PREFIX}};
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(configs); ++i) {
    EXPECT_EQ(configs[i].expected_result,
              GetMessagePrefixID(configs[i].is_app,
                                 configs[i].is_extension,
                                 configs[i].is_incognito,
                                 configs[i].is_prerender,
                                 configs[i].is_background));
  }
}

} //  namespace util

} //  namespace task_manager
