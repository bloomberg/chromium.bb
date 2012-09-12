// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_view_controller.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/search/toolbar_search_animator.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/location_bar/location_bar_container.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// Stacks view's layer above all its sibling layers.
void StackViewsLayerAtTop(views::View* view) {
  DCHECK(view->layer());
  DCHECK(view->layer()->parent());
  view->layer()->parent()->StackAtTop(view->layer());
}

// Stacks native view's layer above all its sibling layers.
void StackWebViewLayerAtTop(views::WebView* view) {
  if (!view->web_contents())
    return;

  ui::Layer* native_view_layer = view->web_contents()->GetNativeView()->layer();
  native_view_layer->parent()->StackAtTop(native_view_layer);
}

// SearchContainerView ---------------------------------------------------------

// SearchContainerView contains the |ntp_container_| and
// |omnibox_popup_parent_|. |ntp_container_| is given the full size and the
// |omnibox_popup_parent_| is given its preferred height.
class SearchContainerView : public views::View {
 public:
  SearchContainerView(views::View* ntp_container,
                      views::View* omnibox_popup_parent)
      : ntp_container_(ntp_container),
        omnibox_popup_parent_(omnibox_popup_parent) {
    AddChildView(ntp_container);
    AddChildView(omnibox_popup_parent);
  }

  virtual void Layout() OVERRIDE {
    ntp_container_->SetBounds(0, 0, width(), height());

    gfx::Size preferred_size(omnibox_popup_parent_->GetPreferredSize());
    int old_height = omnibox_popup_parent_->height();
    omnibox_popup_parent_->SetBounds(0, 0, width(), preferred_size.height());

    // Schedule paints the line below the popup (painted in
    // OnPaintBackground()).
    int border_y = std::max(old_height, preferred_size.height()) + 1;
    SchedulePaintInRect(gfx::Rect(0, 0, width(), border_y));
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(chrome::search::kSearchBackgroundColor);
    gfx::Size preferred_size = omnibox_popup_parent_->GetPreferredSize();
    // The color for this rect must be the same as that used as background for
    // InlineOmniboxPopupView.
    canvas->FillRect(gfx::Rect(0, 0, width(), preferred_size.height()),
                     chrome::search::kSuggestBackgroundColor);
    canvas->FillRect(
        gfx::Rect(0, preferred_size.height(), width(), 1),
        chrome::search::kResultsSeparatorColor);
  }

 private:
  views::View* ntp_container_;
  views::View* omnibox_popup_parent_;

  DISALLOW_COPY_AND_ASSIGN(SearchContainerView);
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
  NTPViewLayoutManager(views::View* logo_view, views::WebView* content_view)
      : logo_view_(logo_view),
        content_view_(content_view) {
  }
  virtual ~NTPViewLayoutManager() {}

  // views::LayoutManager overrides:
  virtual void Layout(views::View* host) OVERRIDE {
    gfx::Size preferred_size = logo_view_->GetPreferredSize();
    logo_view_->SetBounds(
        (host->width() - preferred_size.width()) / 2,
        chrome::search::kLogoYPosition,
        preferred_size.width(),
        preferred_size.height());

    // Note: Next would be the Omnibox layout.  That is done in
    // |ToolbarView::LayoutForSearch| however.

    const int kContentTop = logo_view_->bounds().bottom() +
        chrome::search::kLogoBottomGap +
        chrome::search::kNTPOmniboxHeight +
        chrome::search::kOmniboxBottomGap;
    content_view_->SetBounds(0,
                             kContentTop,
                             host->width(),
                             host->height() - kContentTop);

    // This is a hack to patch up ordering of native layer.  Changes to the view
    // hierarchy can |ReorderLayers| which messes with layer stacking.  Layout
    // typically follows reorderings, so we patch things up here.
    StackWebViewLayerAtTop(content_view_);
  }

  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE {
    // Preferred size doesn't matter for the NTPView.
    return gfx::Size();
  }

 private:
  views::View* logo_view_;
  views::WebView* content_view_;

  DISALLOW_COPY_AND_ASSIGN(NTPViewLayoutManager);
};

}  // namespace

namespace internal {

// OmniboxPopupContainer -------------------------------------------------------

// View the omnibox is added to. Listens for changes to the visibility of the
// omnibox and updates the SearchViewController appropriately.
class OmniboxPopupContainer : public views::View {
 public:
  explicit OmniboxPopupContainer(SearchViewController* search_view_controller);
  virtual ~OmniboxPopupContainer();

