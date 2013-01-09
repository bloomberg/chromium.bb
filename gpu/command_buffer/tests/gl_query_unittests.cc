// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/threading/platform_thread.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class QueryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_.Initialize(GLManager::Options());
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(QueryTest, GetErrorBasic) {
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

TEST_F(QueryTest, DISABLED_LatencyQueryBasic) {
  EXPECT_TRUE(GLTestHelper::HasExtension(
                  "GL_CHROMIUM_command_buffer_latency_query"));

  GLuint query = 0;
  glGenQueriesEXT(1, &query);

  GLuint query_result = 0;
  GLuint available = 0;

  // First test a query with a ~1ms "latency".
  const unsigned int kExpectedLatencyMicroseconds = 2000;
  const unsigned int kTimePrecisionMicroseconds = 1000;

  glBeginQueryEXT(GL_LATENCY_QUERY_CHROMIUM, query);
  // Usually, we want to measure gpu-side latency, but we fake it by
  // adding client side latency for our test because it's easier.
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMicroseconds(kExpectedLatencyMicroseconds));
  glEndQueryEXT(GL_LATENCY_QUERY_CHROMIUM);

  glFinish();

  query_result = 0;
  available = 0;
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(available);
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_EXT, &query_result);
  EXPECT_GE(query_result, kExpectedLatencyMicroseconds
                          - kTimePrecisionMicroseconds);
  EXPECT_LE(query_result, kExpectedLatencyMicroseconds
                          + kTimePrecisionMicroseconds);

  // Then test a query with the lowest latency possible.
  glBeginQueryEXT(GL_LATENCY_QUERY_CHROMIUM, query);
  glEndQueryEXT(GL_LATENCY_QUERY_CHROMIUM);

  glFinish();

  query_result = 0;
  available = 0;
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(available);
  glGetQueryObjectuivEXT(query, GL_QUERY_RESULT_EXT, &query_result);

  EXPECT_LE(query_result, kTimePrecisionMicroseconds);
}

}  // namespace gpu


