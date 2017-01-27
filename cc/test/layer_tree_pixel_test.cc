// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "cc/base/switches.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_compositor_frame_sink.h"
#include "cc/test/test_in_process_context_provider.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/gl_in_process_context.h"

using gpu::gles2::GLES2Interface;

namespace cc {

LayerTreePixelTest::LayerTreePixelTest()
    : pixel_comparator_(new ExactPixelComparator(true)),
      test_type_(PIXEL_TEST_GL),
      pending_texture_mailbox_callbacks_(0) {}

LayerTreePixelTest::~LayerTreePixelTest() {}

std::unique_ptr<TestCompositorFrameSink>
    LayerTreePixelTest::CreateCompositorFrameSink(
        scoped_refptr<ContextProvider>,
        scoped_refptr<ContextProvider>) {
  scoped_refptr<TestInProcessContextProvider> compositor_context_provider;
  scoped_refptr<TestInProcessContextProvider> worker_context_provider;
  if (test_type_ == PIXEL_TEST_GL) {
    compositor_context_provider = new TestInProcessContextProvider(nullptr);
    worker_context_provider =
        new TestInProcessContextProvider(compositor_context_provider.get());
  }
  bool synchronous_composite =
      !HasImplThread() &&
      !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
  // Allow resource reclaiming for partial raster tests to get back
  // resources from the Display.
  bool force_disable_reclaim_resources = false;
  auto delegating_output_surface = base::MakeUnique<TestCompositorFrameSink>(
      compositor_context_provider, std::move(worker_context_provider),
      shared_bitmap_manager(), gpu_memory_buffer_manager(), RendererSettings(),
      ImplThreadTaskRunner(), synchronous_composite,
      force_disable_reclaim_resources);
  delegating_output_surface->SetEnlargePassTextureAmount(
      enlarge_texture_amount_);
  return delegating_output_surface;
}

std::unique_ptr<OutputSurface>
LayerTreePixelTest::CreateDisplayOutputSurfaceOnThread(
    scoped_refptr<ContextProvider> compositor_context_provider) {
  std::unique_ptr<PixelTestOutputSurface> display_output_surface;
  if (test_type_ == PIXEL_TEST_GL) {
    // Pixel tests use a separate context for the Display to more closely
    // mimic texture transport from the renderer process to the Display
    // compositor.
    auto display_context_provider =
        make_scoped_refptr(new TestInProcessContextProvider(nullptr));
    display_context_provider->BindToCurrentThread();

    bool flipped_output_surface = false;
    display_output_surface = base::MakeUnique<PixelTestOutputSurface>(
        std::move(display_context_provider), flipped_output_surface);
  } else {
    display_output_surface = base::MakeUnique<PixelTestOutputSurface>(
        base::MakeUnique<SoftwareOutputDevice>());
  }
  return std::move(display_output_surface);
}

std::unique_ptr<CopyOutputRequest>
LayerTreePixelTest::CreateCopyOutputRequest() {
  return CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&LayerTreePixelTest::ReadbackResult, base::Unretained(this)));
}

void LayerTreePixelTest::ReadbackResult(
    std::unique_ptr<CopyOutputResult> result) {
  ASSERT_TRUE(result->HasBitmap());
  result_bitmap_ = result->TakeBitmap();
  EndTest();
}

void LayerTreePixelTest::BeginTest() {
  Layer* target =
      readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
  target->RequestCopyOfOutput(CreateCopyOutputRequest());
  PostSetNeedsCommitToMainThread();
}

