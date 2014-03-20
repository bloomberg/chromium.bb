// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const gfx::Rect kOverlayRect(0, 0, 128, 128);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);

void MailboxReleased(unsigned sync_point, bool lost_resource) {}

class SingleOverlayValidator : public OverlayCandidateValidator {
 public:
  virtual void CheckOverlaySupport(OverlayCandidateList* surfaces) OVERRIDE;
};

void SingleOverlayValidator::CheckOverlaySupport(
    OverlayCandidateList* surfaces) {
  ASSERT_EQ(2U, surfaces->size());

  OverlayCandidate& candidate = surfaces->back();
  EXPECT_EQ(kOverlayRect.ToString(), candidate.display_rect.ToString());
  EXPECT_EQ(BoundingRect(kUVTopLeft, kUVBottomRight).ToString(),
            candidate.uv_rect.ToString());
  candidate.overlay_handled = true;
}

class SingleOverlayProcessor : public OverlayProcessor {
 public:
  SingleOverlayProcessor(OutputSurface* surface,
                         ResourceProvider* resource_provider);
  // Virtual to allow testing different strategies.
  virtual void Initialize() OVERRIDE;
};

SingleOverlayProcessor::SingleOverlayProcessor(
    OutputSurface* surface,
    ResourceProvider* resource_provider)
    : OverlayProcessor(surface, resource_provider) {
  EXPECT_EQ(surface, surface_);
  EXPECT_EQ(resource_provider, resource_provider_);
}

void SingleOverlayProcessor::Initialize() {
  OverlayCandidateValidator* candidates =
      surface_->overlay_candidate_validator();
  ASSERT_TRUE(candidates != NULL);
  strategies_.push_back(scoped_ptr<Strategy>(
      new OverlayStrategySingleOnTop(candidates, resource_provider_)));
}

class DefaultOverlayProcessor : public OverlayProcessor {
 public:
  DefaultOverlayProcessor(OutputSurface* surface,
                          ResourceProvider* resource_provider);
  size_t GetStrategyCount();
};

DefaultOverlayProcessor::DefaultOverlayProcessor(
    OutputSurface* surface,
    ResourceProvider* resource_provider)
    : OverlayProcessor(surface, resource_provider) {}

size_t DefaultOverlayProcessor::GetStrategyCount() {
  return strategies_.size();
}

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(scoped_refptr<ContextProvider> context_provider)
      : OutputSurface(context_provider) {}

  void InitWithSingleOverlayValidator() {
    overlay_candidate_validator_.reset(new SingleOverlayValidator);
  }
};

scoped_ptr<RenderPass> CreateRenderPass() {
  RenderPass::Id id(1, 0);
  gfx::Rect output_rect(0, 0, 256, 256);
  bool has_transparent_background = true;

  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id,
               output_rect,
               output_rect,
               gfx::Transform(),
               has_transparent_background,
               RenderPass::NO_OVERLAY);

  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->opacity = 1.f;
  pass->shared_quad_state_list.push_back(shared_state.Pass());
  return pass.Pass();
}

scoped_ptr<TextureDrawQuad> CreateCandidateQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state) {
  unsigned sync_point = 0;
  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), GL_TEXTURE_2D, sync_point);
  mailbox.set_allow_overlay(true);
  scoped_ptr<SingleReleaseCallback> release_callback =
      SingleReleaseCallback::Create(base::Bind(&MailboxReleased));

  ResourceProvider::ResourceId resource_id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, release_callback.Pass());
  bool premultiplied_alpha = false;
  bool flipped = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  scoped_ptr<TextureDrawQuad> overlay_quad = TextureDrawQuad::Create();
  overlay_quad->SetNew(shared_quad_state,
                       kOverlayRect,
                       kOverlayRect,
                       kOverlayRect,
                       resource_id,
                       premultiplied_alpha,
                       kUVTopLeft,
                       kUVBottomRight,
                       SK_ColorTRANSPARENT,
                       vertex_opacity,
                       flipped);

  return overlay_quad.Pass();
}

scoped_ptr<DrawQuad> CreateCheckeredQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state) {
  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
      CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(
      shared_quad_state, kOverlayRect, kOverlayRect, SkColor());
  return checkerboard_quad.PassAs<DrawQuad>();
}

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i];
    RenderPass* actual = actual_list[i];

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_RECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_RECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);
    EXPECT_EQ(expected->overlay_state, actual->overlay_state);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (size_t i = 0; i < expected->quad_list.size(); ++i) {
      EXPECT_EQ(expected->quad_list[i]->rect.ToString(),
                actual->quad_list[i]->rect.ToString());
      EXPECT_EQ(
          expected->quad_list[i]->shared_quad_state->content_bounds.ToString(),
          actual->quad_list[i]->shared_quad_state->content_bounds.ToString());
    }
  }
}

