// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search/search_view_controller.h"

#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/search/toolbar_search_animator.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/location_bar/location_bar_container.h"
#include "chrome/browser/ui/views/search/search_ntp_container_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#endif

namespace {

// Stacks view's layer above all its sibling layers.
void StackViewsLayerAtTop(views::View* view) {
#if defined(USE_AURA)
  DCHECK(view->layer());
  DCHECK(view->layer()->parent());
  view->layer()->parent()->StackAtTop(view->layer());
#else
  // TODO(mad): http://crbug.com/156866 Properly stack views.
#endif  // USE_AURA
}

// Stacks native view's layer above all its sibling layers.
void StackWebViewLayerAtTop(views::WebView* view) {
  if (!view->web_contents())
    return;

#if defined(USE_AURA)
  ui::Layer* native_view_layer = view->web_contents()->GetNativeView()->layer();
  native_view_layer->parent()->StackAtTop(native_view_layer);
#else
  // TODO(mad): http://crbug.com/156866 Properly stack views
#endif  // USE_AURA
}

// SearchContainerView ---------------------------------------------------------

// SearchContainerView contains the |ntp_container_|, which is given the full
// size.
class SearchContainerView : public views::View {
 public:
  explicit SearchContainerView(views::View* ntp_container)
      : ntp_container_(ntp_container) {
    AddChildView(ntp_container);
  }

  virtual void Layout() OVERRIDE {
    ntp_container_->SetBounds(0, 0, width(), height());
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(chrome::search::kSearchBackgroundColor);
  }

 private:
  views::View* ntp_container_;

  DISALLOW_COPY_AND_ASSIGN(SearchContainerView);
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
    BrowserView* browser_view)
    : browser_context_(browser_context),
      contents_container_(contents_container),
      toolbar_search_animator_(toolbar_search_animator),
      browser_view_(browser_view),
      location_bar_container_(NULL),
      state_(STATE_NOT_VISIBLE),
      tab_contents_(NULL),
      search_container_(NULL),
      ntp_container_(NULL),
      content_view_(NULL),
      omnibox_popup_parent_(NULL),
      notify_css_background_y_pos_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  omnibox_popup_parent_ = new internal::OmniboxPopupContainer(this);

#if defined(ENABLE_THEMES)
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(
                         Profile::FromBrowserContext(browser_context_))));
#endif  // defined(ENABLE_THEMES)
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
  if (search_container_) {
    StackViewsLayerAtTop(search_container_);
    StackViewsLayerAtTop(ntp_container_);
    StackViewsLayerAtTop(GetLogoView());
    StackWebViewLayerAtTop(content_view_);
    // Instant should go on top.
    if (contents_container_->preview())
      StackWebViewLayerAtTop(contents_container_->preview());
  }
  location_bar_container_->StackAtTop();
}

void SearchViewController::WillCommitInstant() {
  // When Instant is committed, it will shuffle the views, so undo any of our
  // own view-shuffling first. http://crbug.com/152450
  DestroyViews();
}

void SearchViewController::ModeChanged(const chrome::search::Mode& old_mode,
                                       const chrome::search::Mode& new_mode) {
  // TODO(samarth): When the mode changes from |SEARCH_SUGGESTIONS| to
  // |DEFAULT|, we need to make sure the popup (currently, Instant) animates up.
  // We will need to wait for that animation before calling UpdateState() here.
  UpdateState();
}

gfx::Rect SearchViewController::GetNTPOmniboxBounds(views::View* destination) {
  if (!is_ntp_state(state_))
    return gfx::Rect();

  const int kOmniboxWidthMax = 742;
  const int kOmniboxWidthMin = 200;
  const int kOmniboxWidthPadding = 80;
  int omnibox_width = ntp_container_->bounds().width() -
      2 * kOmniboxWidthPadding;
  if (omnibox_width > kOmniboxWidthMax) {
    omnibox_width = kOmniboxWidthMax;
  } else if (omnibox_width < kOmniboxWidthMin) {
    omnibox_width = kOmniboxWidthMin;
  }

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

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SEARCH_VIEW_CONTROLLER_ANIMATION_FINISHED,
      content::Source<SearchViewController>(this),
      content::NotificationService::NoDetails());
}

// static
bool SearchViewController::is_ntp_state(State state) {
  return state == STATE_NTP || state == STATE_NTP_LOADING;
}

void SearchViewController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(ENABLE_THEMES)
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  if (ntp_container_)
    ntp_container_->set_to_notify_css_background_y_pos();
  else
    notify_css_background_y_pos_ = true;
#endif  // defined(ENABLE_THEMES)
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

  content::WebContents* saved = content_view_ ?
      content_view_->web_contents() : NULL;

  State old_state = state_;
  switch (state) {
    case STATE_NOT_VISIBLE:
      DestroyViews();
      break;

    case STATE_NTP_LOADING:
    case STATE_NTP:
      if (content_view_ && saved)
        content_view_->SetWebContents(NULL);
      DestroyViews();
      CreateViews(state);
      if (content_view_ && saved)
        content_view_->SetWebContents(saved);
      break;

    case STATE_NTP_ANIMATING:
      // Should only animate from the ntp.
      DCHECK(is_ntp_state(old_state));
      StartAnimation();
      break;

    case STATE_SUGGESTIONS:
      DestroyViews();
      break;
  }
  state_ = state;

  // In |CreateViews|, the main web contents view was reparented, so for
  // |STATE_NTP| and |STATE_NTP_LOADING|, force a search re-layout by
  // |ToolbarView| to re-position the omnibox per the new bounds of web
  // contents view.
  // Note: call |LayoutForSearch()| after |state_| has been updated, because
  // the former calls |GetNTPOmniboxBounds()| which accesses the latter.
  if (state_ == STATE_NTP_LOADING || state_ == STATE_NTP)
    browser_view_->toolbar()->LayoutForSearch();
}

