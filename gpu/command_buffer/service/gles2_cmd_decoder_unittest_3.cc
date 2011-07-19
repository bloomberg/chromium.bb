// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest3 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest3() { }
};

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_3_autogen.h"

}  // namespace gles2
}  // namespace gpu

