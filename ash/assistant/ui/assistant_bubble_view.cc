// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_bubble_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/ui/suggestion_container_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 32;
constexpr int kMinHeightDip = 200;
constexpr int kMaxHeightDip = 640;

// TODO(b/77638210): Replace with localized resource strings.
constexpr char kDefaultPrompt[] = "Hi, how can I help?";
constexpr char kStylusPrompt[] = "Draw with your stylus to select";

// TODO(dmblack): Try to use existing StyledLabel class.
// InteractionLabel ------------------------------------------------------------

class InteractionLabel : public views::View {
 public:
  explicit InteractionLabel(
      const AssistantInteractionModel* assistant_interaction_model)
      : assistant_interaction_model_(assistant_interaction_model),
        render_text_(gfx::RenderText::CreateHarfBuzzInstance()) {
    render_text_->SetFontList(render_text_->font_list().DeriveWithSizeDelta(4));
    render_text_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    render_text_->SetMultiline(true);
    ClearQuery();
  }

  ~InteractionLabel() override = default;

  // views::View:
  int GetHeightForWidth(int width) const override {
    if (width == 0)
      return 0;

    // Cache original |display_rect|.
    const gfx::Rect display_rect = render_text_->display_rect();

    // Measure |height| for |width|.
    render_text_->SetDisplayRect(gfx::Rect(width, 0));
    int height = render_text_->GetStringSize().height();

    // Restore original |display_rect|.
    render_text_->SetDisplayRect(display_rect);

    return height;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    render_text_->Draw(canvas);
  }

  void SetQuery(const AssistantQuery& query) {
    render_text_->SetColor(kTextColorPrimary);

    // Empty query.
    if (query.Empty()) {
      render_text_->SetText(GetPrompt());
    } else {
      // Populated query.
      switch (query.type()) {
        case AssistantQueryType::kText:
          SetTextQuery(static_cast<const AssistantTextQuery&>(query));
          break;
        case AssistantQueryType::kVoice:
          SetVoiceQuery(static_cast<const AssistantVoiceQuery&>(query));
          break;
        case AssistantQueryType::kEmpty:
          // Empty queries are already handled.
          NOTREACHED();
          break;
      }
    }
    PreferredSizeChanged();
    SchedulePaint();
  }

  void ClearQuery() { SetQuery(AssistantEmptyQuery()); }

 protected:
  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return render_text_->GetStringSize();
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    render_text_->SetDisplayRect(GetContentsBounds());
  }

 private:
  base::string16 GetPrompt() {
    switch (assistant_interaction_model_->input_modality()) {
      case InputModality::kStylus:
        return base::UTF8ToUTF16(kStylusPrompt);
      case InputModality::kKeyboard:  // fall through
      case InputModality::kVoice:
        return base::UTF8ToUTF16(kDefaultPrompt);
    }
  }

  void SetTextQuery(const AssistantTextQuery& query) {
    render_text_->SetText(base::UTF8ToUTF16(query.text()));
  }

  void SetVoiceQuery(const AssistantVoiceQuery& query) {
    const base::string16& high_confidence_speech =
        base::UTF8ToUTF16(query.high_confidence_speech());

    render_text_->SetText(high_confidence_speech);

    // We render low confidence speech in a different color than high
    // confidence speech for emphasis.
    if (!query.low_confidence_speech().empty()) {
      const base::string16& low_confidence_speech =
          base::UTF8ToUTF16(query.low_confidence_speech());

      render_text_->AppendText(low_confidence_speech);
      render_text_->ApplyColor(kTextColorHint,
                               gfx::Range(high_confidence_speech.length(),
                                          high_confidence_speech.length() +
                                              low_confidence_speech.length()));
    }
  }

  // Owned by AssistantController.
  const AssistantInteractionModel* const assistant_interaction_model_;
  std::unique_ptr<gfx::RenderText> render_text_;

  DISALLOW_COPY_AND_ASSIGN(InteractionLabel);
};

// InteractionContainer --------------------------------------------------------

class InteractionContainer : public views::View {
 public:
  explicit InteractionContainer(
      const AssistantInteractionModel* assistant_interaction_model)
      : interaction_label_(new InteractionLabel(assistant_interaction_model)) {
    InitLayout();
  }

  ~InteractionContainer() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void SetQuery(const AssistantQuery& query) {
    interaction_label_->SetQuery(query);
  }

  void ClearQuery() { interaction_label_->ClearQuery(); }

 private:
  void InitLayout() {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(0, kPaddingDip), 2 * kSpacingDip));

    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

    // Icon.
    views::ImageView* icon_view = new views::ImageView();
    icon_view->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
    icon_view->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
    icon_view->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
    AddChildView(icon_view);

    // Interaction label.
    AddChildView(interaction_label_);

    layout->SetFlexForView(interaction_label_, 1);
  }

  InteractionLabel* interaction_label_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(InteractionContainer);
};

}  // namespace

