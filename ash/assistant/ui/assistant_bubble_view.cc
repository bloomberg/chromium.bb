// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_bubble_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "ui/app_list/views/suggestion_chip_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPaddingDip = 14;
constexpr int kPreferredWidthDip = 640;
constexpr int kSpacingDip = 8;
constexpr SkColor kTextBackgroundColor = SkColorSetARGB(0x8A, 0x42, 0x85, 0xF4);
constexpr int kTextCornerRadiusDip = 16;
constexpr int kTextPaddingHorizontalDip = 12;
constexpr int kTextPaddingVerticalDip = 4;

// Typography.
constexpr SkColor kTextColorHint = SkColorSetA(SK_ColorBLACK, 0x42);
constexpr SkColor kTextColorPrimary = SkColorSetA(SK_ColorBLACK, 0xDE);

// TODO(dmblack): Remove after removing placeholders.
// Placeholder.
constexpr SkColor kPlaceholderColor = SkColorSetA(SK_ColorBLACK, 0x1F);
constexpr int kPlaceholderIconSizeDip = 32;

// TODO(b/77638210): Replace with localized resource strings.
constexpr char kDefaultPrompt[] = "Hi, how can I help?";
constexpr char kStylusPrompt[] = "Draw with your stylus to select";

// TODO(dmblack): Remove after removing placeholders.
// RoundRectBackground ---------------------------------------------------------

class RoundRectBackground : public views::Background {
 public:
  RoundRectBackground(SkColor color, int corner_radius)
      : color_(color), corner_radius_(corner_radius) {}

  ~RoundRectBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawRoundRect(view->GetContentsBounds(), corner_radius_, flags);
  }

 private:
  const SkColor color_;
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectBackground);
};

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

    // TODO(dmblack): Implement stateful icon. Icon will change state in
    // correlation with speech recognition events.
    // Icon placeholder.
    views::View* icon_placeholder = new views::View();
    icon_placeholder->SetBackground(std::make_unique<RoundRectBackground>(
        kPlaceholderColor, kPlaceholderIconSizeDip / 2));
    icon_placeholder->SetPreferredSize(
        gfx::Size(kPlaceholderIconSizeDip, kPlaceholderIconSizeDip));
    AddChildView(icon_placeholder);

    // Interaction label.
    AddChildView(interaction_label_);

    layout->SetFlexForView(interaction_label_, 1);
  }

  InteractionLabel* interaction_label_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(InteractionContainer);
};

// UiElementContainer ----------------------------------------------------------

class UiElementContainer : public views::View {
 public:
  UiElementContainer() { InitLayout(); }

  ~UiElementContainer() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void AddText(const std::string& text) {
    // Container.
    views::View* text_container = new views::View();
    text_container->SetBackground(std::make_unique<RoundRectBackground>(
        kTextBackgroundColor, kTextCornerRadiusDip));
    text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets(kTextPaddingVerticalDip, kTextPaddingHorizontalDip)));

    // Label.
    views::Label* text_view = new views::Label(base::UTF8ToUTF16(text));
    text_view->SetAutoColorReadabilityEnabled(false);
    text_view->SetEnabledColor(kTextColorPrimary);
    text_view->SetFontList(text_view->font_list().DeriveWithSizeDelta(4));
    text_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    text_view->SetMultiLine(true);

    text_container->AddChildView(text_view);
    AddChildView(text_container);

    PreferredSizeChanged();
  }

  void EmbedCard(const base::UnguessableToken& embed_token) {
    // When the card has been rendered in the same process, its view is
    // available in the AnswerCardContentsRegistry's token-to-view map.
    if (app_list::AnswerCardContentsRegistry::Get()) {
      AddChildView(
          app_list::AnswerCardContentsRegistry::Get()->GetView(embed_token));
    }
    // TODO(dmblack): Handle Mash case.
  }

  void ClearUiElements() {
    RemoveAllChildViews(true);
    PreferredSizeChanged();
  }

 private:
  void InitLayout() {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical,
            gfx::Insets(0, kPaddingDip), kSpacingDip));

    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);
  }

  DISALLOW_COPY_AND_ASSIGN(UiElementContainer);
};

// TODO(dmblack): Container should wrap chips in a horizontal scroll view.
// SuggestionsContainer --------------------------------------------------------

class SuggestionsContainer : public views::View {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  explicit SuggestionsContainer(app_list::SuggestionChipListener* listener)
      : suggestion_chip_listener_(listener) {}

  ~SuggestionsContainer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    int width = 0;
    int height = 0;

