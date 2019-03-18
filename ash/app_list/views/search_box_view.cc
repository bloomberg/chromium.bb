// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/resources/grit/app_list_resources.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ash::ColorProfileType;

namespace app_list {

namespace {

constexpr int kPaddingSearchResult = 16;
constexpr int kSearchBoxBorderWidth = 4;

constexpr SkColor kSearchBoxBorderColor =
    SkColorSetARGB(0x3D, 0xFF, 0xFF, 0xFF);

constexpr int kAssistantIconSize = 24;
constexpr int kCloseIconSize = 24;
constexpr int kSearchBoxFocusBorderCornerRadius = 28;

// Range of the fraction of app list from collapsed to peeking that search box
// should change opacity.
constexpr float kOpacityStartFraction = 0.11f;
constexpr float kOpacityEndFraction = 1.0f;

// Minimum amount of characters required to enable autocomplete.
constexpr int kMinimumLengthToAutocomplete = 2;

// Gets the box layout inset horizontal padding for the state of AppListModel.
int GetBoxLayoutPaddingForState(ash::AppListState state) {
  if (state == ash::AppListState::kStateSearchResults)
    return kPaddingSearchResult;
  return search_box::kPadding;
}

float GetAssistantButtonOpacityForState(ash::AppListState state) {
  if (state == ash::AppListState::kStateSearchResults)
    return .0f;
  return 1.f;
}

}  // namespace

SearchBoxView::SearchBoxView(search_box::SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : search_box::SearchBoxViewBase(delegate),
      view_delegate_(view_delegate),
      app_list_view_(app_list_view),
      is_app_list_search_autocomplete_enabled_(
          app_list_features::IsAppListSearchAutocompleteEnabled()),
      weak_ptr_factory_(this) {
  set_is_tablet_mode(app_list_view->is_tablet_mode());
  if (app_list_features::IsZeroStateSuggestionsEnabled())
    set_show_close_button_when_active(true);
}

SearchBoxView::~SearchBoxView() {
  search_model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::ClearSearch() {
  search_box::SearchBoxViewBase::ClearSearch();
  app_list_view_->SetStateFromSearchBoxView(
      true, false /*triggered_by_contents_change*/);
}

views::View* SearchBoxView::GetSelectedViewInContentsView() {
  if (!contents_view_)
    return nullptr;
  return contents_view_->GetSelectedView();
}

void SearchBoxView::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::ET_MOUSEWHEEL) {
    if (!app_list_view_->HandleScroll(
            located_event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL)) {
      return;
    }
  }
  search_box::SearchBoxViewBase::HandleSearchBoxEvent(located_event);
}

void SearchBoxView::ModelChanged() {
  if (search_model_)
    search_model_->search_box()->RemoveObserver(this);

  search_model_ = view_delegate_->GetSearchModel();
  DCHECK(search_model_);
  UpdateSearchIcon();
  search_model_->search_box()->AddObserver(this);

  HintTextChanged();
  OnWallpaperColorsChanged();
  ShowAssistantChanged();
}

void SearchBoxView::UpdateKeyboardVisibility() {
  if (!keyboard::KeyboardController::HasInstance())
    return;
  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  if (!keyboard_controller->IsEnabled() ||
      is_search_box_active() == keyboard_controller->IsKeyboardVisible()) {
    return;
  }

  if (is_search_box_active()) {
    keyboard_controller->ShowKeyboard(false);
    return;
  }

  keyboard_controller->HideKeyboardByUser();
}

void SearchBoxView::UpdateModel(bool initiated_by_user) {
  // Temporarily remove from observer to ignore notifications caused by us.
  search_model_->search_box()->RemoveObserver(this);
  search_model_->search_box()->Update(search_box()->text(), initiated_by_user);
  search_model_->search_box()->SetSelectionModel(
      search_box()->GetSelectionModel());
  search_model_->search_box()->AddObserver(this);
}

void SearchBoxView::UpdateSearchIcon() {
  const gfx::VectorIcon& google_icon =
      is_search_box_active() ? kGoogleColorIcon : kGoogleBlackIcon;
  const gfx::VectorIcon& icon = search_model_->search_engine_is_google()
                                    ? google_icon
                                    : kSearchEngineNotGoogleIcon;
  SetSearchIconImage(gfx::CreateVectorIcon(icon, search_box::kSearchIconSize,
                                           search_box_color()));
}

void SearchBoxView::UpdateSearchBoxBorder() {
  if (search_box()->HasFocus() && !is_search_box_active() &&
      !is_tablet_mode()) {
    // Show a gray ring around search box to indicate that the search box is
    // selected. Do not show it when search box is active, because blinking
    // cursor already indicates that.
    SetBorder(views::CreateRoundedRectBorder(kSearchBoxBorderWidth,
                                             kSearchBoxFocusBorderCornerRadius,
                                             kSearchBoxBorderColor));
    return;
  }

  // Creates an empty border as a placeholder for colored border so that
  // re-layout won't move views below the search box.
  SetBorder(
      views::CreateEmptyBorder(kSearchBoxBorderWidth, kSearchBoxBorderWidth,
                               kSearchBoxBorderWidth, kSearchBoxBorderWidth));
}

void SearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetImage(views::ImageButton::STATE_NORMAL,
                  gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseIconSize,
                                        search_box_color()));
  close->SetVisible(false);
  base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
  close->SetAccessibleName(close_button_label);
  close->SetTooltipText(close_button_label);
}

void SearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  back->SetImage(views::ImageButton::STATE_NORMAL,
                 rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
  back->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                          views::ImageButton::ALIGN_MIDDLE);
  back->SetVisible(false);
  base::string16 back_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_BACK));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
}

void SearchBoxView::RecordSearchBoxActivationHistogram(
    ui::EventType event_type) {
  search_box::ActivationSource activation_type;
  switch (event_type) {
    case ui::ET_GESTURE_TAP:
      activation_type = search_box::ActivationSource::kGestureTap;
      break;
    case ui::ET_MOUSE_PRESSED:
      activation_type = search_box::ActivationSource::kMousePress;
      break;
    case ui::ET_KEY_PRESSED:
      activation_type = search_box::ActivationSource::kKeyPress;
      break;
    default:
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchBoxActivated", activation_type);
}

void SearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  app_list_view_->RedirectKeyEventToSearchBox(event);

  if (!IsUnhandledUpDownKeyEvent(*event))
    return;

  // Handles arrow key events from the search box while the search box is
  // inactive. This covers both folder traversal and apps grid traversal. Search
  // result traversal is handled in |HandleKeyEvent|
  AppListPage* page =
      contents_view_->GetPageView(contents_view_->GetActivePageIndex());
  views::View* arrow_view = contents_view_->expand_arrow_view();
  views::View* next_view = nullptr;

  if (event->key_code() == ui::VKEY_UP) {
    if (arrow_view && arrow_view->IsFocusable())
      next_view = arrow_view;
    else
      next_view = page->GetLastFocusableView();
  } else {
    next_view = page->GetFirstFocusableView();
  }

  if (next_view)
    next_view->RequestFocus();
  event->SetHandled();
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);
  return false;
}

void SearchBoxView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (HasAutocompleteText()) {
    node_data->role = ax::mojom::Role::kTextField;
    node_data->SetValue(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_SEARCH_BOX_AUTOCOMPLETE, search_box()->text()));
  }
}

void SearchBoxView::UpdateBackground(double progress,
                                     ash::AppListState current_state,
                                     ash::AppListState target_state) {
  SetSearchBoxBackgroundCornerRadius(gfx::Tween::LinearIntValueBetween(
      progress, GetSearchBoxBorderCornerRadiusForState(current_state),
      GetSearchBoxBorderCornerRadiusForState(target_state)));
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(current_state),
      GetBackgroundColorForState(target_state));
  UpdateBackgroundColor(color);
}

void SearchBoxView::UpdateLayout(double progress,
                                 ash::AppListState current_state,
                                 ash::AppListState target_state) {
  const int horizontal_spacing = gfx::Tween::LinearIntValueBetween(
      progress, GetBoxLayoutPaddingForState(current_state),
      GetBoxLayoutPaddingForState(target_state));
  box_layout()->set_inside_border_insets(
      gfx::Insets(0, horizontal_spacing, 0, 0));
  box_layout()->set_between_child_spacing(horizontal_spacing);
  if (show_assistant_button()) {
    assistant_button()->layer()->SetOpacity(gfx::Tween::LinearIntValueBetween(
        progress, GetAssistantButtonOpacityForState(current_state),
        GetAssistantButtonOpacityForState(target_state)));
  }
  InvalidateLayout();
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults &&
      !app_list_view_->is_in_drag()) {
    return search_box::kSearchBoxBorderCornerRadiusSearchResult;
  }
  return search_box::kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults)
    return AppListConfig::instance().card_background_color();
  return search_box::kSearchBoxBackgroundDefault;
}

