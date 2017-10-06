// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/run_loop.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/render_pass_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

template <typename RendererType>
class CopyOutputScalingPixelTest
    : public cc::RendererPixelTest<RendererType>,
      public testing::WithParamInterface<std::tuple<int, int, int, int>> {
 public:
  // Include the public accessor method from the parent class.
  using cc::RendererPixelTest<RendererType>::renderer;

  // This tests that copy requests requesting scaled results execute correctly.
  // The test procedure creates a scene similar to the wall art that can be
  // found in the stairwell of a certain Google office building: A white
  // background" (W=white) and four blocks of different colors (r=red, g=green,
  // b=blue, y=yellow).
  //
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWrrrrWWWWWWWWggggWWWW
  //   WWWWrrrrWWWWWWWWggggWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWbbbbWWWWWWWWyyyyWWWW
  //   WWWWbbbbWWWWWWWWyyyyWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //
  // The scene is drawn, which also causes the copy request to execute. Then,
  // the resulting bitmap is compared against an expected bitmap.
  //
  // TODO(crbug/760348): This is the same as in software_renderer_unittest.cc.
  // It should be de-duped moved into a common location.
  void RunTest() {
    constexpr gfx::Size viewport_size = gfx::Size(24, 10);
    constexpr int x_block = 4;
    constexpr int y_block = 2;
    constexpr SkColor smaller_pass_colors[4] = {SK_ColorRED, SK_ColorGREEN,
                                                SK_ColorBLUE, SK_ColorYELLOW};
    constexpr SkColor root_pass_color = SK_ColorWHITE;

    // Test parameters: The scaling ratios for the copy requests.
    const gfx::Vector2d scale_from(std::get<0>(GetParam()),
                                   std::get<1>(GetParam()));
    const gfx::Vector2d scale_to(std::get<2>(GetParam()),
                                 std::get<3>(GetParam()));

    RenderPassList list;

    // Create the render passes drawn on top of the root render pass.
    RenderPass* smaller_passes[4];
    gfx::Rect smaller_pass_rects[4];
    int pass_id = 5;
    for (int i = 0; i < 4; ++i, --pass_id) {
      smaller_pass_rects[i] = gfx::Rect(
          i % 2 == 0 ? x_block : (viewport_size.width() - 2 * x_block),
          i / 2 == 0 ? y_block : (viewport_size.height() - 2 * y_block),
          x_block, y_block);
      smaller_passes[i] =
          AddRenderPass(&list, pass_id, smaller_pass_rects[i], gfx::Transform(),
                        cc::FilterOperations());
      cc::AddQuad(smaller_passes[i], smaller_pass_rects[i],
                  smaller_pass_colors[i]);
    }

    // Create the root render pass and add all the child passes to it.
    RenderPass* root_pass =
        cc::AddRenderPass(&list, pass_id, gfx::Rect(viewport_size),
                          gfx::Transform(), cc::FilterOperations());
    for (int i = 0; i < 4; ++i)
      cc::AddRenderPassQuad(root_pass, smaller_passes[i]);
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), root_pass_color);

    renderer()->DecideRenderPassAllocationsForFrame(list);

    // Make a copy request and execute it by drawing a frame. A subset of the
    // viewport is requested, to test that scaled offsets are being computed
    // correctly as well.
    const gfx::Rect copy_rect(x_block, y_block,
                              viewport_size.width() - 2 * x_block,
                              viewport_size.height() - 2 * y_block);
    std::unique_ptr<CopyOutputResult> result;
    {
      base::RunLoop loop;
      std::unique_ptr<CopyOutputRequest> request(new CopyOutputRequest(
          CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](std::unique_ptr<CopyOutputResult>* test_result,
                 const base::Closure& quit_closure,
                 std::unique_ptr<CopyOutputResult> result_from_renderer) {
                *test_result = std::move(result_from_renderer);
                quit_closure.Run();
              },
              &result, loop.QuitClosure())));
      request->set_area(copy_rect);
      request->SetScaleRatio(scale_from, scale_to);
      list.back()->copy_requests.push_back(std::move(request));
      renderer()->DrawFrame(&list, 1.0f, viewport_size);
      loop.Run();
    }

    // Check that the result succeeded and provides a bitmap of the expected
    // size.
    const gfx::Rect expected_result_rect =
        copy_output::ComputeResultRect(copy_rect, scale_from, scale_to);
    EXPECT_EQ(expected_result_rect, result->rect());
    const SkBitmap result_bitmap = result->AsSkBitmap();
    ASSERT_TRUE(result_bitmap.readyToDraw());
    ASSERT_EQ(expected_result_rect.width(), result_bitmap.width());
    ASSERT_EQ(expected_result_rect.height(), result_bitmap.height());

    // Create the "expected result" bitmap.
    SkBitmap expected_bitmap;
    expected_bitmap.allocN32Pixels(expected_result_rect.width(),
                                   expected_result_rect.height());
    expected_bitmap.eraseColor(root_pass_color);
    for (int i = 0; i < 4; ++i) {
      gfx::Rect rect = smaller_pass_rects[i] - copy_rect.OffsetFromOrigin();
      rect = copy_output::ComputeResultRect(rect, scale_from, scale_to);
      expected_bitmap.erase(
          smaller_pass_colors[i],
          SkIRect{rect.x(), rect.y(), rect.right(), rect.bottom()});
    }

    // Do an approximate comparison of the result bitmap to the expected one to
    // confirm the position and size of the color values in the result is
    // correct. Allow for pixel values to be a bit off: The scaler algorithms
    // are not using a naïve box filter, and so will blend things together at
    // edge boundaries.
    int num_bad_pixels = 0;
    gfx::Point first_failure_position;
    for (int y = 0; y < expected_bitmap.height(); ++y) {
      for (int x = 0; x < expected_bitmap.width(); ++x) {
        const SkColor expected = expected_bitmap.getColor(x, y);
        const SkColor actual = result_bitmap.getColor(x, y);
        const bool red_bad =
            (SkColorGetR(expected) < 0x80) != (SkColorGetR(actual) < 0x80);
        const bool green_bad =
            (SkColorGetG(expected) < 0x80) != (SkColorGetG(actual) < 0x80);
        const bool blue_bad =
            (SkColorGetB(expected) < 0x80) != (SkColorGetB(actual) < 0x80);
        const bool alpha_bad =
            (SkColorGetA(expected) < 0x80) != (SkColorGetA(actual) < 0x80);
        if (red_bad || green_bad || blue_bad || alpha_bad) {
          if (num_bad_pixels == 0)
            first_failure_position = gfx::Point(x, y);
          ++num_bad_pixels;
        }
      }
    }
    EXPECT_EQ(0, num_bad_pixels)
        << "First failure position at: " << first_failure_position.ToString()
        << "\nExpected bitmap: " << cc::GetPNGDataUrl(expected_bitmap)
        << "\nActual bitmap: " << cc::GetPNGDataUrl(result_bitmap);
  }
};