void LayerTreePixelTest::AfterTest() {
  base::FilePath test_data_dir;
  EXPECT_TRUE(PathService::Get(CCPaths::DIR_TEST_DATA, &test_data_dir));
  base::FilePath ref_file_path = test_data_dir.Append(ref_file_);

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    EXPECT_TRUE(WritePNGFile(*result_bitmap_, ref_file_path, true));
  EXPECT_TRUE(MatchesPNGFile(*result_bitmap_,
                             ref_file_path,
                             *pixel_comparator_));
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::CreateSolidColorLayer(
    const gfx::Rect& rect, SkColor color) {
  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetIsDrawable(true);
  layer->SetBounds(rect.size());
  layer->SetPosition(gfx::PointF(rect.origin()));
  layer->SetBackgroundColor(color);
  return layer;
}

void LayerTreePixelTest::EndTest() {
  // Drop TextureMailboxes on the main thread so that they can be cleaned up and
  // the pending callbacks will fire.
  for (size_t i = 0; i < texture_layers_.size(); ++i) {
    texture_layers_[i]->SetTextureMailbox(TextureMailbox(), nullptr);
  }

  TryEndTest();
}

void LayerTreePixelTest::TryEndTest() {
  if (!result_bitmap_)
    return;
  if (pending_texture_mailbox_callbacks_)
    return;
  LayerTreeTest::EndTest();
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::
    CreateSolidColorLayerWithBorder(
        const gfx::Rect& rect, SkColor color,
        int border_width, SkColor border_color) {
  scoped_refptr<SolidColorLayer> layer = CreateSolidColorLayer(rect, color);
  scoped_refptr<SolidColorLayer> border_top = CreateSolidColorLayer(
      gfx::Rect(0, 0, rect.width(), border_width), border_color);
  scoped_refptr<SolidColorLayer> border_left = CreateSolidColorLayer(
      gfx::Rect(0,
                border_width,
                border_width,
                rect.height() - border_width * 2),
      border_color);
  scoped_refptr<SolidColorLayer> border_right =
      CreateSolidColorLayer(gfx::Rect(rect.width() - border_width,
                                      border_width,
                                      border_width,
                                      rect.height() - border_width * 2),
                            border_color);
  scoped_refptr<SolidColorLayer> border_bottom = CreateSolidColorLayer(
      gfx::Rect(0, rect.height() - border_width, rect.width(), border_width),
      border_color);
  layer->AddChild(border_top);
  layer->AddChild(border_left);
  layer->AddChild(border_right);
  layer->AddChild(border_bottom);
  return layer;
}

void LayerTreePixelTest::RunPixelTest(
    PixelTestType test_type,
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = NULL;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunSingleThreadedPixelTest(
    PixelTestType test_type,
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = NULL;
  ref_file_ = file_name;
  RunTest(CompositorMode::SINGLE_THREADED);
}

void LayerTreePixelTest::RunPixelTestWithReadbackTarget(
    PixelTestType test_type,
    scoped_refptr<Layer> content_root,
    Layer* target,
    base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = target;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::SetupTree() {
  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(content_root_->bounds());
  root->AddChild(content_root_);
  layer_tree_host()->SetRootLayer(root);
  LayerTreeTest::SetupTree();
}

std::unique_ptr<SkBitmap> LayerTreePixelTest::CopyTextureMailboxToBitmap(
    const gfx::Size& size,
    const TextureMailbox& texture_mailbox) {
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return nullptr;

  std::unique_ptr<gpu::GLInProcessContext> context =
      CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();

  if (texture_mailbox.sync_token().HasData())
    gl->WaitSyncTokenCHROMIUM(texture_mailbox.sync_token().GetConstData());

  GLuint texture_id = 0;
  gl->GenTextures(1, &texture_id);
  gl->BindTexture(GL_TEXTURE_2D, texture_id);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->ConsumeTextureCHROMIUM(texture_mailbox.target(), texture_mailbox.name());
  gl->BindTexture(GL_TEXTURE_2D, 0);

  GLuint fbo = 0;
  gl->GenFramebuffers(1, &fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  EXPECT_EQ(static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE),
            gl->CheckFramebufferStatus(GL_FRAMEBUFFER));

  std::unique_ptr<uint8_t[]> pixels(new uint8_t[size.GetArea() * 4]);
  gl->ReadPixels(0,
                 0,
                 size.width(),
                 size.height(),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.get());

  gl->DeleteFramebuffers(1, &fbo);
  gl->DeleteTextures(1, &texture_id);

  std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->allocN32Pixels(size.width(), size.height());

  uint8_t* out_pixels = static_cast<uint8_t*>(bitmap->getPixels());

  size_t row_bytes = size.width() * 4;
  size_t total_bytes = size.height() * row_bytes;
  for (size_t dest_y = 0; dest_y < total_bytes; dest_y += row_bytes) {
    // Flip Y axis.
    size_t src_y = total_bytes - dest_y - row_bytes;
    // Swizzle OpenGL -> Skia byte order.
    for (size_t x = 0; x < row_bytes; x += 4) {
      out_pixels[dest_y + x + SK_R32_SHIFT/8] = pixels.get()[src_y + x + 0];
      out_pixels[dest_y + x + SK_G32_SHIFT/8] = pixels.get()[src_y + x + 1];
      out_pixels[dest_y + x + SK_B32_SHIFT/8] = pixels.get()[src_y + x + 2];
      out_pixels[dest_y + x + SK_A32_SHIFT/8] = pixels.get()[src_y + x + 3];
    }
  }

  return bitmap;
}

void LayerTreePixelTest::Finish() {
  std::unique_ptr<gpu::GLInProcessContext> context =
      CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();
  gl->Finish();
}

}  // namespace cc
