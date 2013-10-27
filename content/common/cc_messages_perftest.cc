// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "cc/output/compositor_frame.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

using cc::CompositorFrame;
using cc::DelegatedFrameData;
using cc::DrawQuad;
using cc::PictureDrawQuad;
using cc::RenderPass;
using cc::SharedQuadState;

namespace content {
namespace {

class CCMessagesPerfTest : public testing::Test {};

const int kNumWarmupRuns = 10;
const int kNumRuns = 100;

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_1_4000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->shared_quad_state_list.push_back(SharedQuadState::Create());
  for (int i = 0; i < 4000; ++i) {
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  for (int i = 0; i < kNumWarmupRuns; ++i) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
  }

  base::TimeDelta min_time_delta;
  for (int i = 0; i < kNumRuns; ++i) {
    base::TimeTicks start = base::TimeTicks::HighResNow();
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
    base::TimeTicks end = base::TimeTicks::HighResNow();
    if (i == 0 || end - start < min_time_delta)
      min_time_delta = end - start;
  }

  perf_test::PrintResult("min_frame_serialization_time",
                         "",
                         "DelegatedFrame_ManyQuads_1_4000",
                         min_time_delta.InMicroseconds(),
                         "us",
                         true);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_1_100000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->shared_quad_state_list.push_back(SharedQuadState::Create());
  for (int i = 0; i < 100000; ++i) {
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  for (int i = 0; i < kNumWarmupRuns; ++i) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
  }

  base::TimeDelta min_time_delta;
  for (int i = 0; i < kNumRuns; ++i) {
    base::TimeTicks start = base::TimeTicks::HighResNow();
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
    base::TimeTicks end = base::TimeTicks::HighResNow();
    if (i == 0 || end - start < min_time_delta)
      min_time_delta = end - start;
  }

  perf_test::PrintResult("min_frame_serialization_time",
                         "",
                         "DelegatedFrame_ManyQuads_1_100000",
                         min_time_delta.InMicroseconds(),
                         "us",
                         true);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_4000_4000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  for (int i = 0; i < 4000; ++i) {
    render_pass->shared_quad_state_list.push_back(SharedQuadState::Create());
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  for (int i = 0; i < kNumWarmupRuns; ++i) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
  }

  base::TimeDelta min_time_delta;
  for (int i = 0; i < kNumRuns; ++i) {
    base::TimeTicks start = base::TimeTicks::HighResNow();
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
    base::TimeTicks end = base::TimeTicks::HighResNow();
    if (i == 0 || end - start < min_time_delta)
      min_time_delta = end - start;
  }

  perf_test::PrintResult("min_frame_serialization_time",
                         "",
                         "DelegatedFrame_ManyQuads_4000_4000",
                         min_time_delta.InMicroseconds(),
                         "us",
                         true);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_100000_100000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  for (int i = 0; i < 100000; ++i) {
    render_pass->shared_quad_state_list.push_back(SharedQuadState::Create());
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  for (int i = 0; i < kNumWarmupRuns; ++i) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
  }

  base::TimeDelta min_time_delta;
  for (int i = 0; i < kNumRuns; ++i) {
    base::TimeTicks start = base::TimeTicks::HighResNow();
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
    base::TimeTicks end = base::TimeTicks::HighResNow();
    if (i == 0 || end - start < min_time_delta)
      min_time_delta = end - start;
  }

  perf_test::PrintResult("min_frame_serialization_time",
                         "",
                         "DelegatedFrame_ManyQuads_100000_100000",
                         min_time_delta.InMicroseconds(),
                         "us",
                         true);
}

TEST_F(CCMessagesPerfTest,
       DelegatedFrame_ManyRenderPasses_10000_100) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data.reset(new DelegatedFrameData);

  for (int i = 0; i < 1000; ++i) {
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    for (int j = 0; j < 100; ++j) {
      render_pass->shared_quad_state_list.push_back(SharedQuadState::Create());
      render_pass->quad_list.push_back(
          PictureDrawQuad::Create().PassAs<DrawQuad>());
    }
    frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());
  }

  for (int i = 0; i < kNumWarmupRuns; ++i) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
  }

  base::TimeDelta min_time_delta;
  for (int i = 0; i < kNumRuns; ++i) {
    base::TimeTicks start = base::TimeTicks::HighResNow();
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, *frame);
    base::TimeTicks end = base::TimeTicks::HighResNow();
    if (i == 0 || end - start < min_time_delta)
      min_time_delta = end - start;
  }

  perf_test::PrintResult("min_frame_serialization_time",
                         "",
                         "DelegatedFrame_ManyRenderPasses_10000_100",
                         min_time_delta.InMicroseconds(),
                         "us",
                         true);
}

}  // namespace
}  // namespace content
