// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"

namespace media {

TEST(OmxTest, SimpleInit) {
  EXPECT_EQ(OMX_ErrorNone, OMX_Init());
  EXPECT_EQ(OMX_ErrorNone, OMX_Deinit());
}

TEST(OmxTest, GetComponentsOfRole) {
  OMX_U32 roles = 0;
  OMX_U8** component_names = NULL;
  char role[] = "video_decoder";
  const int kSize = 256;

  EXPECT_EQ(OMX_ErrorNone, OMX_Init());
  EXPECT_EQ(OMX_ErrorNone, OMX_GetComponentsOfRole(role, &roles, 0));

  // TODO(hclam): Should assert the component number.
  LOG(INFO) << "Video decoder components: " << roles;

  if (roles) {
    component_names = new OMX_U8*[roles];
    for (size_t i = 0; i < roles; ++i)
      component_names[i] = new OMX_U8[kSize];

    OMX_U32 roles_backup = roles;
    EXPECT_EQ(OMX_ErrorNone,
              OMX_GetComponentsOfRole(role, &roles, component_names));
    ASSERT_EQ(roles_backup, roles);

    for (size_t i = 0; i < roles; ++i)
      delete [] component_names[i];
    delete [] component_names;
  }
  EXPECT_EQ(OMX_ErrorNone, OMX_Deinit());
}

}  // namespace media