  bool is_child_visible() { return child_count() && child_at(0)->visible(); }

 protected:
  // views::View overrides:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

 private:
  SearchViewController* search_view_controller_;

  bool child_visible_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContainer);
};

OmniboxPopupContainer::OmniboxPopupContainer(
    SearchViewController* search_view_controller)
    : search_view_controller_(search_view_controller),
      child_visible_(false) {
  SetLayoutManager(new views::FillLayout);
}

OmniboxPopupContainer::~OmniboxPopupContainer() {
}

void OmniboxPopupContainer::ChildPreferredSizeChanged(
    views::View* child) {
  if (parent() && (child->visible() || child_visible_))
    parent()->Layout();
  if (child_visible_ != child->visible()) {
    child_visible_ = child->visible();
    search_view_controller_->PopupVisibilityChanged();
  }
}

}  // namespace internal

// SearchViewController --------------------------------------------------------

SearchViewController::SearchViewController(
    content::BrowserContext* browser_context,
    ContentsContainer* contents_container,
    chrome::search::ToolbarSearchAnimator* toolbar_search_animator,
    ToolbarView* toolbar_view)
    : browser_context_(browser_context),
      contents_container_(contents_container),
      toolbar_search_animator_(toolbar_search_animator),
      toolbar_view_(toolbar_view),
      location_bar_container_(NULL),
      state_(STATE_NOT_VISIBLE),
      tab_contents_(NULL),
      search_container_(NULL),
      ntp_container_(NULL),
      content_view_(NULL),
      omnibox_popup_parent_(NULL) {
  omnibox_popup_parent_ = new internal::OmniboxPopupContainer(this);
}

SearchViewController::~SearchViewController() {
  if (search_model())
    search_model()->RemoveObserver(this);

  // If the |omnibox_popup_view_| isn't parented, delete it. Otherwise it'll be
  // deleted by its parent.
  if (!omnibox_popup_parent_->parent())
    delete omnibox_popup_parent_;
}

views::View* SearchViewController::omnibox_popup_parent() {
  return omnibox_popup_parent_;
}

void SearchViewController::SetTabContents(TabContents* tab_contents) {
  if (tab_contents_ == tab_contents)
    return;

  if (search_model())
    search_model()->RemoveObserver(this);
  tab_contents_ = tab_contents;
  if (search_model())
    search_model()->AddObserver(this);

  UpdateState();
}

void SearchViewController::StackAtTop() {
#if defined(USE_AURA)
  if (search_container_) {
    StackViewsLayerAtTop(search_container_);
    StackViewsLayerAtTop(ntp_container_);
    StackViewsLayerAtTop(GetLogoView());
    StackWebViewLayerAtTop(content_view_);
  }
#else
  NOTIMPLEMENTED();
#endif
  location_bar_container_->StackAtTop();
}

void SearchViewController::InstantReady() {
}

void SearchViewController::ModeChanged(const chrome::search::Mode& old_mode,
                                       const chrome::search::Mode& new_mode) {
  // When the mode changes from |SEARCH_SUGGESTIONS| to |DEFAULT| and omnibox
  // popup is still visible i.e. still retracting, delay state update, until the
  // omnibox popup has finished retracting and |PopupVisibilityChanged| has been
  // called; this persists all the necessary views for the duration of
  // the animated retraction of the omnibox popup.
  if (!(old_mode.mode == chrome::search::Mode::MODE_SEARCH_SUGGESTIONS &&
        new_mode.is_default() &&
        omnibox_popup_parent_->is_child_visible())) {
    UpdateState();
  }
}

gfx::Rect SearchViewController::GetNTPOmniboxBounds(views::View* destination) {
  if (!is_ntp_state(state_))
    return gfx::Rect();

  const float kNTPPageWidthRatio = 0.73f;
  int omnibox_width = kNTPPageWidthRatio * ntp_container_->bounds().width();
  int omnibox_x = (ntp_container_->bounds().width() - omnibox_width) / 2;
  gfx::Point omnibox_origin(omnibox_x,
                            GetLogoView()->bounds().bottom() +
                                chrome::search::kLogoBottomGap);
  views::View::ConvertPointToTarget(ntp_container_, destination,
                                    &omnibox_origin);
  return gfx::Rect(omnibox_origin.x(), omnibox_origin.y(),
                   omnibox_width, chrome::search::kNTPOmniboxHeight);
}

