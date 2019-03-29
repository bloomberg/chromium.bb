// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Hover card and preview image dimensions.
int GetPreferredTabHoverCardWidth() {
  return TabStyle::GetStandardWidth();
}

gfx::Size GetTabHoverCardPreviewImageSize() {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetPreferredTabHoverCardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

bool AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

// Get delay threshold based on flag settings option selected. This is for
// user testing.
// TODO(corising): remove this after user study is completed.
base::TimeDelta GetMinimumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      0);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(500);
    case 1:
      return base::TimeDelta::FromMilliseconds(200);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

base::TimeDelta GetMaximumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      0);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(1000);
    case 1:
      return base::TimeDelta::FromMilliseconds(700);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

}  // namespace

// static
bool TabHoverCardBubbleView::disable_animations_for_testing_ = false;

// TODO(corising): Move this to a place where it could be used for all widgets.
class TabHoverCardBubbleView::WidgetFadeAnimationDelegate
    : public gfx::AnimationDelegate {
 public:
  explicit WidgetFadeAnimationDelegate(views::Widget* hover_card)
      : widget_(hover_card),
        fade_animation_(std::make_unique<gfx::LinearAnimation>(this)) {}
  ~WidgetFadeAnimationDelegate() override {}

  enum class FadeAnimationState {
    // No animation is running.
    IDLE,
    FADE_IN,
    FADE_OUT,
  };

  void set_animation_state(FadeAnimationState state) {
    animation_state_ = state;
  }

  bool IsFadingIn() const {
    return animation_state_ == FadeAnimationState::FADE_IN;
  }

  bool IsFadingOut() const {
    return animation_state_ == FadeAnimationState::FADE_OUT;
  }

  void FadeIn() {
    if (IsFadingIn())
      return;
    constexpr base::TimeDelta kFadeInDuration =
        base::TimeDelta::FromMilliseconds(200);
    set_animation_state(FadeAnimationState::FADE_IN);
    widget_->SetOpacity(0);
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    fade_animation_->SetDuration(kFadeInDuration);
    fade_animation_->Start();
  }

  void FadeOut() {
    if (IsFadingOut())
      return;
    constexpr base::TimeDelta kFadeOutDuration =
        base::TimeDelta::FromMilliseconds(150);
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    set_animation_state(FadeAnimationState::FADE_OUT);
    fade_animation_->SetDuration(kFadeOutDuration);
    fade_animation_->Start();
  }

  void CancelFadeOut() {
    if (!IsFadingOut())
      return;

    fade_animation_->Stop();
    set_animation_state(FadeAnimationState::IDLE);
    widget_->SetOpacity(1);
  }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override {
    // Get the value of the animation with a material ease applied.
    double value = gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                              animation->GetCurrentValue());
    float opaqueness = 0;
    if (IsFadingOut()) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f);
    } else if (animation_state_ == FadeAnimationState::FADE_IN) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f);
    }

    if (IsFadingOut() && opaqueness == 0) {
      widget_->Hide();
    } else {
      widget_->SetOpacity(opaqueness);
    }
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    AnimationProgressed(animation);
    set_animation_state(FadeAnimationState::IDLE);
  }

  views::Widget* const widget_;
  std::unique_ptr<gfx::LinearAnimation> fade_animation_;
  FadeAnimationState animation_state_ = FadeAnimationState::IDLE;

  DISALLOW_COPY_AND_ASSIGN(WidgetFadeAnimationDelegate);
};

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab, views::BubbleBorder::TOP_LEFT) {
  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);

  title_label_ =
      new views::Label(base::string16(), CONTEXT_TAB_HOVER_CARD_TITLE,
                       views::style::STYLE_PRIMARY);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(false);
  AddChildView(title_label_);

  domain_label_ = new views::Label(base::string16(), CONTEXT_BODY_TEXT_LARGE,
                                   ChromeTextStyle::STYLE_SECONDARY);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);
  AddChildView(domain_label_);

  if (AreHoverCardImagesEnabled()) {
    preview_image_ = new views::ImageView();
    preview_image_->SetVisible(AreHoverCardImagesEnabled());
    preview_image_->SetHorizontalAlignment(views::ImageViewBase::LEADING);
    preview_image_->SetImageSize(GetTabHoverCardPreviewImageSize());
    AddChildView(preview_image_);
  }

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  constexpr int kOuterMargin = 12;
  constexpr int kLineSpacing = 8;
  title_label_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(kOuterMargin, kOuterMargin, kLineSpacing, kOuterMargin));
  domain_label_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(kLineSpacing, kOuterMargin, kOuterMargin, kOuterMargin));

  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
  fade_animation_delegate_ =
      std::make_unique<WidgetFadeAnimationDelegate>(widget_);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateAndShow(Tab* tab) {
  if (preview_image_)
    preview_image_->SetVisible(!tab->IsActive());

  UpdateCardContent(tab->data());

  views::BubbleDialogDelegateView::SetAnchorView(tab);

  fade_animation_delegate_->CancelFadeOut();

  // Start trigger timer if necessary.
  if (!widget_->IsVisible()) {
    // Note that this will restart the timer if it is already running. If the
    // hover cards are not yet visible, moving the cursor within the tabstrip
    // will not trigger the hover cards.
    delayed_show_timer_.Start(FROM_HERE, GetDelay(tab->width()), this,
                              &TabHoverCardBubbleView::FadeInToShow);
  }
}

