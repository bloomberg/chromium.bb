// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_view.h"

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bar_control_button.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace {

// The margins around the UI controls, derived from assets and design specs.
const int kMarginLeftOfCloseButton = 3;
const int kMarginRightOfCloseButton = 7;
const int kMarginLeftOfMatchCountLabel = 3;
const int kMarginRightOfMatchCountLabel = 1;
const int kMarginLeftOfFindTextfield = 12;
const int kMarginVerticalFindTextfield = 6;

// Constants for the MD layout, all in dp.
// The horizontal and vertical insets for the bar.
const int kInteriorPadding = 8;
// Default spacing between child views.
const int kInterChildSpacing = 4;
// Additional spacing around the separator.
const int kSeparatorLeftSpacing = 12 - kInterChildSpacing;
const int kSeparatorRightSpacing = 8 - kInterChildSpacing;

// The margins around the match count label (We add extra space so that the
// background highlight extends beyond just the text).
const int kMatchCountExtraWidth = 9;

// Minimum width for the match count label.
const int kMatchCountMinWidth = 30;

// The text color for the match count label.
const SkColor kTextColorMatchCount = SkColorSetRGB(178, 178, 178);

// The text color for the match count label when no matches are found.
const SkColor kTextColorNoMatch = SK_ColorBLACK;

// The background color of the match count label when results are found.
const SkColor kBackgroundColorMatch = SkColorSetARGB(0, 255, 255, 255);

// The background color of the match count label when no results are found.
const SkColor kBackgroundColorNoMatch = SkColorSetRGB(255, 102, 102);

// The default number of average characters that the text box will be. This
// number brings the width on a "regular fonts" system to about 300px.
const int kDefaultCharWidth = 43;

// The match count label is like a normal label, but can process events (which
// makes it easier to forward events to the text input --- see
// FindBarView::TargetForRect).
class MatchCountLabel : public views::Label {
 public:
  MatchCountLabel() {}
  ~MatchCountLabel() override {}

  // views::Label overrides:
  bool CanProcessEventsWithinSubtree() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MatchCountLabel);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarHost* host)
    : DropdownBarView(host),
      find_text_(nullptr),
      match_count_text_(nullptr),
      focus_forwarder_view_(nullptr),
      separator_(nullptr),
      find_previous_button_(nullptr),
      find_next_button_(nullptr),
      close_button_(nullptr) {
  find_text_ = new views::Textfield;
  find_text_->set_id(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  find_text_->set_default_width_in_chars(kDefaultCharWidth);
  find_text_->set_controller(this);
  find_text_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND));
  find_text_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  AddChildView(find_text_);

  if (ui::MaterialDesignController::IsModeMaterial()) {
    BarControlButton* find_previous = new BarControlButton(this);
    find_previous->SetIcon(
        gfx::VectorIconId::FIND_PREV,
        base::Bind(&FindBarView::GetTextColorForIcon, base::Unretained(this)));
    BarControlButton* find_next = new BarControlButton(this);
    find_next->SetIcon(
        gfx::VectorIconId::FIND_NEXT,
        base::Bind(&FindBarView::GetTextColorForIcon, base::Unretained(this)));
    BarControlButton* close = new BarControlButton(this);
    close->SetIcon(
        gfx::VectorIconId::BAR_CLOSE,
        base::Bind(&FindBarView::GetTextColorForIcon, base::Unretained(this)));

    find_previous_button_ = find_previous;
    find_next_button_ = find_next;
    close_button_ = close;
  } else {
    find_previous_button_ = new views::ImageButton(this);
    find_next_button_ = new views::ImageButton(this);
    close_button_ = new views::ImageButton(this);
  }

  find_previous_button_->set_id(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);
  find_previous_button_->SetFocusable(true);
  find_previous_button_->set_request_focus_on_press(false);
  find_previous_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP));
  find_previous_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_PREVIOUS));
  AddChildView(find_previous_button_);

  find_next_button_->set_id(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  find_next_button_->SetFocusable(true);
  find_next_button_->set_request_focus_on_press(false);
  find_next_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_NEXT_TOOLTIP));
  find_next_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEXT));
  AddChildView(find_next_button_);

  close_button_->set_id(VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON);
  close_button_->SetFocusable(true);
  close_button_->set_request_focus_on_press(false);
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);

  // Create a focus forwarder view which sends focus to find_text_.
  focus_forwarder_view_ = new FocusForwarderView(find_text_);
  AddChildView(focus_forwarder_view_);

  EnableCanvasFlippingForRTLUI(true);

  if (ui::MaterialDesignController::IsModeMaterial())
    InitViewsForMaterial();
  else
    InitViewsForNonMaterial();
}

