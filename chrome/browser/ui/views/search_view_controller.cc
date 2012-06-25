// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_view_controller.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/location_bar/location_bar_container.h"
#include "grit/theme_resources.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace {

class FixedSizedLayoutManager : public views::LayoutManager {
 public:
  explicit FixedSizedLayoutManager(const gfx::Size& size)
      : preferred_size_(size) {
  }

  // views::LayoutManager overrides:
  virtual void Layout(views::View* host) OVERRIDE {}
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE {
    return preferred_size_;
  }

 private:
  const gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedLayoutManager);
};

// Background for the NTP container.
class NTPContainerBackground : public views::Background {
 public:
  NTPContainerBackground() {}

  // views::Background overrides:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    canvas->DrawColor(chrome::search::kSearchBackgroundColor);
    canvas->FillRect(gfx::Rect(0, 0, view->width(),
                               chrome::search::kSearchResultsHeight),
                     chrome::search::kNTPBackgroundColor);
    canvas->FillRect(
        gfx::Rect(0, chrome::search::kSearchResultsHeight, view->width(), 1),
        chrome::search::kResultsSeparatorColor);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPContainerBackground);
};

// Background for the NTP view.
class NTPViewBackground : public views::Background {
 public:
  NTPViewBackground() {}

  // views::Background overrides:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    canvas->DrawColor(chrome::search::kNTPBackgroundColor);
    // Have to use the height of the layer here since the layer is animated
    // independant of the view.
    int height = view->layer()->bounds().height();
    if (height < chrome::search::kSearchResultsHeight)
      return;
    canvas->FillRect(gfx::Rect(0, height - 1, view->width(), 1),
                     chrome::search::kResultsSeparatorColor);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPViewBackground);
};

// LayoutManager for the NTPView.
class NTPViewLayoutManager : public views::LayoutManager {
 public:
  NTPViewLayoutManager(views::View* logo_view, views::View* content_view)
      : logo_view_(logo_view),
        content_view_(content_view) {
  }

  // views::LayoutManager overrides:
  virtual void Layout(views::View* host) OVERRIDE {
    gfx::Size logo_pref = logo_view_->GetPreferredSize();
    logo_view_->SetBounds(
        (host->width() - logo_pref.width()) / 2,
        chrome::search::kOmniboxYPosition - 20 - logo_pref.height(),
        logo_pref.width(),
        logo_pref.height());

    gfx::Size content_pref(content_view_->GetPreferredSize());
    int content_y = std::max(chrome::search::kOmniboxYPosition + 50,
                             host->height() - content_pref.height() - 50);
    content_view_->SetBounds((host->width() - content_pref.width()) / 2,
                             content_y, content_pref.width(),
                             content_pref.height());
  }

  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE {
    // Preferred size doesn't matter for the NTPView.
    return gfx::Size();
  }

 private:
  views::View* logo_view_;
  views::View* content_view_;

  DISALLOW_COPY_AND_ASSIGN(NTPViewLayoutManager);
};

// Stacks view's layer above all its sibling layers.
void StackViewsLayerAtTop(views::View* view) {
  DCHECK(view->layer());
  DCHECK(view->layer()->parent());
  view->layer()->parent()->StackAtTop(view->layer());
}

}  // namespace

SearchViewController::SearchViewController(
    ContentsContainer* contents_container,
    LocationBarContainer* location_bar_container)
    : contents_container_(contents_container),
      location_bar_container_(location_bar_container),
      animating_(false),
      tab_(NULL),
      ntp_container_(NULL),
      ntp_view_(NULL),
      logo_view_(NULL),
      content_view_(NULL) {
}

SearchViewController::~SearchViewController() {
  if (search_model())
    search_model()->RemoveObserver(this);
}

void SearchViewController::SetTabContents(TabContents* tab) {
  if (tab_ == tab)
    return;

  if (search_model())
    search_model()->RemoveObserver(this);
  tab_ = tab;
  if (search_model())
    search_model()->AddObserver(this);

  if (animating_)
    DestroyViews();

  Update();
}

void SearchViewController::StackAtTop() {
#if defined(USE_AURA)
  if (ntp_container_) {
    StackViewsLayerAtTop(ntp_container_);
    StackViewsLayerAtTop(ntp_view_);
    StackViewsLayerAtTop(logo_view_);
    StackViewsLayerAtTop(content_view_);
  }
#else
  NOTIMPLEMENTED();
#endif
  location_bar_container_->StackAtTop();
}

