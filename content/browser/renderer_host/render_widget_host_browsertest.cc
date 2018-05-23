// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class RenderWidgetHostBrowserTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostBrowserTest,
                       ProhibitsCopyRequestsFromRenderer) {
  NavigateToURL(shell(), GURL("data:text/html,<!doctype html>"
                              "<body style='background-color: red;'></body>"));

  // Wait for the view's surface to become available.
  auto* const view = static_cast<RenderWidgetHostViewBase*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  while (!view->IsSurfaceAvailableForCopy()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(250));
    run_loop.Run();
  }

  // The browser process should be allowed to make a CopyOutputRequest.
  bool did_receive_copy_result = false;
  base::RunLoop run_loop;
  view->CopyFromSurface(gfx::Rect(), gfx::Size(),
                        base::BindOnce(
                            [](bool* success, base::OnceClosure quit_closure,
                               const SkBitmap& bitmap) {
                              *success = !bitmap.drawsNothing();
                              std::move(quit_closure).Run();
                            },
                            &did_receive_copy_result, run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(did_receive_copy_result);

  // Create a simulated-from-renderer CompositorFrame with a CopyOutputRequest.
  viz::CompositorFrame frame;
  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  const gfx::Rect output_rect =
      gfx::Rect(view->GetCompositorViewportPixelSize());
  pass->SetNew(1 /* render pass id */, output_rect, output_rect,
               gfx::Transform());
  bool did_receive_aborted_copy_result = false;
  pass->copy_requests.push_back(std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(
          [](bool* got_nothing, std::unique_ptr<viz::CopyOutputResult> result) {
            *got_nothing = result->IsEmpty();
          },
          &did_receive_aborted_copy_result)));
  frame.render_pass_list.push_back(std::move(pass));

  // Submit the frame and expect the renderer to be instantly killed.
  auto* const host = RenderWidgetHostImpl::From(view->GetRenderWidgetHost());
  RenderProcessHostKillWaiter waiter(host->GetProcess());
  host->SubmitCompositorFrame(viz::LocalSurfaceId(), std::move(frame), nullptr,
                              0);
  base::Optional<bad_message::BadMessageReason> result = waiter.Wait();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bad_message::RWH_COPY_REQUEST_ATTEMPT, *result);

  // Check that the copy request result callback received an empty result. In a
  // normal browser, the requestor (in the render process) might never see a
  // response to the copy request before the process is killed. Nevertheless,
  // ensure the result is empty, just in case there is a race.
  EXPECT_TRUE(did_receive_aborted_copy_result);
}

}  // namespace content