void SearchBoxView::UpdateOpacity() {
  // The opacity of searchbox is a function of the fractional displacement of
  // the app list from collapsed(0) to peeking(1) state. When the fraction
  // changes from |kOpacityStartFraction| to |kOpaticyEndFraction|, the opacity
  // of searchbox changes from 0.f to 1.0f.
  if (!contents_view_->GetPageView(contents_view_->GetActivePageIndex())
           ->ShouldShowSearchBox()) {
    return;
  }
  const int shelf_height = AppListConfig::instance().shelf_height();
  float fraction =
      std::max<float>(
          0, contents_view_->app_list_view()->GetCurrentAppListHeight() -
                 shelf_height) /
      (AppListConfig::instance().peeking_app_list_height() - shelf_height);

  float opacity =
      std::min(std::max((fraction - kOpacityStartFraction) /
                            (kOpacityEndFraction - kOpacityStartFraction),
                        0.f),
               1.0f);

  AppListView* app_list_view = contents_view_->app_list_view();
  bool should_restore_opacity =
      !app_list_view->is_in_drag() &&
      (app_list_view->app_list_state() != AppListViewState::CLOSED);
  // Restores the opacity of searchbox if the gesture dragging ends.
  this->layer()->SetOpacity(should_restore_opacity ? 1.0f : opacity);
  contents_view_->search_results_page_view()->layer()->SetOpacity(
      should_restore_opacity ? 1.0f : opacity);
}

void SearchBoxView::ShowZeroStateSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("AppList_ShowZeroStateSuggestions"));
  base::string16 empty_query;
  ContentsChanged(search_box(), empty_query);
}