// AssistantBubbleView ---------------------------------------------------------

AssistantBubbleView::AssistantBubbleView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      caption_bar_(new CaptionBar()),
      interaction_container_(
          new InteractionContainer(assistant_controller->interaction_model())),
      ui_element_container_(new UiElementContainerView(assistant_controller)),
      suggestions_container_(new SuggestionContainerView(assistant_controller)),
      dialog_plate_(new DialogPlate(assistant_controller)),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  // Observe changes to interaction model.
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubbleView::~AssistantBubbleView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size AssistantBubbleView::CalculatePreferredSize() const {
  int preferred_height = GetHeightForWidth(kPreferredWidthDip);

  // When not using stylus input modality:
  // |min_height_dip_| <= preferred_height <= |kMaxHeightDip|.
  if (assistant_controller_->interaction_model()->input_modality() !=
      InputModality::kStylus) {
    preferred_height =
        std::min(std::max(preferred_height, min_height_dip_), kMaxHeightDip);
  }

  return gfx::Size(kPreferredWidthDip, preferred_height);
}

void AssistantBubbleView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  // Until Assistant UI is hidden, the view may grow in height but not shrink.
  // The exception to this rule is if using stylus input modality.
  min_height_dip_ = std::max(min_height_dip_, height());
}

void AssistantBubbleView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // We force a layout here because, though we are receiving a
  // ChildPreferredSizeChanged event, it may be that the
  // |ui_element_container_|'s bounds will not actually change due to the height
  // restrictions imposed by AssistantBubbleView. When this is the case, we
  // need to force a layout to see |ui_element_container_|'s new contents.
  if (child == ui_element_container_)
    Layout();
}

void AssistantBubbleView::ChildVisibilityChanged(views::View* child) {
  // When toggling the visibility of the caption bar or dialog plate, we also
  // need to update the padding of the layout.
  if (child == caption_bar_ || child == dialog_plate_) {
    const int padding_top_dip = caption_bar_->visible() ? 0 : kPaddingDip;
    const int padding_bottom_dip = dialog_plate_->visible() ? 0 : kPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets(padding_top_dip, 0, padding_bottom_dip, 0));
  }
  PreferredSizeChanged();
}

void AssistantBubbleView::InitLayout() {
  // Caption bar, dialog plate, suggestion container, and UI element container
  // are not visible when using stylus modality.
  const bool is_using_stylus =
      assistant_controller_->interaction_model()->input_modality() ==
      InputModality::kStylus;

  // There should be no vertical padding on the layout when the caption bar
  // and dialog plate are present.
  const int padding_vertical_dip = is_using_stylus ? kPaddingDip : 0;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(padding_vertical_dip, 0), kSpacingDip));

  // Caption bar.
  caption_bar_->SetVisible(!is_using_stylus);
  AddChildView(caption_bar_);

  // Interaction container.
  AddChildView(interaction_container_);

  // UI element container.
  ui_element_container_->SetVisible(!is_using_stylus);
  AddChildView(ui_element_container_);

  layout_manager_->SetFlexForView(ui_element_container_, 1);

  // Suggestions container.
  suggestions_container_->SetVisible(false);
  AddChildView(suggestions_container_);

  // Dialog plate.
  dialog_plate_->SetVisible(!is_using_stylus);
  AddChildView(dialog_plate_);
}

void AssistantBubbleView::OnInputModalityChanged(InputModality input_modality) {
  // Caption bar, dialog plate, suggestion container, and UI element container
  // are not visible when using stylus modality.
  caption_bar_->SetVisible(input_modality != InputModality::kStylus);
  dialog_plate_->SetVisible(input_modality != InputModality::kStylus);
  suggestions_container_->SetVisible(input_modality != InputModality::kStylus);
  ui_element_container_->SetVisible(input_modality != InputModality::kStylus);

  // If the query for the interaction is empty, we may need to update the prompt
  // to reflect the current input modality.
  if (assistant_controller_->interaction_model()->query().Empty()) {
    interaction_container_->ClearQuery();
  }
}

void AssistantBubbleView::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kInactive)
    return;

  // When the Assistant UI is being hidden we need to reset our minimum height
  // restriction so that the default restrictions are restored for the next
  // time the view is shown.
  min_height_dip_ = kMinHeightDip;
  PreferredSizeChanged();
}

void AssistantBubbleView::OnQueryChanged(const AssistantQuery& query) {
  interaction_container_->SetQuery(query);
}

void AssistantBubbleView::OnQueryCleared() {
  interaction_container_->ClearQuery();
}

void AssistantBubbleView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace ash