void SearchViewController::InstantReady() {
  if (animating_)
    return;
  DestroyViews();
}

void SearchViewController::ModeChanged(const chrome::search::Mode& mode) {
  Update();
}

void SearchViewController::OnImplicitAnimationsCompleted() {
  animating_ = false;
  // Wait to destroy the views until the preview is ready (otherwise the user is
  // left with seeing an old page). InstantReady() is invoked when the preview
  // is ready.
  if (contents_container_->preview_web_contents())
    DestroyViews();
}

void SearchViewController::Update() {
  if (!search_model()) {
    DestroyViews();
    return;
  }
  switch (search_model()->mode().mode) {
    case chrome::search::Mode::MODE_DEFAULT:
      DestroyViews();
      break;

    case chrome::search::Mode::MODE_NTP:
      if (animating_)
        DestroyViews();
      if (!ntp_view_)
        CreateViews();
      break;

    case chrome::search::Mode::MODE_SEARCH:
      if (search_model()->mode().animate && ntp_view_ && !animating_)
        StartAnimation();
      else if (!search_model()->mode().animate)
        DestroyViews();
      break;
  }
  StackAtTop();
}

void SearchViewController::StartAnimation() {
  // TODO(sky): remove this factor after we're happy its all working smoothly.
  int factor = 1;
  DCHECK(!animating_);
  animating_ = true;
  {
    ui::Layer* ntp_layer = ntp_view_->layer();
    ui::ScopedLayerAnimationSettings settings(ntp_layer->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(180 * factor));
    settings.SetTweenType(ui::Tween::EASE_IN_OUT);
    gfx::Rect bounds(ntp_layer->bounds());
    bounds.set_height(1);
    ntp_layer->SetBounds(bounds);
  }

  {
    ui::Layer* logo_layer = logo_view_->layer();
    ui::ScopedLayerAnimationSettings settings(logo_layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(135 * factor));
    settings.SetTweenType(ui::Tween::EASE_IN_OUT);
    gfx::Rect bounds(logo_layer->bounds());
    bounds.set_y(bounds.y() - 100);
    logo_layer->SetBounds(bounds);
    logo_layer->SetOpacity(0.0f);
  }

  {
    ui::Layer* content_layer = content_view_->layer();
    ui::ScopedLayerAnimationSettings settings(content_layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(180 * factor));
    settings.SetTweenType(ui::Tween::LINEAR);
    gfx::Rect bounds(content_layer->bounds());
    bounds.set_y(bounds.y() - 250);
    content_layer->SetBounds(bounds);
    content_layer->SetOpacity(0.0f);
  }
}

void SearchViewController::CreateViews() {
  ntp_container_ = new views::View;
  ntp_container_->set_background(new NTPContainerBackground);
  ntp_container_->SetPaintToLayer(true);
  ntp_container_->SetLayoutManager(new views::FillLayout);
  ntp_container_->layer()->SetMasksToBounds(true);

  ntp_view_ = new views::View;
  ntp_view_->set_background(new NTPViewBackground);
  ntp_view_->SetPaintToLayer(true);
  ntp_view_->layer()->SetMasksToBounds(true);

  logo_view_ = new views::View;
  logo_view_->SetLayoutManager(
      new FixedSizedLayoutManager(gfx::Size(300, 200)));
  logo_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorRED));
  logo_view_->SetPaintToLayer(true);
  logo_view_->SetFillsBoundsOpaquely(false);

  // TODO: replace with WebContents for NTP.
  content_view_ = new views::View;
  content_view_->SetLayoutManager(
      new FixedSizedLayoutManager(gfx::Size(400, 200)));
  content_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLUE));
  content_view_->SetPaintToLayer(true);
  content_view_->SetFillsBoundsOpaquely(false);

  ntp_view_->SetLayoutManager(
      new NTPViewLayoutManager(logo_view_, content_view_));

  ntp_container_->AddChildView(ntp_view_);
  ntp_view_->AddChildView(logo_view_);
  ntp_view_->AddChildView(content_view_);

  contents_container_->SetOverlay(ntp_container_);
}

void SearchViewController::DestroyViews() {
  if (!ntp_container_)
    return;

  contents_container_->SetOverlay(NULL);
  delete ntp_container_;
  ntp_container_ = ntp_view_ = NULL;
  content_view_ = logo_view_ = NULL;
  animating_ = false;
}

chrome::search::SearchModel* SearchViewController::search_model() {
  return tab_ ? tab_->search_tab_helper()->model() : NULL;
}