    for (int i = 0; i < child_count(); i++) {
      const views::View* child = child_at(i);
      gfx::Size child_size = child->GetPreferredSize();

      if (i > 0) {
        // Add spacing between chips.
        width += kSpacingDip;
      }

      width += child_size.width();
      height = std::max(child_size.height(), height);
    }

    if (width > 0) {
      // Add horizontal padding.
      width += 2 * kPaddingDip;
    }

    return gfx::Size(width, height);
  }

  // views::View:
  void Layout() override {
    const int height = views::View::height();
    int left = kPaddingDip;

    for (int i = 0; i < child_count(); i++) {
      views::View* child = child_at(i);
      gfx::Size child_size = child->GetPreferredSize();

      child->SetBounds(left, (height - child_size.height()) / 2,
                       child_size.width(), child_size.height());

      left += child_size.width() + kSpacingDip;
    }
  }

  void AddSuggestions(const std::map<int, AssistantSuggestion*>& suggestions) {
    for (const std::pair<int, AssistantSuggestion*>& suggestion : suggestions) {
      app_list::SuggestionChipView::Params params;
      params.text = base::UTF8ToUTF16(suggestion.second->text);

      // TODO(dmblack): Here we are using a placeholder image for the suggestion
      // chip icon but we need to handle the actual icon URL.
      if (!suggestion.second->icon_url.is_empty())
        params.icon = gfx::CreateVectorIcon(
            kCircleIcon, app_list::SuggestionChipView::kIconSizeDip,
            gfx::kGoogleGrey300);

      views::View* suggestion_chip_view =
          new app_list::SuggestionChipView(params, suggestion_chip_listener_);

      // When adding a SuggestionChipView, we give the view the same id by which
      // the interaction model identifies the corresponding suggestion. This
      // allows us to look up the suggestion for the view during event handling.
      suggestion_chip_view->set_id(suggestion.first);

      AddChildView(suggestion_chip_view);
    }
    PreferredSizeChanged();
  }

  void ClearSuggestions() {
    RemoveAllChildViews(true);
    PreferredSizeChanged();
  }

 private:
  app_list::SuggestionChipListener* const suggestion_chip_listener_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsContainer);
};

}  // namespace

// AssistantBubbleView ---------------------------------------------------------

AssistantBubbleView::AssistantBubbleView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      interaction_container_(
          new InteractionContainer(assistant_controller->interaction_model())),
      ui_element_container_(new UiElementContainer()),
      suggestions_container_(new SuggestionsContainer(this)),
      dialog_plate_(new DialogPlate(assistant_controller)),
      render_request_weak_factory_(this) {
  InitLayout();

  // Observe changes to interaction model.
  DCHECK(assistant_controller_);
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubbleView::~AssistantBubbleView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
  OnReleaseCards();
}

gfx::Size AssistantBubbleView::CalculatePreferredSize() const {
  int preferred_height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kPreferredWidthDip);
  return gfx::Size(kPreferredWidthDip, preferred_height);
}

void AssistantBubbleView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantBubbleView::ChildVisibilityChanged(views::View* child) {
  // When toggling the visibility of the dialog plate, we also need to update
  // the bottom padding of the layout.
  if (child == dialog_plate_) {
    const int padding_bottom_dip = dialog_plate_->visible() ? 0 : kPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets(kPaddingDip, 0, padding_bottom_dip, 0));
  }
  PreferredSizeChanged();
}

void AssistantBubbleView::InitLayout() {
  // Dialog plate is not visible when using the stylus input modality.
  const bool show_dialog_plate =
      assistant_controller_->interaction_model()->input_modality() !=
      InputModality::kStylus;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kPaddingDip, 0, show_dialog_plate ? 0 : kPaddingDip, 0),
      kSpacingDip));

  // Interaction container.
  AddChildView(interaction_container_);

  // UI element container.
  ui_element_container_->SetVisible(false);
  AddChildView(ui_element_container_);

  // Suggestions container.
  suggestions_container_->SetVisible(false);
  AddChildView(suggestions_container_);

  // Dialog plate.
  dialog_plate_->SetVisible(show_dialog_plate);
  AddChildView(dialog_plate_);
}

void AssistantBubbleView::SetProcessingUiElement(bool is_processing) {
  if (is_processing == is_processing_ui_element_)
    return;

  is_processing_ui_element_ = is_processing;

  // If we are no longer processing a UI element, we need to handle anything
  // that was put in the pending queue. Note that the elements left in the
  // pending queue may themselves require processing that again pends the queue.
  if (!is_processing_ui_element_)
    ProcessPendingUiElements();
}

void AssistantBubbleView::ProcessPendingUiElements() {
  while (!is_processing_ui_element_ && !pending_ui_element_list_.empty()) {
    const AssistantUiElement* ui_element = pending_ui_element_list_.front();
    pending_ui_element_list_.pop_front();
    OnUiElementAdded(ui_element);
  }
}