FindBarView::~FindBarView() {
}

void FindBarView::SetFindTextAndSelectedRange(
    const base::string16& find_text,
    const gfx::Range& selected_range) {
  find_text_->SetText(find_text);
  find_text_->SelectRange(selected_range);
}

base::string16 FindBarView::GetFindText() const {
  return find_text_->text();
}

gfx::Range FindBarView::GetSelectedRange() const {
  return find_text_->GetSelectedRange();
}

base::string16 FindBarView::GetFindSelectedText() const {
  return find_text_->GetSelectedText();
}

base::string16 FindBarView::GetMatchCountText() const {
  return match_count_text_->text();
}

void FindBarView::UpdateForResult(const FindNotificationDetails& result,
                                  const base::string16& find_text) {
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // http://crbug.com/34970: some IMEs get confused if we change the text
  // composed by them. To avoid this problem, we should check the IME status and
  // update the text only when the IME is not composing text.
  if (find_text_->text() != find_text && !find_text_->IsIMEComposing()) {
    find_text_->SetText(find_text);
    find_text_->SelectAll(true);
  }

  if (find_text.empty() || !have_valid_range) {
    // If there was no text entered, we don't show anything in the result count
    // area.
    ClearMatchCount();
    return;
  }

  match_count_text_->SetText(l10n_util::GetStringFUTF16(IDS_FIND_IN_PAGE_COUNT,
      base::IntToString16(result.active_match_ordinal()),
      base::IntToString16(result.number_of_matches())));

  UpdateMatchCountAppearance(result.number_of_matches() == 0 &&
                             result.final_update());

  // The match_count label may have increased/decreased in size so we need to
  // do a layout and repaint the dialog so that the find text field doesn't
  // partially overlap the match-count label when it increases on no matches.
  Layout();
  SchedulePaint();
}

void FindBarView::ClearMatchCount() {
  match_count_text_->SetText(base::string16());
  UpdateMatchCountAppearance(false);
  Layout();
  SchedulePaint();
}

void FindBarView::SetFocusAndSelection(bool select_all) {
  find_text_->RequestFocus();
  GetWidget()->GetInputMethod()->ShowImeIfNeeded();
  if (select_all && !find_text_->text().empty())
    find_text_->SelectAll(true);
}

///////////////////////////////////////////////////////////////////////////////
// FindBarView, views::View overrides:

void FindBarView::OnPaint(gfx::Canvas* canvas) {
  if (ui::MaterialDesignController::IsModeMaterial())
    return views::View::OnPaint(canvas);

  // Paint drop down bar border and background.
  DropdownBarView::OnPaint(canvas);

  // Paint the background and border for the textfield.
  const int find_text_x = kMarginLeftOfFindTextfield / 2;
  const gfx::Rect text_bounds(find_text_x, find_next_button_->y(),
                              find_next_button_->bounds().right() - find_text_x,
                              find_next_button_->height());
  const int kBorderCornerRadius = 2;
  gfx::Rect background_bounds = text_bounds;
  background_bounds.Inset(kBorderCornerRadius, kBorderCornerRadius);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(find_text_->GetBackgroundColor());
  canvas->DrawRoundRect(background_bounds, kBorderCornerRadius, paint);
  canvas->Save();
  canvas->ClipRect(gfx::Rect(0, 0, find_previous_button_->x(), height()));
  views::Painter::PaintPainterAt(canvas, find_text_border_.get(), text_bounds);
  canvas->Restore();

  // Draw the background of the match text. We want to make sure the red
  // "no-match" background almost completely fills up the amount of vertical
  // space within the text box. We therefore fix the size relative to the button
  // heights. We use the FindPrev button, which has a 1px outer whitespace
  // margin, 1px border and we want to appear 1px below the border line so we
  // subtract 3 for top and 3 for bottom.
  gfx::Rect match_count_background_bounds(match_count_text_->bounds());
  match_count_background_bounds.set_height(
      find_previous_button_->height() - 6);  // Subtract 3px x 2.
  match_count_background_bounds.set_y(
      (height() - match_count_background_bounds.height()) / 2);
  canvas->FillRect(match_count_background_bounds,
                   match_count_text_->background_color());
}

