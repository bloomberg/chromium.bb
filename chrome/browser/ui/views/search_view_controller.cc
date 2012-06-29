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
#include "chrome/browser/ui/webui/instant_ui.h"
#include "grit/theme_resources.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace {

// SearchContainerView ---------------------------------------------------------

// SearchContainerView contains the |ntp_view_| and
// |omnibox_popup_view_parent_|. |ntp_view_| is given the full size and
// the |omnibox_popup_view_parent_| is gives its preferred height.
class SearchContainerView : public views::View {
 public:
  SearchContainerView(views::View* ntp_view,
                      views::View* omnibox_popup_view_parent)
      : ntp_view_(ntp_view),
        omnibox_popup_view_parent_(omnibox_popup_view_parent) {
    AddChildView(ntp_view);
    AddChildView(omnibox_popup_view_parent);
  }

  virtual void Layout() OVERRIDE {
    ntp_view_->SetBounds(0, 0, width(), height());

    gfx::Size omnibox_popup_view_parent_pref(
        omnibox_popup_view_parent_->GetPreferredSize());
    int old_omnibox_popup_view_parent_height(
        omnibox_popup_view_parent_->height());
    omnibox_popup_view_parent_->SetBounds(
        0, 0, width(), omnibox_popup_view_parent_pref.height());

    // Schedule paints the line below the popup (painted in
    // OnPaintBackground()).
    int border_y =
        std::max(old_omnibox_popup_view_parent_height,
                 omnibox_popup_view_parent_pref.height()) + 1;
    SchedulePaintInRect(gfx::Rect(0, 0, width(), border_y));
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(chrome::search::kSearchBackgroundColor);
    gfx::Size omnibox_pref(omnibox_popup_view_parent_->GetPreferredSize());
    // The color for this rect must be the same as that used as background for
    // InlineOmniboxPopupView.
    canvas->FillRect(gfx::Rect(0, 0, width(), omnibox_pref.height()),
                     chrome::search::kSuggestBackgroundColor);
    canvas->FillRect(
        gfx::Rect(0, omnibox_pref.height(), width(), 1),
        chrome::search::kResultsSeparatorColor);
  }

 private:
  views::View* ntp_view_;
  views::View* omnibox_popup_view_parent_;

  DISALLOW_COPY_AND_ASSIGN(SearchContainerView);
};

// FixedSizeLayoutManager ------------------------------------------------------

// LayoutManager that returns a specific preferred size.

class FixedSizeLayoutManager : public views::LayoutManager {
 public:
  explicit FixedSizeLayoutManager(const gfx::Size& size)
      : preferred_size_(size) {
  }
  virtual ~FixedSizeLayoutManager() {}

  // views::LayoutManager overrides:
  virtual void Layout(views::View* host) OVERRIDE {}
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE {
    return preferred_size_;
  }

 private:
  const gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizeLayoutManager);
};

// NTPViewBackground -----------------------------------------------------------

// Background for the NTP view.
class NTPViewBackground : public views::Background {
 public:
  NTPViewBackground() {}
  virtual ~NTPViewBackground() {}

  // views::Background overrides:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    canvas->DrawColor(chrome::search::kNTPBackgroundColor);
    // Have to use the height of the layer here since the layer is animated
    // independent of the view.
    int height = view->layer()->bounds().height();
    if (height < chrome::search::kSearchResultsHeight)
      return;
    canvas->FillRect(gfx::Rect(0, height - 1, view->width(), 1),
                     chrome::search::kResultsSeparatorColor);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPViewBackground);
};

// NTPViewLayoutManager --------------------------------------------------------

// LayoutManager for the NTPView.
class NTPViewLayoutManager : public views::LayoutManager {
 public:
  NTPViewLayoutManager(views::View* logo_view, views::View* content_view)
      : logo_view_(logo_view),
        content_view_(content_view) {
  }
  virtual ~NTPViewLayoutManager() {}

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

// SearchViewController::OmniboxPopupViewParent --------------------------------

// View the omnibox is added to. Listens for changes to the visibility of the
// omnibox and updates the SearchViewController appropriately.
class SearchViewController::OmniboxPopupViewParent : public views::View {
 public:
  explicit OmniboxPopupViewParent(SearchViewController* search_view_controller);
  virtual ~OmniboxPopupViewParent();

  bool is_child_visible() { return child_count() && child_at(0)->visible(); }

 protected:
  // views::View overrides:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

 private:
  SearchViewController* search_view_controller_;

  bool child_visible_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupViewParent);
};

SearchViewController::OmniboxPopupViewParent::OmniboxPopupViewParent(
    SearchViewController* search_view_controller)
    : search_view_controller_(search_view_controller),
      child_visible_(false) {
  SetLayoutManager(new views::FillLayout);
}

SearchViewController::OmniboxPopupViewParent::~OmniboxPopupViewParent() {
}

void SearchViewController::OmniboxPopupViewParent::ChildPreferredSizeChanged(
    views::View* child) {
  if (parent() && (child->visible() || child_visible_))
    parent()->Layout();
  if (child_visible_ != child->visible()) {
    child_visible_ = child->visible();
    search_view_controller_->PopupVisibilityChanged();
  }
}

// SearchViewController --------------------------------------------------------

SearchViewController::SearchViewController(
    ContentsContainer* contents_container)
    : contents_container_(contents_container),
      location_bar_container_(NULL),
      state_(STATE_NOT_VISIBLE),
      tab_(NULL),
      search_container_(NULL),
      ntp_view_(NULL),
      logo_view_(NULL),
      content_view_(NULL),
      omnibox_popup_view_parent_(NULL) {
  omnibox_popup_view_parent_ = new OmniboxPopupViewParent(this);
}