TEST(OverlayTest, NoOverlaysByDefault) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  EXPECT_EQ(NULL, output_surface.overlay_candidate_validator());

  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.overlay_candidate_validator() != NULL);
}

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.overlay_candidate_validator() != NULL);

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(&output_surface, NULL, 0, false, 1));

  scoped_ptr<DefaultOverlayProcessor> overlay_processor(
      new DefaultOverlayProcessor(&output_surface, resource_provider.get()));
  overlay_processor->Initialize();
  EXPECT_GE(1U, overlay_processor->GetStrategyCount());
}

class SingleOverlayOnTopTest : public testing::Test {
 protected:
  virtual void SetUp() {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    EXPECT_TRUE(output_surface_->BindToClient(&client_));
    output_surface_->InitWithSingleOverlayValidator();
    EXPECT_TRUE(output_surface_->overlay_candidate_validator() != NULL);

    resource_provider_ =
        ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1);

    overlay_processor_.reset(new SingleOverlayProcessor(
        output_surface_.get(), resource_provider_.get()));
    overlay_processor_->Initialize();
  }

  scoped_refptr<TestContextProvider> provider_;
  scoped_ptr<OverlayOutputSurface> output_surface_;
  FakeOutputSurfaceClient client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SingleOverlayProcessor> overlay_processor_;
};

TEST_F(SingleOverlayOnTopTest, SuccessfullOverlay) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> original_quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());

  pass->quad_list.push_back(
      original_quad->Copy(pass->shared_quad_state_list.back()));
  // Add something behind it.
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Check for potential candidates.
  overlay_processor_->ProcessForOverlays(&pass_list);

  // This should have one more pass with an overlay.
  ASSERT_EQ(2U, pass_list.size());

  RenderPass* overlay_pass = pass_list.front();
  EXPECT_EQ(RenderPass::SIMPLE_OVERLAY, overlay_pass->overlay_state);
  RenderPass* main_pass = pass_list.back();
  EXPECT_EQ(RenderPass::NO_OVERLAY, main_pass->overlay_state);

  // Check that the quad is what we expect it to be.
  EXPECT_EQ(1U, overlay_pass->quad_list.size());
  const DrawQuad* overlay_quad = overlay_pass->quad_list.front();
  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT, overlay_quad->material);
  EXPECT_EQ(original_quad->resource_id,
            TextureDrawQuad::MaterialCast(overlay_quad)->resource_id);
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  overlay_processor_->ProcessForOverlays(&pass_list);
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  pass->quad_list.push_back(
      CreateCandidateQuad(resource_provider_.get(),
                          pass->shared_quad_state_list.back())
          .PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  overlay_processor_->ProcessForOverlays(&pass_list);
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  RenderPassList pass_list;
  pass_list.push_back(CreateRenderPass());

  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> original_quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());

  pass->quad_list.push_back(
      original_quad->Copy(pass->shared_quad_state_list.back()));
  // Add something behind it.
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));
  pass->quad_list.push_back(CreateCheckeredQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back()));

  pass_list.push_back(pass.Pass());

  // Check for potential candidates.
  overlay_processor_->ProcessForOverlays(&pass_list);

  // This should have one more pass with an overlay.
  ASSERT_EQ(3U, pass_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectPremultipliedAlpha) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->premultiplied_alpha = true;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

TEST_F(SingleOverlayOnTopTest, RejectBlending) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->needs_blending = true;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  quad->background_color = SK_ColorBLACK;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->blend_mode = SkXfermode::kScreen_Mode;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

TEST_F(SingleOverlayOnTopTest, RejectTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  scoped_ptr<TextureDrawQuad> quad = CreateCandidateQuad(
      resource_provider_.get(), pass->shared_quad_state_list.back());
  pass->shared_quad_state_list.back()->content_to_target_transform.Scale(2.f,
                                                                         2.f);

  pass->quad_list.push_back(quad.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  overlay_processor_->ProcessForOverlays(&pass_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(RenderPass::NO_OVERLAY, pass_list.back()->overlay_state);
}

}  // namespace
}  // namespace cc