// Parameters common to all test instantiations. These are tuples consisting of
// {scale_from_x, scale_from_y, scale_to_x, scale_to_y}.
const auto kParameters = testing::Values(std::make_tuple(1, 1, 1, 1),
                                         std::make_tuple(1, 1, 2, 1),
                                         std::make_tuple(1, 1, 3, 1),
                                         std::make_tuple(1, 1, 1, 2),
                                         std::make_tuple(1, 1, 1, 3),
                                         std::make_tuple(2, 1, 1, 1),
                                         std::make_tuple(1, 2, 1, 1),
                                         std::make_tuple(2, 2, 1, 1));

using GLCopyOutputScalingPixelTest = CopyOutputScalingPixelTest<GLRenderer>;
TEST_P(GLCopyOutputScalingPixelTest, ScaledCopyOfDrawnFrame) {
  RunTest();
}
INSTANTIATE_TEST_CASE_P(, GLCopyOutputScalingPixelTest, kParameters);

using SoftwareCopyOutputScalingPixelTest =
    CopyOutputScalingPixelTest<SoftwareRenderer>;
TEST_P(SoftwareCopyOutputScalingPixelTest, ScaledCopyOfDrawnFrame) {
  RunTest();
}
INSTANTIATE_TEST_CASE_P(, SoftwareCopyOutputScalingPixelTest, kParameters);

}  // namespace
}  // namespace viz
