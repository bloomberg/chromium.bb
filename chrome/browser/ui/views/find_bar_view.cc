// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_view.h"

#include <algorithm>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"

// The amount of whitespace to have before the find button.
static const int kWhiteSpaceAfterMatchCountLabel = 1;

// The margins around the search field and the close button.
static const int kMarginLeftOfCloseButton = 3;
static const int kMarginRightOfCloseButton = 7;
static const int kMarginLeftOfFindTextfield = 12;

// The margins around the match count label (We add extra space so that the
// background highlight extends beyond just the text).
static const int kMatchCountExtraWidth = 9;

// Minimum width for the match count label.
static const int kMatchCountMinWidth = 30;

// The text color for the match count label.
static const SkColor kTextColorMatchCount = SkColorSetRGB(178, 178, 178);

// The text color for the match count label when no matches are found.
static const SkColor kTextColorNoMatch = SK_ColorBLACK;

// The background color of the match count label when results are found.
static const SkColor kBackgroundColorMatch = SK_ColorWHITE;

// The background color of the match count label when no results are found.
static const SkColor kBackgroundColorNoMatch = SkColorSetRGB(255, 102, 102);

// The default number of average characters that the text box will be. This
// number brings the width on a "regular fonts" system to about 300px.
static const int kDefaultCharWidth = 43;

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarHost* host)
    : DropdownBarView(host),
      find_text_(NULL),
      match_count_text_(NULL),
      focus_forwarder_view_(NULL),
      find_previous_button_(NULL),
      find_next_button_(NULL),
      close_button_(NULL),
      background_(NULL),
      background_left_(NULL) {
  set_id(VIEW_ID_FIND_IN_PAGE);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  find_text_ = new SearchTextfieldView();
  find_text_->set_id(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  find_text_->SetFont(rb.GetFont(ResourceBundle::BaseFont));
  find_text_->set_default_width_in_chars(kDefaultCharWidth);
  find_text_->SetController(this);
  find_text_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND));
  AddChildView(find_text_);

  match_count_text_ = new views::Label();
  match_count_text_->SetFont(rb.GetFont(ResourceBundle::BaseFont));
  match_count_text_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  AddChildView(match_count_text_);

  // Create a focus forwarder view which sends focus to find_text_.
  focus_forwarder_view_ = new FocusForwarderView(find_text_);
  AddChildView(focus_forwarder_view_);

  find_previous_button_ = new views::ImageButton(this);
  find_previous_button_->set_tag(FIND_PREVIOUS_TAG);
  find_previous_button_->set_focusable(true);
  find_previous_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV));
  find_previous_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV_H));
  find_previous_button_->SetImage(views::CustomButton::BS_DISABLED,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV_P));
  find_previous_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP));
  find_previous_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_PREVIOUS));
  AddChildView(find_previous_button_);

  find_next_button_ = new views::ImageButton(this);
  find_next_button_->set_tag(FIND_NEXT_TAG);
  find_next_button_->set_focusable(true);
  find_next_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT));
  find_next_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT_H));
  find_next_button_->SetImage(views::CustomButton::BS_DISABLED,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT_P));
  find_next_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_NEXT_TOOLTIP));
  find_next_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEXT));
  AddChildView(find_next_button_);

  close_button_ = new views::ImageButton(this);
  close_button_->set_tag(CLOSE_TAG);
  close_button_->set_focusable(true);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  SetDialogBorderBitmaps(rb.GetBitmapNamed(IDR_FIND_DIALOG_LEFT),
                         rb.GetBitmapNamed(IDR_FIND_DIALOG_MIDDLE),
                         rb.GetBitmapNamed(IDR_FIND_DIALOG_RIGHT));

  // Background images for the Find edit box.
  background_ = rb.GetBitmapNamed(IDR_FIND_BOX_BACKGROUND);
  background_left_ = rb.GetBitmapNamed(IDR_FIND_BOX_BACKGROUND_LEFT);
}

FindBarView::~FindBarView() {
}

void FindBarView::SetFindText(const string16& find_text) {
  find_text_->SetText(find_text);
}

string16 FindBarView::GetFindText() const {
  return find_text_->text();
}

string16 FindBarView::GetFindSelectedText() const {
  return find_text_->GetSelectedText();
}

string16 FindBarView::GetMatchCountText() const {
  return match_count_text_->GetText();
}