void AssistantBubbleView::OnInputModalityChanged(InputModality input_modality) {
  // Dialog plate is not visible when using stylus input modality.
  dialog_plate_->SetVisible(input_modality != InputModality::kStylus);

  // If the query for the interaction is empty, we may need to update the prompt
  // to reflect the current input modality.
  if (assistant_controller_->interaction_model()->query().Empty()) {
    interaction_container_->ClearQuery();
  }
}

void AssistantBubbleView::OnUiElementAdded(
    const AssistantUiElement* ui_element) {
  // If we are processing a UI element we need to pend the incoming element
  // instead of handling it immediately.
  if (is_processing_ui_element_) {
    pending_ui_element_list_.push_back(ui_element);
    return;
  }

  switch (ui_element->GetType()) {
    case AssistantUiElementType::kCard:
      OnCardAdded(static_cast<const AssistantCardElement*>(ui_element));
      break;
    case AssistantUiElementType::kText:
      OnTextAdded(static_cast<const AssistantTextElement*>(ui_element));
      break;
  }
}

void AssistantBubbleView::OnUiElementsCleared() {
  // Prevent any in-flight card rendering requests from returning.
  render_request_weak_factory_.InvalidateWeakPtrs();

  ui_element_container_->SetVisible(false);
  ui_element_container_->ClearUiElements();

  OnReleaseCards();

  // We can clear any pending UI elements as they are no longer relevant.
  pending_ui_element_list_.clear();
  SetProcessingUiElement(false);
}

void AssistantBubbleView::OnCardAdded(
    const AssistantCardElement* card_element) {
  DCHECK(!is_processing_ui_element_);

  // We need to pend any further UI elements until the card has been rendered.
  // This insures that views will be added to the view hierarchy in the order in
  // which they were received.
  SetProcessingUiElement(true);

  // Generate a unique identifier for the card. This will be used to clean up
  // card resources when it is no longer needed.
  base::UnguessableToken id_token = base::UnguessableToken::Create();

  // Configure parameters for the card.
  ash::mojom::AssistantCardParamsPtr params(
      ash::mojom::AssistantCardParams::New());
  params->html = card_element->GetHtml();
  params->min_width_dip = kPreferredWidthDip - 2 * kPaddingDip;
  params->max_width_dip = kPreferredWidthDip - 2 * kPaddingDip;

  // The card will be rendered by AssistantCardRenderer, running the specified
  // callback when the card is ready for embedding.
  assistant_controller_->RenderCard(
      id_token, std::move(params),
      base::BindOnce(&AssistantBubbleView::OnCardReady,
                     render_request_weak_factory_.GetWeakPtr()));

  // Cache the card identifier for freeing up resources when it is no longer
  // needed.
  id_token_list_.push_back(id_token);
}

void AssistantBubbleView::OnCardReady(
    const base::UnguessableToken& embed_token) {
  ui_element_container_->EmbedCard(embed_token);
  ui_element_container_->SetVisible(true);

  // Once the card has been rendered and embedded, we can resume processing
  // any UI elements that are in the pending queue.
  SetProcessingUiElement(false);
}

void AssistantBubbleView::OnReleaseCards() {
  if (!id_token_list_.empty()) {
    // Release any resources associated with the cards identified in
    // |id_token_list_| owned by AssistantCardRenderer.
    assistant_controller_->ReleaseCards(id_token_list_);
    id_token_list_.clear();
  }
}

void AssistantBubbleView::OnQueryChanged(const AssistantQuery& query) {
  interaction_container_->SetQuery(query);
}

void AssistantBubbleView::OnQueryCleared() {
  interaction_container_->ClearQuery();
}

void AssistantBubbleView::OnSuggestionsAdded(
    const std::map<int, AssistantSuggestion*>& suggestions) {
  suggestions_container_->AddSuggestions(suggestions);
  suggestions_container_->SetVisible(true);
}

void AssistantBubbleView::OnSuggestionsCleared() {
  suggestions_container_->ClearSuggestions();
  suggestions_container_->SetVisible(false);
}

void AssistantBubbleView::OnSuggestionChipPressed(
    app_list::SuggestionChipView* suggestion_chip_view) {
  assistant_controller_->OnSuggestionChipPressed(suggestion_chip_view->id());
}

void AssistantBubbleView::OnTextAdded(
    const AssistantTextElement* text_element) {
  DCHECK(!is_processing_ui_element_);

  ui_element_container_->AddText(text_element->GetText());
  ui_element_container_->SetVisible(true);
}

void AssistantBubbleView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace ash