void FindBarView::OnPaintBackground(gfx::Canvas* canvas) {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return views::View::OnPaintBackground(canvas);

  // Draw within the lines.
  canvas->Save();
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(border()->GetInsets());
  canvas->ClipRect(bounds);
  views::View::OnPaintBackground(canvas);
  canvas->Restore();
}

void FindBarView::Layout() {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    views::View::Layout();
  } else {
    int panel_width = GetPreferredSize().width();

    // Stay within view bounds.
    int view_width = width();
    if (view_width && view_width < panel_width)
      panel_width = view_width;

    // Set the color.
    OnThemeChanged();

    // First we position the close button on the far right.
    close_button_->SizeToPreferredSize();
    close_button_->SetPosition(gfx::Point(
        panel_width - close_button_->width() - kMarginRightOfCloseButton,
        (height() - close_button_->height()) / 2));

    // Then, the next button to the left of the close button.
    find_next_button_->SizeToPreferredSize();
    find_next_button_->SetPosition(
        gfx::Point(close_button_->x() - find_next_button_->width() -
                       kMarginLeftOfCloseButton,
                   (height() - find_next_button_->height()) / 2));

    // Then, the previous button to the left of the next button.
    find_previous_button_->SizeToPreferredSize();
    find_previous_button_->SetPosition(gfx::Point(
        find_next_button_->x() - find_previous_button_->width(),
        (height() - find_previous_button_->height()) / 2));

    // Then the label showing the match count number.
    gfx::Size sz = match_count_text_->GetPreferredSize();
    // We extend the label bounds a bit to give the background highlighting a
    // bit of breathing room (margins around the text).
    sz.Enlarge(kMatchCountExtraWidth, 0);
    sz.SetToMax(gfx::Size(kMatchCountMinWidth, 0));
    const int match_count_x =
        find_previous_button_->x() - kMarginRightOfMatchCountLabel - sz.width();
    const int find_text_y = kMarginVerticalFindTextfield;
    const gfx::Insets find_text_insets(find_text_->GetInsets());
    match_count_text_->SetBounds(match_count_x,
                                 find_text_y - find_text_insets.top() +
                                     find_text_->GetBaseline() -
                                     match_count_text_->GetBaseline(),
                                 sz.width(), sz.height());

    // Fill the remaining width and available height with the textfield.
    const int left_margin =
        kMarginLeftOfFindTextfield - find_text_insets.left();
    const int find_text_width = std::max(0, match_count_x - left_margin -
        kMarginLeftOfMatchCountLabel + find_text_insets.right());
    find_text_->SetBounds(left_margin, find_text_y, find_text_width,
                          height() - 2 * kMarginVerticalFindTextfield);
  }

  // The focus forwarder view is a hidden view that should cover the area
  // between the find text box and the find button so that when the user clicks
  // in that area we focus on the find text box.
  const int find_text_edge = find_text_->x() + find_text_->width();
  focus_forwarder_view_->SetBounds(
      find_text_edge, find_previous_button_->y(),
      find_previous_button_->x() - find_text_edge,
      find_previous_button_->height());
}