void SearchViewController::OnImplicitAnimationsCompleted() {
  DCHECK_EQ(STATE_NTP_ANIMATING, state_);
  state_ = STATE_SUGGESTIONS;
  ntp_container_->SetVisible(false);
  MaybeHideOverlay();
  // While |ntp_container_| was fading out, location bar was animating from the
  // middle of the NTP page to the top toolbar, at the same rate.
  // Suggestions need to be aligned with the final location of the location bar.
  // So if omnibox popup view (InlineOmniboxPopupView) is visible, force a
  // re-layout of its children (i.e. the suggestions) to align with the location
  // bar's final bounds.
  if (omnibox_popup_parent_->is_child_visible())
    omnibox_popup_parent_->child_at(0)->Layout();
}

// static
bool SearchViewController::is_ntp_state(State state) {
  return state == STATE_NTP || state == STATE_NTP_LOADING;
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

    case chrome::search::Mode::MODE_NTP_LOADING:
      new_state = STATE_NTP_LOADING;
      break;

    case chrome::search::Mode::MODE_NTP:
      new_state = STATE_NTP;
      break;

    case chrome::search::Mode::MODE_SEARCH_SUGGESTIONS:
      if (search_model()->mode().animate && is_ntp_state(state_))
        new_state = STATE_NTP_ANIMATING;
      else if (omnibox_popup_parent_->is_child_visible())
        new_state = STATE_SUGGESTIONS;
      break;

    case chrome::search::Mode::MODE_SEARCH_RESULTS:
      new_state = STATE_NOT_VISIBLE;
      break;
  }
  SetState(new_state);
}

void SearchViewController::SetState(State state) {
  if (state_ == state)
    return;

  State old_state = state_;
  switch (state) {
    case STATE_NOT_VISIBLE:
      DestroyViews();
      break;

    case STATE_NTP_LOADING:
    case STATE_NTP:
      DestroyViews();
      CreateViews(state);
      // In |CreateViews|, the main web contents view was reparented, so force
      // a search re-layout by |toolbar_view_| to re-position the omnibox per
      // the new bounds of web contents view.
      toolbar_view_->LayoutForSearch();
      break;

    case STATE_NTP_ANIMATING:
      // Should only animate from the ntp.
      DCHECK(is_ntp_state(old_state));
      StartAnimation();
      break;

    case STATE_SUGGESTIONS:
      DestroyViews();
      CreateViews(state);
      break;
  }
  state_ = state;
}

void SearchViewController::StartAnimation() {
  int factor = InstantUI::GetSlowAnimationScaleFactor();
  {
    ui::Layer* ntp_layer = ntp_container_->layer();
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
    ui::Layer* logo_layer = GetLogoView()->layer();
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
    ui::Layer* content_layer =
        content_view_->web_contents()->GetNativeView()->layer();
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

void SearchViewController::StopAnimation() {
  ntp_container_->layer()->GetAnimator()->StopAnimating();
  GetLogoView()->layer()->GetAnimator()->StopAnimating();
  if (content_view_->web_contents()) {
    content_view_->web_contents()->GetNativeView()->layer()->GetAnimator()->
        StopAnimating();
  }
}

void SearchViewController::CreateViews(State state) {
  DCHECK(!ntp_container_);

  ntp_container_ = new views::View;
  ntp_container_->set_background(new NTPViewBackground);
  ntp_container_->SetPaintToLayer(true);
  ntp_container_->layer()->SetMasksToBounds(true);

  const TemplateURL* default_provider =
      TemplateURLServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_))->
              GetDefaultSearchProvider();

#if defined(GOOGLE_CHROME_BUILD)
  if (default_provider &&
      (TemplateURLPrepopulateData::GetEngineType(default_provider->url()) ==
       SEARCH_ENGINE_GOOGLE)) {
    default_provider_logo_.reset(new views::ImageView());
    default_provider_logo_->set_owned_by_client();
    default_provider_logo_->SetImage(ui::ResourceBundle::GetSharedInstance().
                                         GetImageSkiaNamed(IDR_GOOGLE_LOGO_LG));
    default_provider_logo_->SetPaintToLayer(true);
    default_provider_logo_->SetFillsBoundsOpaquely(false);
  }
#endif

  if (!default_provider_logo_.get()) {
    default_provider_name_.reset(new views::Label(
        default_provider ? default_provider->short_name() : string16()));
    default_provider_name_->set_owned_by_client();
    // TODO(msw): Use a transparent background color as a workaround to support
    // using Labels' view layers via gfx::Canvas::NO_SUBPIXEL_RENDERING.
    default_provider_name_->SetBackgroundColor(0x00000000);
    default_provider_name_->set_background(
        views::Background::CreateSolidBackground(SK_ColorRED));
    default_provider_name_->SetEnabledColor(SK_ColorRED);
    default_provider_name_->SetFont(
        default_provider_name_->font().DeriveFont(75, gfx::Font::BOLD));
    default_provider_name_->SetPaintToLayer(true);
    default_provider_name_->SetFillsBoundsOpaquely(false);
  }

  // Reparent the main web contents view out of |contents_container_| and
  // in to |ntp_container_| below.  Reparent back in destructor.
  content_view_ = contents_container_->active();
  DCHECK(content_view_);
  contents_container_->SetActive(NULL);

  views::View* logo_view = GetLogoView();
  DCHECK(logo_view);
  ntp_container_->SetLayoutManager(
      new NTPViewLayoutManager(logo_view, content_view_));
  ntp_container_->AddChildView(logo_view);
  ntp_container_->AddChildView(content_view_);

  search_container_ =
      new SearchContainerView(ntp_container_, omnibox_popup_parent_);
  search_container_->SetPaintToLayer(true);
  search_container_->SetLayoutManager(new views::FillLayout);
  search_container_->layer()->SetMasksToBounds(true);

  contents_container_->SetOverlay(search_container_);

  if (state == STATE_SUGGESTIONS) {
    ntp_container_->SetVisible(false);
    MaybeHideOverlay();
  }

  // When loading, the |content_view_| needs to be invisible, but the web
  // contents that backs it needs to think it is visible so the back-end
  // renderer continues to draw.  Once drawing is complete, we'll receive
  // a state change to |STATE_NTP| and make the view itself visible.
  if (state == STATE_NTP_LOADING) {
    content_view_->SetVisible(false);
    content_view_->web_contents()->WasShown();
  } else {
    content_view_->SetVisible(true);
  }
}

