// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/non_client_frame_controller.h"

#include "ash/ash_layout_constants.h"
#include "ash/mus/top_level_window_factory.h"
#include "ash/mus/window_manager_application.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "cc/base/math_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/layer_tree_settings.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/fake_context_factory.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace mus {

namespace {

gfx::Rect GetQuadBoundsInScreen(const viz::DrawQuad* quad) {
  return cc::MathUtil::MapEnclosingClippedRect(
      quad->shared_quad_state->quad_to_target_transform, quad->visible_rect);
}

bool FindAnyQuad(const cc::CompositorFrame& frame,
                 const gfx::Rect& screen_rect) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const cc::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

bool FindColorQuad(const cc::CompositorFrame& frame,
                   const gfx::Rect& screen_rect,
                   SkColor color) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const cc::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (quad->material != viz::DrawQuad::Material::SOLID_COLOR)
      continue;

    auto* color_quad = cc::SolidColorDrawQuad::MaterialCast(quad);
    if (color_quad->color != color)
      continue;
    if (GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

bool FindTiledContentQuad(const cc::CompositorFrame& frame,
                          const gfx::Rect& screen_rect) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const cc::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (quad->material == viz::DrawQuad::Material::TILED_CONTENT &&
        GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

}  // namespace

class NonClientFrameControllerTest : public AshTestBase {
 public:
  NonClientFrameControllerTest() = default;
  ~NonClientFrameControllerTest() override = default;

  const cc::CompositorFrame& GetLastCompositorFrame() const {
    return context_factory_.GetLastCompositorFrame();
  }

  // AshTestBase:
  void SetUp() override {
    aura::Env* env = aura::Env::GetInstance();
    DCHECK(env);
    context_factory_to_restore_ = env->context_factory();
    env->set_context_factory(&context_factory_);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    aura::Env::GetInstance()->set_context_factory(context_factory_to_restore_);
  }

 private:
  ui::FakeContextFactory context_factory_;
  ui::ContextFactory* context_factory_to_restore_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameControllerTest);
};

TEST_F(NonClientFrameControllerTest, ContentRegionNotDrawnForClient) {
  std::map<std::string, std::vector<uint8_t>> properties;
  std::unique_ptr<aura::Window> window(mus::CreateAndParentTopLevelWindow(
      ash_test_helper()->window_manager_app()->window_manager(),
      ui::mojom::WindowType::WINDOW, &properties));
  ASSERT_TRUE(window);

  NonClientFrameController* controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(controller);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  ASSERT_TRUE(widget);

  const int caption_height =
      GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).height();
  const gfx::Size tile_size = cc::LayerTreeSettings().default_tile_size;
  const int tile_width = tile_size.width();
  const int tile_height = tile_size.height();
  const int tile_x = tile_width;
  const int tile_y = tile_height;

  const gfx::Rect kTileBounds(gfx::Point(tile_x, tile_y), tile_size);
  ui::Compositor* compositor = widget->GetCompositor();

  // Give the ui::Compositor a LocalSurfaceId so that it does not defer commit
  // when a draw is scheduled.
  viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
  compositor->SetLocalSurfaceId(local_surface_id);

  // Without the window visible, there should be a tile for the wallpaper at
  // (tile_x, tile_y) of size |tile_size|.
  compositor->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
  {
    const cc::CompositorFrame& frame = GetLastCompositorFrame();
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_TRUE(FindColorQuad(frame, kTileBounds, SK_ColorBLACK));
  }

  // Show |widget|, and position it so that it covers that wallpaper tile.
  const gfx::Rect widget_bound(tile_x - 10, tile_y - 10, tile_width + 20,
                               tile_height + 20);
  widget->SetBounds(widget_bound);
  widget->Show();
  compositor->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
  {
    // This time, that tile for the wallpaper will not be drawn.
    const cc::CompositorFrame& frame = GetLastCompositorFrame();
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_FALSE(FindColorQuad(frame, kTileBounds, SK_ColorBLACK));

    // Any solid-color quads for the widget are transparent and will be
    // optimized away.
    const gfx::Rect top_left(widget_bound.origin(), tile_size);
    const gfx::Rect top_right(
        top_left.top_right(),
        gfx::Size(widget_bound.width() - top_left.width(), top_left.height()));
    const gfx::Rect bottom_left(
        top_left.bottom_left(),
        gfx::Size(top_left.width(), widget_bound.height() - top_left.height()));
    const gfx::Rect bottom_right(
        top_left.bottom_right(),
        gfx::Size(top_right.width(), bottom_left.height()));
    EXPECT_FALSE(FindAnyQuad(frame, top_left));
    EXPECT_FALSE(FindAnyQuad(frame, top_right));
    EXPECT_FALSE(FindAnyQuad(frame, bottom_left));
    EXPECT_FALSE(FindAnyQuad(frame, bottom_right));

    // And there will be a content quad for the window caption.
    gfx::Rect caption_bound(widget_bound);
    caption_bound.set_height(caption_height);
    EXPECT_TRUE(FindTiledContentQuad(frame, caption_bound));
  }
}

}  // namespace mus
}  // namespace ash