void FindBarView::UpdateForResult(const FindNotificationDetails& result,
                                  const string16& find_text) {
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // http://crbug.com/34970: some IMEs get confused if we change the text
  // composed by them. To avoid this problem, we should check the IME status and
  // update the text only when the IME is not composing text.
  if (find_text_->text() != find_text && !find_text_->IsIMEComposing()) {
    find_text_->SetText(find_text);
    find_text_->SelectAll();
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
  match_count_text_->SetText(string16());
  UpdateMatchCountAppearance(false);
  Layout();
  SchedulePaint();
}

void FindBarView::SetFocusAndSelection(bool select_all) {
  find_text_->RequestFocus();
  if (select_all && !find_text_->text().empty())
    find_text_->SelectAll();
}

///////////////////////////////////////////////////////////////////////////////
// FindBarView, views::View overrides:

void FindBarView::OnPaint(gfx::Canvas* canvas) {
  SkPaint paint;

  gfx::Rect bounds = PaintOffsetToolbarBackground(canvas);

  // Now flip the canvas for the rest of the graphics if in RTL mode.
  canvas->Save();
  if (base::i18n::IsRTL()) {
    canvas->Translate(gfx::Point(width(), 0));
    canvas->Scale(-1, 1);
  }

  PaintDialogBorder(canvas, bounds);

  // Then we draw the background image for the Find Textfield. We start by
  // calculating the position of background images for the Find text box.
  int find_text_x = find_text_->x();
  gfx::Point back_button_origin = find_previous_button_->bounds().origin();

  // Draw the image to the left that creates a curved left edge for the box.
  canvas->TileImageInt(*background_left_,
      find_text_x - background_left_->width(), back_button_origin.y(),
      background_left_->width(), background_left_->height());

  // Draw the top and bottom border for whole text box (encompasses both the
  // find_text_ edit box and the match_count_text_ label).
  canvas->TileImageInt(*background_, find_text_x, back_button_origin.y(),
                       back_button_origin.x() - find_text_x,
                       background_->height());

  PaintAnimatingEdges(canvas, bounds);

  canvas->Restore();
}

void FindBarView::Layout() {
  int panel_width = GetPreferredSize().width();

  // Stay within view bounds.
  int view_width = width();
  if (view_width && view_width < panel_width)
    panel_width = view_width;

  // First we draw the close button on the far right.
  gfx::Size sz = close_button_->GetPreferredSize();
  close_button_->SetBounds(panel_width - sz.width() -
                               kMarginRightOfCloseButton,
                           (height() - sz.height()) / 2,
                           sz.width(),
                           sz.height());
  // Set the color.
  OnThemeChanged();

  // Next, the FindNext button to the left the close button.
  sz = find_next_button_->GetPreferredSize();
  find_next_button_->SetBounds(close_button_->x() -
                                   find_next_button_->width() -
                                   kMarginLeftOfCloseButton,
                               (height() - sz.height()) / 2,
                                sz.width(),
                                sz.height());

  // Then, the FindPrevious button to the left the FindNext button.
  sz = find_previous_button_->GetPreferredSize();
  find_previous_button_->SetBounds(find_next_button_->x() -
                                       find_previous_button_->width(),
                                   (height() - sz.height()) / 2,
                                   sz.width(),
                                   sz.height());

  // Then the label showing the match count number.
  sz = match_count_text_->GetPreferredSize();
  // We want to make sure the red "no-match" background almost completely fills
  // up the amount of vertical space within the text box. We therefore fix the
  // size relative to the button heights. We use the FindPrev button, which has
  // a 1px outer whitespace margin, 1px border and we want to appear 1px below
  // the border line so we subtract 3 for top and 3 for bottom.
  sz.set_height(find_previous_button_->height() - 6);  // Subtract 3px x 2.

  // We extend the label bounds a bit to give the background highlighting a bit
  // of breathing room (margins around the text).
  sz.Enlarge(kMatchCountExtraWidth, 0);
  sz.set_width(std::max(kMatchCountMinWidth, static_cast<int>(sz.width())));
  int match_count_x = find_previous_button_->x() -
                      kWhiteSpaceAfterMatchCountLabel -
                      sz.width();
  match_count_text_->SetBounds(match_count_x,
                               (height() - sz.height()) / 2,
                               sz.width(),
                               sz.height());

  // And whatever space is left in between, gets filled up by the find edit box.
  sz = find_text_->GetPreferredSize();
  sz.set_width(std::max(0, match_count_x - kMarginLeftOfFindTextfield));
  int find_text_x = std::max(0, match_count_x - sz.width());
  find_text_->SetBounds(find_text_x,
                        (height() - sz.height()) / 2 + 1,
                        sz.width(),
                        sz.height());

  // The focus forwarder view is a hidden view that should cover the area
  // between the find text box and the find button so that when the user clicks
  // in that area we focus on the find text box.
  int find_text_edge = find_text_->x() + find_text_->width();
  focus_forwarder_view_->SetBounds(find_text_edge,
                                   find_previous_button_->y(),
                                   find_previous_button_->x() -
                                       find_text_edge,
                                   find_previous_button_->height());
}

void FindBarView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    find_text_->SetHorizontalMargins(3, 3);  // Left and Right margins.
    find_text_->SetVerticalMargins(0, 0);  // Top and bottom margins.
    find_text_->RemoveBorder();  // We draw our own border (a background image).
  }
}