views::View* SearchViewController::GetLogoView() const {
  if (default_provider_logo_.get())
    return default_provider_logo_.get();
  return default_provider_name_.get();
}

void SearchViewController::DestroyViews() {
  if (!search_container_)
    return;

  // Before destroying all views, cancel all animations to restore all views
  // into place.  Otherwise, the parent restoration of web_contents back to the
  // |main_contents_view_| does not work smoothly, resulting in an empty
  // contents page.  E.g. if the mode changes to |SEARCH_RESULTS| while views
  // are still animating, the results page will not show up if the animations
  // are not stopped before the views are destroyed.
  StopAnimation();

  // We persist the parent of the omnibox so that we don't have to inject a new
  // parent into ToolbarView.
  omnibox_popup_parent_->parent()->RemoveChildView(
      omnibox_popup_parent_);

  // Restore control/parenting of the web_contents back to the
  // |main_contents_view_|.
  ntp_container_->SetLayoutManager(NULL);
  ntp_container_->RemoveChildView(content_view_);
  if (content_view_->web_contents())
    content_view_->web_contents()->GetNativeView()->layer()->SetOpacity(1.0f);
  content_view_->SetVisible(true);
  contents_container_->SetActive(content_view_);
  contents_container_->SetOverlay(NULL);

  delete search_container_;
  search_container_ = NULL;
  ntp_container_ = NULL;
  default_provider_logo_.reset();
  default_provider_name_.reset();
  content_view_ = NULL;

  state_ = STATE_NOT_VISIBLE;
}

void SearchViewController::PopupVisibilityChanged() {
  // Don't do anything while animating if the child is visible. Otherwise we'll
  // prematurely cancel the animation.
  if (state_ != STATE_NTP_ANIMATING ||
      !omnibox_popup_parent_->is_child_visible()) {
    UpdateState();
    if (!omnibox_popup_parent_->is_child_visible())
      toolbar_search_animator_->OnOmniboxPopupClosed();
  }
}

void SearchViewController::MaybeHideOverlay() {
  search_container_->SetVisible(
      !InstantController::IsInstantEnabled(
          Profile::FromBrowserContext(browser_context_)));
}

chrome::search::SearchModel* SearchViewController::search_model() {
  return tab_contents_ ? tab_contents_->search_tab_helper()->model() : NULL;
}

content::WebContents* SearchViewController::web_contents() {
  return tab_contents_ ? tab_contents_->web_contents() : NULL;
}
