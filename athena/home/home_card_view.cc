// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_view.h"

#include "athena/home/home_card_constants.h"
#include "athena/util/athena_constants.h"
#include "athena/wm/public/window_manager.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace athena {

namespace {

const float kMinimizedHomeOpacity = 0.65f;
const int kIndicatorOffset = 24;
const int kAppListOffset = -128;
}

HomeCardView::HomeCardView(app_list::AppListViewDelegate* view_delegate,
                           HomeCardGestureManager::Delegate* gesture_delegate)
    : background_(new views::View),
      main_view_(new app_list::AppListMainView(view_delegate)),
      search_box_view_(new app_list::SearchBoxView(main_view_, view_delegate)),
      minimized_background_(new views::View()),
      drag_indicator_(new views::View()),
      gesture_delegate_(gesture_delegate),
      weak_factory_(this) {
  background_->set_background(
      views::Background::CreateVerticalGradientBackground(SK_ColorLTGRAY,
                                                          SK_ColorWHITE));
  background_->SetPaintToLayer(true);
  background_->SetFillsBoundsOpaquely(false);
  AddChildView(background_);

  main_view_->SetPaintToLayer(true);
  main_view_->SetFillsBoundsOpaquely(false);
  main_view_->layer()->SetMasksToBounds(true);
  AddChildView(main_view_);

  search_box_view_->SetPaintToLayer(true);
  search_box_view_->SetFillsBoundsOpaquely(false);
  search_box_view_->layer()->SetMasksToBounds(true);
  AddChildView(search_box_view_);

  minimized_background_->set_background(
      views::Background::CreateSolidBackground(
          SkColorSetA(SK_ColorBLACK, 256 * kMinimizedHomeOpacity)));
  minimized_background_->SetPaintToLayer(true);
  minimized_background_->SetFillsBoundsOpaquely(false);
  minimized_background_->layer()->set_name("MinimizedBackground");
  AddChildView(minimized_background_);

  drag_indicator_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  drag_indicator_->SetPaintToLayer(true);
  AddChildView(drag_indicator_);
}

HomeCardView::~HomeCardView() {
}

void HomeCardView::Init() {
  main_view_->Init(GetWidget()->GetNativeView(),
                   -1, /* inital apps page: -1 means default */
                   search_box_view_);
}

void HomeCardView::SetStateProgress(HomeCard::State from_state,
                                    HomeCard::State to_state,
                                    float progress) {
  // TODO(mukai): not clear the focus, but simply close the virtual keyboard.
  GetFocusManager()->ClearFocus();

  gfx::Rect from_main_bounds = GetMainViewBounds(from_state);
  gfx::Rect to_main_bounds = GetMainViewBounds(to_state);
  if (from_main_bounds != to_main_bounds) {
    DCHECK_EQ(from_main_bounds.size().ToString(),
              to_main_bounds.size().ToString());
    gfx::Rect main_bounds = gfx::Tween::RectValueBetween(
        progress, from_main_bounds, to_main_bounds);
    main_view_->SetBoundsRect(main_bounds);
    main_bounds.set_height(
        search_box_view_->GetHeightForWidth(main_bounds.width()));
    search_box_view_->SetBoundsRect(main_bounds);
  }

  float background_opacity = 1.0f;
  if (from_state == HomeCard::VISIBLE_MINIMIZED ||
      to_state == HomeCard::VISIBLE_MINIMIZED) {
    background_opacity = (from_state == HomeCard::VISIBLE_MINIMIZED)
                             ? progress
                             : (1.0f - progress);
  }
  background_->layer()->SetOpacity(background_opacity);
  minimized_background_->layer()->SetOpacity(1.0f - background_opacity);
  UpdateMinimizedBackgroundVisibility();

  int background_height = kHomeCardHeight;
  if (from_state == HomeCard::VISIBLE_CENTERED ||
      to_state == HomeCard::VISIBLE_CENTERED) {
    gfx::Rect window_bounds = GetWidget()->GetWindowBoundsInScreen();
    background_height = window_bounds.height() - window_bounds.y();
  }
  gfx::Transform background_transform;
  background_transform.Scale(SK_MScalar1, SkIntToMScalar(background_height) /
                                              SkIntToMScalar(height()));
  background_->layer()->SetTransform(background_transform);

  gfx::Rect from_bounds = GetDragIndicatorBounds(from_state);
  gfx::Rect to_bounds = GetDragIndicatorBounds(to_state);
  if (from_bounds != to_bounds) {
    DCHECK_EQ(from_bounds.size().ToString(), to_bounds.size().ToString());
    drag_indicator_->SetBoundsRect(
        gfx::Tween::RectValueBetween(progress, from_bounds, to_bounds));
  }
}