void TabHoverCardBubbleView::FadeOutToHide() {
  delayed_show_timer_.Stop();
  if (disable_animations_for_testing_) {
    widget_->Hide();
  } else {
    fade_animation_delegate_->FadeOut();
  }
}

int TabHoverCardBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::TimeDelta TabHoverCardBubbleView::GetDelay(int tab_width) const {
  // Delay is calculated as a logarithmic scale and bounded by a minimum width
  // based on the width of a pinned tab and a maximum of the standard width.
  //
  //  delay (ms)
  //           |
  // max delay-|                                    *
  //           |                          *
  //           |                    *
  //           |                *
  //           |            *
  //           |         *
  //           |       *
  //           |     *
  //           |    *
  // min delay-|****
  //           |___________________________________________ tab width
  //               |                                |
  //       pinned tab width               standard tab width
  base::TimeDelta minimum_trigger_delay = GetMinimumTriggerDelay();
  if (tab_width < TabStyle::GetPinnedWidth())
    return minimum_trigger_delay;
  base::TimeDelta maximum_trigger_delay = GetMaximumTriggerDelay();
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor =
      maximum_trigger_delay - minimum_trigger_delay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + minimum_trigger_delay;
  return delay;
}

void TabHoverCardBubbleView::FadeInToShow() {
  widget_->Show();
  if (!disable_animations_for_testing_)
    fade_animation_delegate_->FadeIn();
}

void TabHoverCardBubbleView::UpdateCardContent(TabRendererData data) {
  title_label_->SetText(data.title);

  base::string16 domain = url_formatter::FormatUrl(
      data.url,
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  domain_label_->SetText(domain);

  // If the preview image feature is not enabled, |preview_image_| will be null.
  if (preview_image_ && preview_image_->visible()) {
    // If there is no valid thumbnail data, blank out the preview, else wait for
    // the image data to be decoded and update momentarily.
    if (!data.thumbnail.AsImageSkiaAsync(
            base::BindOnce(&TabHoverCardBubbleView::UpdatePreviewImage,
                           weak_factory_.GetWeakPtr()))) {
      preview_image_->SetImage(gfx::ImageSkia());
    }
  }
}

void TabHoverCardBubbleView::UpdatePreviewImage(gfx::ImageSkia preview_image) {
  preview_image_->SetImage(preview_image);
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(GetPreferredTabHoverCardWidth());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}