gfx::Size FindBarView::GetPreferredSize() const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // The entire bar is sized to a specific number of characters set on
    // |find_text_|.
    gfx::Size size = views::View::GetPreferredSize();
    size.set_width(find_text_->GetPreferredSize().width());
    return size;
  }

  gfx::Size prefsize = find_text_->GetPreferredSize();
  prefsize.set_height(preferred_height_);

  // Add up all the preferred sizes and margins of the rest of the controls.
  prefsize.Enlarge(kMarginLeftOfCloseButton + kMarginRightOfCloseButton +
                       kMarginLeftOfFindTextfield -
                       find_text_->GetInsets().width(),
                   0);
  prefsize.Enlarge(find_previous_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(find_next_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(close_button_->GetPreferredSize().width(), 0);
  return prefsize;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::ButtonListener implementation:

void FindBarView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (sender->id()) {
    case VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON:
    case VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON:
      if (!find_text_->text().empty()) {
        FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(
            find_bar_host()->GetFindBarController()->web_contents());
        find_tab_helper->StartFinding(
            find_text_->text(),
            sender->id() == VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON,
            false);  // Not case sensitive.
      }

      if (event.IsMouseEvent()) {
        // Move focus to the find textfield.
        find_text_->RequestFocus();
      }
      break;
    case VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON:
      find_bar_host()->GetFindBarController()->EndFindSession(
          FindBarController::kKeepSelectionOnPage,
          FindBarController::kKeepResultsInFindBox);
      break;
    default:
      NOTREACHED() << "Unknown button";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::TextfieldController implementation:

bool FindBarView::HandleKeyEvent(views::Textfield* sender,
                                 const ui::KeyEvent& key_event) {
  // If the dialog is not visible, there is no reason to process keyboard input.
  if (!host()->IsVisible())
    return false;

  if (find_bar_host()->MaybeForwardKeyEventToWebpage(key_event))
    return true;  // Handled, we are done!

  if (key_event.key_code() == ui::VKEY_RETURN) {
    // Pressing Return/Enter starts the search (unless text box is empty).
    base::string16 find_string = find_text_->text();
    if (!find_string.empty()) {
      FindBarController* controller = find_bar_host()->GetFindBarController();
      FindTabHelper* find_tab_helper =
          FindTabHelper::FromWebContents(controller->web_contents());
      // Search forwards for enter, backwards for shift-enter.
      find_tab_helper->StartFinding(find_string,
                                    !key_event.IsShiftDown(),
                                    false);  // Not case sensitive.
    }
    return true;
  }

  return false;
}

void FindBarView::OnAfterUserAction(views::Textfield* sender) {
  // The composition text wouldn't be what the user is really looking for.
  // We delay the search until the user commits the composition text.
  if (!sender->IsIMEComposing() && sender->text() != last_searched_text_)
    Find(sender->text());
}

void FindBarView::OnAfterPaste() {
  // Clear the last search text so we always search for the user input after
  // a paste operation, even if the pasted text is the same as before.
  // See http://crbug.com/79002
  last_searched_text_.clear();
}

views::View* FindBarView::TargetForRect(View* root, const gfx::Rect& rect) {
  DCHECK_EQ(match_count_text_, root);
  return find_text_;
}

void FindBarView::InitViewsForNonMaterial() {
  match_count_text_ = new views::Label();
  AddChildView(match_count_text_);

  // The find bar textfield has a background image instead of a border.
  find_text_->SetBorder(views::Border::NullBorder());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  find_previous_button_->SetImage(views::CustomButton::STATE_NORMAL,
                                  rb.GetImageSkiaNamed(IDR_FINDINPAGE_PREV));
  find_previous_button_->SetImage(views::CustomButton::STATE_HOVERED,
                                  rb.GetImageSkiaNamed(IDR_FINDINPAGE_PREV_H));
  find_previous_button_->SetImage(views::CustomButton::STATE_PRESSED,
                                  rb.GetImageSkiaNamed(IDR_FINDINPAGE_PREV_P));
  find_previous_button_->SetImage(views::CustomButton::STATE_DISABLED,
                                  rb.GetImageSkiaNamed(IDR_FINDINPAGE_PREV_D));

  find_next_button_->SetImage(views::CustomButton::STATE_NORMAL,
                              rb.GetImageSkiaNamed(IDR_FINDINPAGE_NEXT));
  find_next_button_->SetImage(views::CustomButton::STATE_HOVERED,
                              rb.GetImageSkiaNamed(IDR_FINDINPAGE_NEXT_H));
  find_next_button_->SetImage(views::CustomButton::STATE_PRESSED,
                              rb.GetImageSkiaNamed(IDR_FINDINPAGE_NEXT_P));
  find_next_button_->SetImage(views::CustomButton::STATE_DISABLED,
                              rb.GetImageSkiaNamed(IDR_FINDINPAGE_NEXT_D));

  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1_P));

  SetBackground(rb.GetImageSkiaNamed(IDR_FIND_DLG_LEFT_BACKGROUND),
                rb.GetImageSkiaNamed(IDR_FIND_DLG_RIGHT_BACKGROUND));
  SetBorderFromIds(IDR_FIND_DIALOG_LEFT, IDR_FIND_DIALOG_MIDDLE,
                   IDR_FIND_DIALOG_RIGHT);

  preferred_height_ = rb.GetImageSkiaNamed(IDR_FIND_DIALOG_MIDDLE)->height();

  static const int kImages[] = IMAGE_GRID(IDR_TEXTFIELD);
  find_text_border_.reset(views::Painter::CreateImageGridPainter(kImages));
}

void FindBarView::InitViewsForMaterial() {
  // The background color is not used since there's no arrow.
  SetBorder(base::WrapUnique(new views::BubbleBorder(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW,
      SK_ColorGREEN)));

  match_count_text_ = new MatchCountLabel();
  match_count_text_->SetEventTargeter(
      base::WrapUnique(new views::ViewTargeter(this)));
  AddChildViewAt(match_count_text_, 1);

  separator_ = new views::Separator(views::Separator::VERTICAL);
  separator_->SetBorder(views::Border::CreateEmptyBorder(
      0, kSeparatorLeftSpacing, 0, kSeparatorRightSpacing));
  AddChildViewAt(separator_, 2);

  find_text_->SetBorder(views::Border::NullBorder());

  views::BoxLayout* manager =
      new views::BoxLayout(views::BoxLayout::kHorizontal, kInteriorPadding,
                           kInteriorPadding, kInterChildSpacing);
  SetLayoutManager(manager);
  manager->SetFlexForView(find_text_, 1);
}

void FindBarView::Find(const base::string16& search_text) {
  FindBarController* controller = find_bar_host()->GetFindBarController();
  DCHECK(controller);
  content::WebContents* web_contents = controller->web_contents();
  // We must guard against a NULL web_contents, which can happen if the text
  // in the Find box is changed right after the tab is destroyed. Otherwise, it
  // can lead to crashes, as exposed by automation testing in issue 8048.
  if (!web_contents)
    return;
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);

  last_searched_text_ = search_text;

  // When the user changes something in the text box we check the contents and
  // if the textbox contains something we set it as the new search string and
  // initiate search (even though old searches might be in progress).
  if (!search_text.empty()) {
    // The last two params here are forward (true) and case sensitive (false).
    find_tab_helper->StartFinding(search_text, true, false);
  } else {
    find_tab_helper->StopFinding(FindBarController::kClearSelectionOnPage);
    UpdateForResult(find_tab_helper->find_result(), base::string16());
    find_bar_host()->MoveWindowIfNecessary(gfx::Rect());

    // Clearing the text box should clear the prepopulate state so that when
    // we close and reopen the Find box it doesn't show the search we just
    // deleted. We can't do this on ChromeOS yet because we get ContentsChanged
    // sent for a lot more things than just the user nulling out the search
    // terms. See http://crbug.com/45372.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    FindBarState* find_bar_state = FindBarStateFactory::GetForProfile(profile);
    find_bar_state->set_last_prepopulate_text(base::string16());
  }
}