SearchViewController::~SearchViewController() {
  if (search_model())
    search_model()->RemoveObserver(this);

  // If the |omnibox_popup_view_| isn't parented, delete it. Otherwise it'll be
  // deleted by its parent.
  if (!omnibox_popup_view_parent_->parent())
    delete omnibox_popup_view_parent_;
}

views::View* SearchViewController::omnibox_popup_view_parent() {
  return omnibox_popup_view_parent_;
}

void SearchViewController::SetTabContents(TabContents* tab) {
  if (tab_ == tab)
    return;

  if (search_model())
    search_model()->RemoveObserver(this);
  tab_ = tab;
  if (search_model())
    search_model()->AddObserver(this);

  UpdateState();
}

void SearchViewController::StackAtTop() {
#if defined(USE_AURA)
  if (search_container_) {
    StackViewsLayerAtTop(search_container_);
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
}

void SearchViewController::ModeChanged(const chrome::search::Mode& mode) {
  UpdateState();
}

void SearchViewController::OnImplicitAnimationsCompleted() {
  DCHECK_EQ(STATE_ANIMATING, state_);
  state_ = STATE_SEARCH;
  ntp_view_->SetVisible(false);
  // While |ntp_view_| was fading out, location bar was animating from the
  // middle of the NTP page to the top toolbar, at the same rate.
  // Suggestions need to be aligned with the final location of the location bar.
  // So if omnibox popup view (InlineOmniboxPopupView) is visible, force a
  // re-layout of its children (i.e. the suggestions) to align with the location
  // bar's final bounds.
  if (omnibox_popup_view_parent_->is_child_visible())
    omnibox_popup_view_parent_->child_at(0)->Layout();
}

void SearchViewController::UpdateState() {
  if (!search_model()) {
    DestroyViews();
    return;
  }
  State new_state = STATE_NOT_VISIBLE;
  switch (search_model()->mode().mode) {
    case chrome::search::Mode::MODE_DEFAULT:
      break;

    case chrome::search::Mode::MODE_NTP:
      new_state = STATE_NTP;
      break;

    case chrome::search::Mode::MODE_SEARCH:
      if (search_model()->mode().animate && state_ == STATE_NTP) {
        new_state = STATE_ANIMATING;
      } else {
        // Only enter into MODE_SEARCH if the omnibox is visible.
        if (omnibox_popup_view_parent_->is_child_visible())
          new_state = STATE_SEARCH;
        else
          new_state = STATE_NOT_VISIBLE;
      }
      break;
  }
  SetState(new_state);
}

void SearchViewController::SetState(State state) {
  if (state_ == state)
    return;

  State old_state = state_;
  state_ = state;
  switch (state_) {
    case STATE_NOT_VISIBLE:
      DestroyViews();
      break;

    case STATE_NTP:
      DestroyViews();
      CreateViews();
      break;

    case STATE_ANIMATING:
      // Should only animate from the ntp.
      DCHECK_EQ(STATE_NTP, old_state);
      StartAnimation();
      break;

    case STATE_SEARCH:
      DestroyViews();
      CreateViews();
      ntp_view_->SetVisible(false);
      break;
  }
}

void SearchViewController::StartAnimation() {
  int factor = InstantUI::GetSlowAnimationScaleFactor();
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
  DCHECK(!ntp_view_);

  ntp_view_ = new views::View;
  ntp_view_->set_background(new NTPViewBackground);
  ntp_view_->SetPaintToLayer(true);
  ntp_view_->layer()->SetMasksToBounds(true);

  logo_view_ = new views::View;
  logo_view_->SetLayoutManager(
      new FixedSizeLayoutManager(gfx::Size(300, 200)));
  logo_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorRED));
  logo_view_->SetPaintToLayer(true);
  logo_view_->SetFillsBoundsOpaquely(false);

  // TODO: replace with WebContents for NTP.
  content_view_ = new views::View;
  content_view_->SetLayoutManager(
      new FixedSizeLayoutManager(gfx::Size(400, 200)));
  content_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLUE));
  content_view_->SetPaintToLayer(true);
  content_view_->SetFillsBoundsOpaquely(false);

  ntp_view_->SetLayoutManager(
      new NTPViewLayoutManager(logo_view_, content_view_));

  search_container_ =
      new SearchContainerView(ntp_view_, omnibox_popup_view_parent_);
  search_container_->SetPaintToLayer(true);
  search_container_->SetLayoutManager(new views::FillLayout);
  search_container_->layer()->SetMasksToBounds(true);

  ntp_view_->AddChildView(logo_view_);
  ntp_view_->AddChildView(content_view_);

  contents_container_->SetOverlay(search_container_);
}

void SearchViewController::DestroyViews() {
  if (!search_container_)
    return;

  // We persist the parent of the omnibox so that we don't have to inject a new
  // parent into ToolbarView.
  omnibox_popup_view_parent_->parent()->RemoveChildView(
      omnibox_popup_view_parent_);

  contents_container_->SetOverlay(NULL);
  delete search_container_;
  search_container_ = ntp_view_ = NULL;
  content_view_ = logo_view_ = NULL;

  state_ = STATE_NOT_VISIBLE;
}

void SearchViewController::PopupVisibilityChanged() {
  // Don't do anything while animating if the child is visible. Otherwise we'll
  // prematurely cancel the animation.
  if (state_ != STATE_ANIMATING ||
      !omnibox_popup_view_parent_->is_child_visible()) {
    UpdateState();
  }
}

chrome::search::SearchModel* SearchViewController::search_model() {
  return tab_ ? tab_->search_tab_helper()->model() : NULL;
}