gfx::Size FindBarView::GetPreferredSize() {
  gfx::Size prefsize = find_text_->GetPreferredSize();
  prefsize.set_height(dialog_middle_->height());

  // Add up all the preferred sizes and margins of the rest of the controls.
  prefsize.Enlarge(kMarginLeftOfCloseButton + kMarginRightOfCloseButton +
                       kMarginLeftOfFindTextfield,
                   0);
  prefsize.Enlarge(find_previous_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(find_next_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(close_button_->GetPreferredSize().width(), 0);
  return prefsize;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::ButtonListener implementation:

void FindBarView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  switch (sender->tag()) {
    case FIND_PREVIOUS_TAG:
    case FIND_NEXT_TAG:
      if (!find_text_->text().empty()) {
        find_bar_host()->GetFindBarController()->tab_contents()->
            find_tab_helper()->StartFinding(find_text_->text(),
                                            sender->tag() == FIND_NEXT_TAG,
                                            false);  // Not case sensitive.
      }
      if (event.IsMouseEvent()) {
        // If mouse event, we move the focus back to the text-field, so that the
        // user doesn't have to click on the text field to change the search. We
        // don't want to do this for keyboard clicks on the button, since the
        // user is more likely to press FindNext again than change the search
        // query.
        find_text_->RequestFocus();
      }
      break;
    case CLOSE_TAG:
      find_bar_host()->GetFindBarController()->EndFindSession(
          FindBarController::kKeepSelection);
      break;
    default:
      NOTREACHED() << L"Unknown button";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::TextfieldController implementation:

void FindBarView::ContentsChanged(views::Textfield* sender,
                                  const string16& new_contents) {
  FindBarController* controller = find_bar_host()->GetFindBarController();
  DCHECK(controller);
  // We must guard against a NULL tab_contents, which can happen if the text
  // in the Find box is changed right after the tab is destroyed. Otherwise, it
  // can lead to crashes, as exposed by automation testing in issue 8048.
  if (!controller->tab_contents())
    return;
  FindTabHelper* find_tab_helper =
      controller->tab_contents()->find_tab_helper();

  // When the user changes something in the text box we check the contents and
  // if the textbox contains something we set it as the new search string and
  // initiate search (even though old searches might be in progress).
  if (!new_contents.empty()) {
    // The last two params here are forward (true) and case sensitive (false).
    find_tab_helper->StartFinding(new_contents, true, false);
  } else {
    find_tab_helper->StopFinding(FindBarController::kClearSelection);
    UpdateForResult(find_tab_helper->find_result(), string16());

    // Clearing the text box should clear the prepopulate state so that when
    // we close and reopen the Find box it doesn't show the search we just
    // deleted. We can't do this on ChromeOS yet because we get ContentsChanged
    // sent for a lot more things than just the user nulling out the search
    // terms. See http://crbug.com/45372.
    FindBarState* find_bar_state =
        controller->tab_contents()->profile()->GetFindBarState();
    find_bar_state->set_last_prepopulate_text(string16());
  }
}

bool FindBarView::HandleKeyEvent(views::Textfield* sender,
                                 const views::KeyEvent& key_event) {
  // If the dialog is not visible, there is no reason to process keyboard input.
  if (!host()->IsVisible())
    return false;

  if (find_bar_host()->MaybeForwardKeyEventToWebpage(key_event))
    return true;  // Handled, we are done!

  if (key_event.key_code() == ui::VKEY_RETURN) {
    // Pressing Return/Enter starts the search (unless text box is empty).
    string16 find_string = find_text_->text();
    if (!find_string.empty()) {
      FindBarController* controller = find_bar_host()->GetFindBarController();
      FindTabHelper* find_tab_helper =
          controller->tab_contents()->find_tab_helper();
      // Search forwards for enter, backwards for shift-enter.
      find_tab_helper->StartFinding(find_string,
                                    !key_event.IsShiftDown(),
                                    false);  // Not case sensitive.
    }
  }

  return false;
}

void FindBarView::UpdateMatchCountAppearance(bool no_match) {
  if (no_match) {
    match_count_text_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColorNoMatch));
    match_count_text_->SetEnabledColor(kTextColorNoMatch);
  } else {
    match_count_text_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColorMatch));
    match_count_text_->SetEnabledColor(kTextColorMatchCount);
  }
  match_count_text_->SetBackgroundColor(
      match_count_text_->background()->get_color());
}

bool FindBarView::FocusForwarderView::OnMousePressed(
    const views::MouseEvent& event) {
  if (view_to_focus_on_mousedown_) {
    view_to_focus_on_mousedown_->ClearSelection();
    view_to_focus_on_mousedown_->RequestFocus();
  }
  return true;
}

FindBarView::SearchTextfieldView::SearchTextfieldView() {
}

FindBarView::SearchTextfieldView::~SearchTextfieldView() {
}

void FindBarView::SearchTextfieldView::RequestFocus() {
  if (HasFocus())
    return;
  views::View::RequestFocus();
  SelectAll();
}

FindBarHost* FindBarView::find_bar_host() const {
  return static_cast<FindBarHost*>(host());
}

void FindBarView::OnThemeChanged() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (GetThemeProvider()) {
    close_button_->SetBackground(
        GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT),
        rb.GetBitmapNamed(IDR_CLOSE_BAR),
        rb.GetBitmapNamed(IDR_CLOSE_BAR_MASK));
  }
}
