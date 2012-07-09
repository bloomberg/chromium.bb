// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GetErrorQueryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_.Initialize(gfx::Size(2, 2));
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(GetErrorQueryTest, Basic) {
  EXPECT_TRUE(GLTestHelper::HasExtension("GL_CHROMIUM_get_error_query"));

  GLuint query = 0;
  glGenQueriesEXT(1, &query);

  GLuint query_status = 0;
  GLuint result = 0;

  glBeginQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM, query);
  glEnable(GL_TEXTURE_2D);  // Generates an INVALID_ENUM error
  glEndQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM);

  glFinish();

  query_status = 0;
  result = 0;
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &result);
  EXPECT_TRUE(result);
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_EXT, &query_status);
  EXPECT_EQ(static_cast<uint32>(GL_INVALID_ENUM), query_status);
}

}  // namespace gpu