void HomeCardView::SetStateWithAnimation(
    HomeCard::State state,
    gfx::Tween::Type tween_type,
    const base::Closure& on_animation_ended) {
  float minimized_opacity =
      (state == HomeCard::VISIBLE_MINIMIZED) ? 1.0f : 0.0f;
  // |minimized_background_| needs to be visible before scheduling animation.
  if (state == HomeCard::VISIBLE_MINIMIZED)
    minimized_background_->SetVisible(true);

  if (minimized_opacity != minimized_background_->layer()->GetTargetOpacity()) {
    ui::ScopedLayerAnimationSettings settings(
        minimized_background_->layer()->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN);
    settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&HomeCardView::UpdateMinimizedBackgroundVisibility,
                   weak_factory_.GetWeakPtr())));
    minimized_background_->layer()->SetOpacity(minimized_opacity);
  }

  gfx::Transform background_transform;
  if (state != HomeCard::VISIBLE_CENTERED) {
    background_transform.Scale(SK_MScalar1, SkIntToMScalar(kHomeCardHeight) /
                                                SkIntToMScalar(height()));
  }
  float background_opacity = 1.0f - minimized_opacity;
  if (background_->layer()->GetTargetTransform() != background_transform ||
      background_->layer()->GetTargetOpacity() != background_opacity) {
    ui::ScopedLayerAnimationSettings settings(
        background_->layer()->GetAnimator());
    settings.SetTweenType(tween_type);
    background_->layer()->SetTransform(background_transform);
    background_->layer()->SetOpacity(background_opacity);
  }

  {
    ui::ScopedLayerAnimationSettings settings(
        drag_indicator_->layer()->GetAnimator());
    settings.SetTweenType(tween_type);
    drag_indicator_->SetBoundsRect(GetDragIndicatorBounds(state));
  }

  {
    ui::ScopedLayerAnimationSettings settings(
        main_view_->layer()->GetAnimator());
    settings.SetTweenType(tween_type);
    settings.AddObserver(new ui::ClosureAnimationObserver(on_animation_ended));
    gfx::Rect main_bounds = GetMainViewBounds(state);
    main_view_->SetBoundsRect(main_bounds);
    main_bounds.set_height(
        search_box_view_->GetHeightForWidth(main_bounds.width()));
    search_box_view_->SetBoundsRect(main_bounds);
  }

  if (state == HomeCard::VISIBLE_BOTTOM) {
    app_list::ContentsView* contents_view = main_view_->contents_view();
    contents_view->SetActivePage(contents_view->GetPageIndexForState(
        app_list::AppListModel::STATE_START));
  }
}

void HomeCardView::ClearGesture() {
  gesture_manager_.reset();
}

void HomeCardView::OnGestureEvent(ui::GestureEvent* event) {
  if (!gesture_manager_ && event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    gesture_manager_.reset(new HomeCardGestureManager(
        gesture_delegate_,
        GetWidget()->GetNativeWindow()->GetRootWindow()->bounds()));
  }

  if (gesture_manager_)
    gesture_manager_->ProcessGestureEvent(event);
}

bool HomeCardView::OnMousePressed(const ui::MouseEvent& event) {
  if (HomeCard::Get()->GetState() == HomeCard::VISIBLE_MINIMIZED &&
      event.IsLeftMouseButton() && event.GetClickCount() == 1) {
    athena::WindowManager::Get()->EnterOverview();
    return true;
  }
  return false;
}

void HomeCardView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();
  background_->SetBoundsRect(contents_bounds);
  minimized_background_->SetBoundsRect(contents_bounds);
  const gfx::Rect drag_indicator_bounds =
      GetDragIndicatorBounds(HomeCard::Get()->GetState());
  drag_indicator_->SetBoundsRect(drag_indicator_bounds);

  gfx::Rect main_bounds(GetMainViewBounds(HomeCard::Get()->GetState()));
  main_view_->SetBoundsRect(main_bounds);

  main_bounds.set_height(
      search_box_view_->GetHeightForWidth(main_bounds.width()));
  search_box_view_->SetBoundsRect(main_bounds);
}

gfx::Rect HomeCardView::GetDragIndicatorBounds(HomeCard::State state) {
  gfx::Rect drag_indicator_bounds(
      GetContentsBounds().CenterPoint().x() - kHomeCardDragIndicatorWidth / 2,
      kHomeCardDragIndicatorMarginHeight, kHomeCardDragIndicatorWidth,
      kHomeCardDragIndicatorHeight);
  if (state == HomeCard::VISIBLE_CENTERED)
    drag_indicator_bounds.Offset(0, kSystemUIHeight);
  return drag_indicator_bounds;
}

gfx::Rect HomeCardView::GetMainViewBounds(HomeCard::State state) {
  const gfx::Rect contents_bounds = GetContentsBounds();
  const int main_width = main_view_->GetPreferredSize().width();
  gfx::Rect main_bounds(
      contents_bounds.CenterPoint().x() - main_width / 2,
      GetDragIndicatorBounds(state).bottom() + kIndicatorOffset, main_width,
      contents_bounds.height());
  // This is a bit hacky but slightly shifting up the main_view to fit
  // the search box and app icons in the home card.
  if (state != HomeCard::VISIBLE_CENTERED)
    main_bounds.set_y(kAppListOffset);
  return main_bounds;
}

void HomeCardView::UpdateMinimizedBackgroundVisibility() {
  minimized_background_->SetVisible(
      minimized_background_->layer()->GetTargetOpacity() != 0.0f);
}

views::View* HomeCardView::GetContentsView() {
  return this;
}

}  // namespace athena
