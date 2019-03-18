// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_stage.h"

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_footer_view.h"
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace app_list {

namespace {

// Footer animation.
constexpr int kFooterAnimationTranslationDip = 22;
constexpr base::TimeDelta kFooterAnimationTranslationDelay =
    base::TimeDelta::FromMilliseconds(66);
constexpr base::TimeDelta kFooterAnimationTranslationDuration =
    base::TimeDelta::FromMilliseconds(416);
constexpr base::TimeDelta kFooterAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(149);
constexpr base::TimeDelta kFooterAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kFooterAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(100);

// Footer entry animation.
constexpr base::TimeDelta kFooterEntryAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kFooterEntryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// Progress animation.
constexpr base::TimeDelta kProgressAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(233);
constexpr base::TimeDelta kProgressAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// HorizontalSeparator ---------------------------------------------------------

// A horizontal line to separate the dialog plate.
class HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width, int preferred_height)
      : preferred_width_(preferred_width),
        preferred_height_(preferred_height) {}

  ~HorizontalSeparator() override = default;

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, preferred_height_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    constexpr SkColor kSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x0F);
    constexpr int kSeparatorThicknessDip = 2;
    gfx::Rect draw_bounds(GetContentsBounds());
    // TODO(wutao): To be finalized.
    const int inset_height =
        (draw_bounds.height() - kSeparatorThicknessDip) / 2;
    draw_bounds.Inset(0, inset_height);
    canvas->FillRect(draw_bounds, kSeparatorColor);
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;
  const int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

}  // namespace

// AssistantMainStage ----------------------------------------------------------

AssistantMainStage::AssistantMainStage(ash::AssistantViewDelegate* delegate)
    : delegate_(delegate),
      footer_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_started_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationStarted,
                  base::Unretained(this)),
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout();

  // The view hierarchy will be destructed before |assistant_controller_| in
  // Shell, which owns AssistantViewDelegate, so AssistantViewDelegate is
  // guaranteed to outlive the AssistantMainStage.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantMainStage::~AssistantMainStage() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantMainStage::GetClassName() const {
  return "AssistantMainStage";
}

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::InitLayout() {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);

  // The children of AssistantMainStage will be animated on their own layers and
  // we want them to be clipped by their parent layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  // Separators: the progress indicator and the horizontal separator will be the
  // separator when querying and showing the results, respectively. The height
  // of the horizontal separator is set to be the same as the progress indicator
  // in order to avoid height changes and relayout.
  // Progress indicator, which will be animated on its own layer.
  progress_indicator_ = new ash::AssistantProgressIndicator();
  progress_indicator_->SetPaintToLayer();
  progress_indicator_->layer()->SetFillsBoundsOpaquely(false);
  progress_indicator_->SetVisible(false);
  AddChildView(progress_indicator_);

  // Horizontal separator.
  // TODO(wutao): finalize the width.
  constexpr int kSeparatorWidthDip = 64;
  horizontal_separator_ = new HorizontalSeparator(
      kSeparatorWidthDip, progress_indicator_->GetPreferredSize().height());
  AddChildView(horizontal_separator_);

  // Query view.
  query_view_ = new ash::AssistantQueryView();
  AddChildView(query_view_);

  // UI element container.
  ui_element_container_ = new ash::UiElementContainerView(delegate_);
  AddChildView(ui_element_container_);

  layout->SetFlexForView(ui_element_container_, 1,
                         /*use_min_size=*/true);

  // Footer.
  // Note that the |footer_| is placed within its own view container so that as
  // its visibility changes, its parent container will still reserve the same
  // layout space. This prevents jank that would otherwise occur due to
  // |ui_element_container_| claiming that empty space.
  views::View* footer_container = new views::View();
  footer_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  footer_ = new ash::AssistantFooterView(delegate_);
  footer_->AddObserver(this);

  // The footer will be animated on its own layer.
  footer_->SetPaintToLayer();
  footer_->layer()->SetFillsBoundsOpaquely(false);

  footer_container->AddChildView(footer_);
  AddChildView(footer_container);
}