void SearchBoxView::OnWallpaperColorsChanged() {
  GetWallpaperProminentColors(
      base::BindOnce(&SearchBoxView::OnWallpaperProminentColorsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchBoxView::ProcessAutocomplete() {
  if (!ShouldProcessAutocomplete())
    return;

  SearchResult* const first_visible_result =
      search_model_->GetFirstVisibleResult();

  // Current non-autocompleted text.
  const base::string16& user_typed_text =
      search_box()->text().substr(0, highlight_range_.start());
  if (last_key_pressed_ == ui::VKEY_BACK ||
      last_key_pressed_ == ui::VKEY_DELETE || !first_visible_result ||
      user_typed_text.length() < kMinimumLengthToAutocomplete) {
    // If the suggestion was rejected, no results exist, or current text
    // is too short for a confident autocomplete suggestion.
    return;
  }

  const base::string16& details = first_visible_result->details();
  const base::string16& search_text = first_visible_result->title();
  if (base::StartsWith(details, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // Current text in the search_box matches the first result's url.
    SetAutocompleteText(details);
    return;
  }
  if (base::StartsWith(search_text, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // Current text in the search_box matches the first result's search result
    // text.
    SetAutocompleteText(search_text);
    return;
  }
  // Current text in the search_box does not match the first result's url or
  // search result text.
  ClearAutocompleteText();
}

void SearchBoxView::GetWallpaperProminentColors(
    AppListViewDelegate::GetWallpaperProminentColorsCallback callback) {
  view_delegate_->GetWallpaperProminentColors(std::move(callback));
}

void SearchBoxView::OnWallpaperProminentColorsReceived(
    const std::vector<SkColor>& prominent_colors) {
  if (prominent_colors.empty())
    return;
  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            prominent_colors.size());

  SetSearchBoxColor(
      prominent_colors[static_cast<int>(ColorProfileType::DARK_MUTED)]);
  UpdateSearchIcon();
  close_button()->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseIconSize,
                            search_box_color()));
  search_box()->set_placeholder_text_color(search_box_color());
  UpdateBackgroundColor(search_box::kSearchBoxBackgroundDefault);
  SchedulePaint();
}

void SearchBoxView::AcceptAutocompleteText() {
  if (!ShouldProcessAutocomplete())
    return;

  // Do not trigger another search here in case the user is left clicking to
  // select existing autocomplete text. (This also matches omnibox behavior.)
  DCHECK(HasAutocompleteText());
  search_box()->ClearSelection();
  ResetHighlightRange();
}

bool SearchBoxView::HasAutocompleteText() {
  // If the selected range is non-empty, it will either be suggested by
  // autocomplete or selected by the user. If the recorded autocomplete
  // |highlight_range_| matches the selection range, this text is suggested by
  // autocomplete.
  return search_box()->GetSelectedRange().EqualsIgnoringDirection(
             highlight_range_) &&
         highlight_range_.length() > 0;
}

void SearchBoxView::ClearAutocompleteText() {
  if (!ShouldProcessAutocomplete())
    return;

  // Avoid triggering subsequent query by temporarily setting controller to
  // nullptr.
  search_box()->set_controller(nullptr);
  search_box()->ClearCompositionText();
  search_box()->set_controller(this);
  ResetHighlightRange();
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  // Update autocomplete text highlight range to track user typed text.
  if (ShouldProcessAutocomplete())
    ResetHighlightRange();
  search_box::SearchBoxViewBase::ContentsChanged(sender, new_contents);
  app_list_view_->SetStateFromSearchBoxView(
      IsSearchBoxTrimmedQueryEmpty(), true /*triggered_by_contents_change*/);
}

void SearchBoxView::SetAutocompleteText(
    const base::string16& autocomplete_text) {
  if (!ShouldProcessAutocomplete())
    return;

  const base::string16& current_text = search_box()->text();
  // Currrent text is a prefix of autocomplete text.
  DCHECK(base::StartsWith(autocomplete_text, current_text,
                          base::CompareCase::INSENSITIVE_ASCII));
  // Don't set autocomplete text if it's the same as current search box text.
  if (autocomplete_text.length() == current_text.length())
    return;

  const base::string16& highlighted_text =
      autocomplete_text.substr(highlight_range_.start());

  // Don't set autocomplete text if the highlighted text is the same as before.
  if (highlighted_text == search_box()->GetSelectedText())
    return;

  highlight_range_.set_end(autocomplete_text.length());
  ui::CompositionText composition_text;
  composition_text.text = highlighted_text;
  composition_text.selection = gfx::Range(0, highlighted_text.length());

  // Avoid triggering subsequent query by temporarily setting controller to
  // nullptr.
  search_box()->set_controller(nullptr);
  search_box()->SetCompositionText(composition_text);
  search_box()->set_controller(this);

  // The controller was null briefly, so it was unaware of a highlight change.
  // As a result, we need to manually declare the range to allow for proper
  // selection behavior.
  search_box()->SelectRange(highlight_range_);

  // Send an event to alert ChromeVox that an autocomplete has occurred.
  // The |kValueChanged| type lets ChromeVox know that it should scan
  // |node_data| for "Value".
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void SearchBoxView::UpdateQuery(const base::string16& new_query) {
  search_box()->SetText(new_query);
  ContentsChanged(search_box(), new_query);
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      key_event.key_code() == ui::VKEY_RETURN) {
    if (is_search_box_active()) {
      // Hitting Enter when focus is on search box opens the first result.
      ui::KeyEvent event(key_event);
      views::View* first_result_view =
          contents_view_->search_results_page_view()->first_result_view();
      if (first_result_view)
        first_result_view->OnKeyEvent(&event);
    } else {
      SetSearchBoxActive(true, key_event.type());
    }
    return true;
  }

  // Events occurring over an inactive search box are handled elsewhere, with
  // the exception of left/right arrow key events
  if (!is_search_box_active()) {
    if (IsUnhandledLeftRightKeyEvent(key_event))
      return ProcessLeftRightKeyTraversalForTextfield(search_box(), key_event);
    else
      return false;
  }

  // Record the |last_key_pressed_| for autocomplete.
  if (!search_box()->text().empty() && ShouldProcessAutocomplete())
    last_key_pressed_ = key_event.key_code();

  // Only arrow key events intended for traversal within search results should
  // be handled from here.
  if (!IsUnhandledArrowKeyEvent(key_event))
    return false;

  SearchResultPageView* search_page =
      contents_view_->search_results_page_view();

  // Define forward/backward keys for traversal based on RTL settings
  ui::KeyboardCode forward =
      base::i18n::IsRTL() ? ui::VKEY_LEFT : ui::VKEY_RIGHT;
  ui::KeyboardCode backward =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;

  // Left/Right arrow keys are handled elsewhere, unless the first result is a
  // tile, in which case right will be handled below.
  // The focus traversal in the search box is based around the 'implicit focus'
  // or whichever result is highlighted. As a result, we are trying to move
  // the actual focus based on the position of this highlight.
  // In addition to that, when there are tiles we want to allow a left/right
  // traversal among the tiles. When there are no tiles, left/right should be
  // handled in the ordinary way that a textfield would handle it.
  if (key_event.key_code() == backward ||
      (key_event.key_code() == forward && !search_page->IsFirstResultTile())) {
    return ProcessLeftRightKeyTraversalForTextfield(search_box(), key_event);
  }

  // Right arrow key should not be handled if the cursor is within text.
  if (key_event.key_code() == forward &&
      !LeftRightKeyEventShouldExitText(search_box(), key_event)) {
    return false;
  }

  views::View* result_view = nullptr;

  // The up arrow will loop focus to the last result.
  // The down and right arrows will be treated the same, moving focus along to
  // the 'next' result. If a result is highlighted, we treat that result as
  // though it already had focus.
  if (key_event.key_code() == ui::VKEY_UP) {
    result_view = search_page->GetLastFocusableView();
  } else if (search_page->IsFirstResultHighlighted()) {
    result_view = search_page->GetFirstFocusableView();

    // Give the parent container a chance to handle the event. This lets the
    // down arrow escape the tile result container.
    if (!result_view->parent()->OnKeyPressed(key_event)) {
      // If the parent container doesn't handle |key_event|, get the next
      // focusable view.
      result_view = result_view->GetFocusManager()->GetNextFocusableView(
          result_view, result_view->GetWidget(), false, false);
    } else {
      // Return early if the parent container handled the event.
      return true;
    }
  } else {
    result_view = search_page->GetFirstFocusableView();
  }

  if (result_view)
    result_view->RequestFocus();

  return true;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    return app_list_view_->HandleScroll(
        (&mouse_event)->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL);
  }
  if (mouse_event.type() == ui::ET_MOUSE_PRESSED && HasAutocompleteText())
    AcceptAutocompleteText();
  return search_box::SearchBoxViewBase::HandleMouseEvent(sender, mouse_event);
}

bool SearchBoxView::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  if (gesture_event.type() == ui::ET_GESTURE_TAP && HasAutocompleteText())
    AcceptAutocompleteText();
  return search_box::SearchBoxViewBase::HandleGestureEvent(sender,
                                                           gesture_event);
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (close_button() && sender == close_button()) {
    SetSearchBoxActive(false, ui::ET_UNKNOWN);
  }
  search_box::SearchBoxViewBase::ButtonPressed(sender, event);
}