void FindBarView::UpdateMatchCountAppearance(bool no_match) {
  bool enable_buttons = !match_count_text_->text().empty() && !no_match;
  find_previous_button_->SetEnabled(enable_buttons);
  find_next_button_->SetEnabled(enable_buttons);

  if (ui::MaterialDesignController::IsModeMaterial())
    return;

  if (no_match) {
    match_count_text_->SetBackgroundColor(kBackgroundColorNoMatch);
    match_count_text_->SetEnabledColor(kTextColorNoMatch);
  } else {
    match_count_text_->SetBackgroundColor(kBackgroundColorMatch);
    match_count_text_->SetEnabledColor(kTextColorMatchCount);
  }
}

bool FindBarView::FocusForwarderView::OnMousePressed(
    const ui::MouseEvent& event) {
  if (view_to_focus_on_mousedown_)
    view_to_focus_on_mousedown_->RequestFocus();
  return true;
}

FindBarHost* FindBarView::find_bar_host() const {
  return static_cast<FindBarHost*>(host());
}

const char* FindBarView::GetClassName() const {
  return "FindBarView";
}

void FindBarView::OnThemeChanged() {
  if (ui::MaterialDesignController::IsModeMaterial())
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (GetThemeProvider()) {
    close_button_->SetBackground(
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_TAB_TEXT),
        rb.GetImageSkiaNamed(IDR_CLOSE_1),
        rb.GetImageSkiaNamed(IDR_CLOSE_1_MASK));
  }
}

void FindBarView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return;

  SkColor bg_color = theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  set_background(views::Background::CreateSolidBackground(bg_color));
  match_count_text_->SetBackgroundColor(bg_color);

  SkColor text_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TextfieldDefaultColor);
  match_count_text_->SetEnabledColor(SkColorSetA(text_color, 0x69));
  separator_->SetColor(SkColorSetA(text_color, 0x26));
}

SkColor FindBarView::GetTextColorForIcon() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}