void SearchViewController::StartAnimation() {
#if defined(USE_AURA)
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
#else
  // The rest of the code is expecting animation to complete async.
  MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SearchViewController::OnImplicitAnimationsCompleted,
                   weak_factory_.GetWeakPtr()));
  // TODO(mad): http://crbug.com/156869 make sure to get the same end result.
#endif  // !USE_AURA
}

void SearchViewController::StopAnimation() {
#if defined(USE_AURA)
  ntp_container_->layer()->GetAnimator()->StopAnimating();
  GetLogoView()->layer()->GetAnimator()->StopAnimating();
  if (content_view_->web_contents()) {
    content_view_->web_contents()->GetNativeView()->layer()->GetAnimator()->
        StopAnimating();
  }
#endif  // USE_AURA
}

void SearchViewController::CreateViews(State state) {
  DCHECK(!ntp_container_);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  ntp_container_ = new internal::SearchNTPContainerView(profile, browser_view_);
#if defined(USE_AURA)
  ntp_container_->SetPaintToLayer(true);
  ntp_container_->layer()->SetMasksToBounds(true);
#endif
  if (notify_css_background_y_pos_) {
    ntp_container_->set_to_notify_css_background_y_pos();
    notify_css_background_y_pos_ = false;
  }

  const TemplateURL* default_provider =
      TemplateURLServiceFactory::GetForProfile(profile)->
              GetDefaultSearchProvider();

  if (default_provider &&
      InstantUI::ShouldShowSearchProviderLogo(browser_context_) &&
      (TemplateURLPrepopulateData::GetEngineType(default_provider->url()) ==
       SEARCH_ENGINE_GOOGLE)) {
    default_provider_logo_.reset(new views::ImageView());
    default_provider_logo_->set_owned_by_client();
    ui::ThemeProvider* theme_provider = browser_view_->GetThemeProvider();
    DCHECK(theme_provider);
    // Use white logo if theme is used, else use regular (colored) logo.
    default_provider_logo_->SetImage(ui::ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(
            theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND) ?
                IDR_GOOGLE_LOGO_WHITE : IDR_GOOGLE_LOGO_LG));
#if defined(USE_AURA)
    default_provider_logo_->SetPaintToLayer(true);
    default_provider_logo_->SetFillsBoundsOpaquely(false);
#endif
  }

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
#if defined(USE_AURA)
    default_provider_name_->SetPaintToLayer(true);
    default_provider_name_->SetFillsBoundsOpaquely(false);
#endif
  }

  // Reparent the main web contents view out of |contents_container_| and
  // in to |ntp_container_| below.  Reparent back in destructor.
  content_view_ = contents_container_->active();
  DCHECK(content_view_);
  contents_container_->SetActive(NULL);

  views::View* logo_view = GetLogoView();
  DCHECK(logo_view);
  ntp_container_->SetLogoView(logo_view);
  ntp_container_->SetContentView(content_view_);

  search_container_ = new SearchContainerView(ntp_container_);
  search_container_->SetLayoutManager(new views::FillLayout);
#if defined(USE_AURA)
  search_container_->SetPaintToLayer(true);
  search_container_->layer()->SetMasksToBounds(true);
#endif
  contents_container_->SetOverlay(search_container_);

  // When loading, the |content_view_| needs to be invisible, but the web
  // contents that backs it needs to think it is visible so the back-end
  // renderer continues to draw.  Once drawing is complete, we'll receive
  // a state change to |STATE_NTP| and make the view itself visible.
  if (state == STATE_NTP_LOADING) {
    content_view_->SetWebContents(web_contents());
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

  // Restore control/parenting of the web_contents back to the
  // |main_contents_view_|.
  ntp_container_->SetContentView(NULL);
#if defined(USE_AURA)
  if (web_contents())
    web_contents()->GetNativeView()->layer()->SetOpacity(1.0f);
#endif  // USE_AURA
  content_view_->SetVisible(true);
  contents_container_->SetActive(content_view_);
  contents_container_->SetOverlay(NULL);
  if (search_model()) {
    if (notify_css_background_y_pos_) {
      ntp_container_->set_to_notify_css_background_y_pos();
      notify_css_background_y_pos_ = false;
    }
    ntp_container_->NotifyNTPBackgroundYPosIfChanged(
        contents_container_->active(), search_model()->mode());
  }

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

chrome::search::SearchModel* SearchViewController::search_model() {
  return tab_contents_ ? chrome::search::SearchTabHelper::FromWebContents(
                             tab_contents_->web_contents())->model()
                       : NULL;
}

content::WebContents* SearchViewController::web_contents() {
  return tab_contents_ ? tab_contents_->web_contents() : NULL;
}

views::View* SearchViewController::ntp_container() const {
  return ntp_container_;
}