void SearchBoxView::HintTextChanged() {
  const app_list::SearchBoxModel* search_box_model =
      search_model_->search_box();
  search_box()->set_placeholder_text(search_box_model->hint_text());
  search_box()->SetAccessibleName(search_box_model->accessible_name());
  SchedulePaint();
}

void SearchBoxView::SelectionModelChanged() {
  search_box()->SelectSelectionModel(
      search_model_->search_box()->selection_model());
}

void SearchBoxView::Update() {
  search_box()->SetText(search_model_->search_box()->text());
  UpdateButtonsVisisbility();
  NotifyQueryChanged();
}

void SearchBoxView::SearchEngineChanged() {
  UpdateSearchIcon();
}

void SearchBoxView::ShowAssistantChanged() {
  if (search_model_) {
    SetShowAssistantButton(
        search_model_->search_box()->show_assistant_button());
  }
}

bool SearchBoxView::ShouldProcessAutocomplete() {
  // IME sets composition text while the user is typing, so avoid handle
  // autocomplete in this case to avoid conflicts.
  return is_app_list_search_autocomplete_enabled_ &&
         !(search_box()->IsIMEComposing() && highlight_range_.is_empty());
}

void SearchBoxView::ResetHighlightRange() {
  DCHECK(ShouldProcessAutocomplete());
  const uint32_t text_length = search_box()->text().length();
  highlight_range_.set_start(text_length);
  highlight_range_.set_end(text_length);
}

void SearchBoxView::SetupAssistantButton() {
  if (search_model_ && !search_model_->search_box()->show_assistant_button()) {
    return;
  }

  views::ImageButton* assistant = assistant_button();
  assistant->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(app_list_features::IsEmbeddedAssistantUIEnabled()
                                ? ash::kAssistantMicIcon
                                : ash::kAssistantIcon,
                            kAssistantIconSize, search_box_color()));
  assistant->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_LIST_START_ASSISTANT));
}

}  // namespace app_list