void AssistantMainStage::OnCommittedQueryChanged(
    const ash::AssistantQuery& query) {
  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;

  // TODO(wutao): Replace the visibility change by animations.
  horizontal_separator_->SetVisible(false);

  // Show the progress indicator.
  progress_indicator_->SetVisible(true);
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kProgressAnimationFadeInDelay),
          // ...then fade in.
          ash::assistant::util::CreateOpacityElement(
              1.f, kProgressAnimationFadeInDuration)));

  // Update the view.
  query_view_->SetQuery(query);
}

void AssistantMainStage::OnPendingQueryChanged(
    const ash::AssistantQuery& query) {
  query_view_->SetQuery(query);
}

void AssistantMainStage::OnResponseChanged(
    const std::shared_ptr<ash::AssistantResponse>& response) {
  // TODO(wutao): Replace the visibility change by animations.
  horizontal_separator_->SetVisible(true);
  progress_indicator_->SetVisible(false);

  UpdateFooter();
}

void AssistantMainStage::OnUiVisibilityChanged(
    ash::AssistantVisibility new_visibility,
    ash::AssistantVisibility old_visibility,
    base::Optional<ash::AssistantEntryPoint> entry_point,
    base::Optional<ash::AssistantExitPoint> exit_point) {
  if (ash::assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the footer.
    using ash::assistant::util::CreateLayerAnimationSequence;
    using ash::assistant::util::CreateOpacityElement;
    using ash::assistant::util::CreateTransformElement;

    // Set up our pre-animation values.
    footer_->layer()->SetOpacity(0.f);

    // Animate the footer to 100% opacity with delay.
    footer_->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kFooterEntryAnimationFadeInDelay),
            CreateOpacityElement(1.f, kFooterEntryAnimationFadeInDuration)));
    return;
  }

  if (!ash::assistant::util::IsFinishingSession(new_visibility))
    return;

  progress_indicator_->layer()->SetOpacity(0.f);
  progress_indicator_->layer()->SetTransform(gfx::Transform());

  UpdateFooter();
}

void AssistantMainStage::UpdateFooter() {
  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;
  using ash::assistant::util::CreateTransformElement;
  using ash::assistant::util::StartLayerAnimationSequence;
  using ash::assistant::util::StartLayerAnimationSequencesTogether;

  // The footer is only visible when the progress indicator is not.
  // When it is not visible, it should not process events.
  bool visible = !progress_indicator_->visible();

  // Reset visibility to enable animation.
  footer_->SetVisible(true);

  if (visible) {
    // The footer will animate up into position so we need to set an initial
    // offset transformation from which to animate.
    gfx::Transform transform;
    transform.Translate(0, kFooterAnimationTranslationDip);
    footer_->layer()->SetTransform(transform);

    // Animate the entry of the footer.
    StartLayerAnimationSequencesTogether(
        footer_->layer()->GetAnimator(),
        {// Animate the translation with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                 kFooterAnimationTranslationDelay),
             CreateTransformElement(gfx::Transform(),
                                    kFooterAnimationTranslationDuration,
                                    gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate the fade in with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kFooterAnimationFadeInDelay),
             CreateOpacityElement(1.f, kFooterAnimationFadeInDuration))},
        // Observer animation start/end events.
        footer_animation_observer_.get());
  } else {
    // Animate the exit of the footer.
    StartLayerAnimationSequence(
        footer_->layer()->GetAnimator(),
        // Animate fade out.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.f, kFooterAnimationFadeOutDuration)),
        // Observe animation start/end events.
        footer_animation_observer_.get());
  }

  // Set the observer to active so that we'll receive start/end events.
  footer_animation_observer_->SetActive();
}

void AssistantMainStage::OnFooterAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should not process events while animating.
  footer_->set_can_process_events_within_subtree(/*can_process=*/false);
}

bool AssistantMainStage::OnFooterAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should only process events when visible. It is only visible when
  // the progress indicator is not visible.
  bool visible = !progress_indicator_->visible();
  footer_->set_can_process_events_within_subtree(visible);
  footer_->SetVisible(visible);

  // Return false so that the observer does not destroy itself.
  return false;
}

}  // namespace app_list
