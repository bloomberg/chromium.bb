// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

namespace {

// How much extra padding to put around our content over what the InfoBubble
// provides.
const int kBubblePadding = 4;

// How much extra padding to put around our content over what the InfoBubble
// provides in alternative OEM bubble.
const int kOEMBubblePadding = 4;

// Padding between parts of strings on the same line (for instance,
// "New!" and "Search from the address bar!"
const int kStringSeparationPadding = 2;

// Margin around close button.
const int kMarginRightOfCloseButton = 7;

}  // namespace

// Base class for implementations of the client view which appears inside the
// first run bubble. It is a dialog-ish view, but is not a true dialog.
class FirstRunBubbleViewBase : public views::View,
                               public views::ButtonListener,
                               public views::FocusChangeListener {
 public:
  // Called by FirstRunBubble::Show to request focus for the proper button
  // in the FirstRunBubbleView when it is shown.
  virtual void BubbleShown() = 0;
};

// FirstRunBubbleView ---------------------------------------------------------

class FirstRunBubbleView : public FirstRunBubbleViewBase {
 public:
  FirstRunBubbleView(FirstRunBubble* bubble_window, Profile* profile);

 private:
  virtual ~FirstRunBubbleView() {}

  // FirstRunBubbleViewBase:
  virtual void BubbleShown();

  // Overridden from View:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // FocusChangeListener:
  virtual void FocusWillChange(View* focused_before, View* focused_now);

  FirstRunBubble* bubble_window_;
  views::Label* label1_;
  views::Label* label2_;
  views::Label* label3_;
  views::NativeButton* change_button_;
  views::NativeButton* keep_button_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleView);
};

FirstRunBubbleView::FirstRunBubbleView(FirstRunBubble* bubble_window,
                                       Profile* profile)
    : bubble_window_(bubble_window),
      label1_(NULL),
      label2_(NULL),
      label3_(NULL),
      keep_button_(NULL),
      change_button_(NULL),
      profile_(profile) {
  const gfx::Font& font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);

  label1_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_TITLE)));
  label1_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
  label1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label1_);

  gfx::Size ps = GetPreferredSize();

  label2_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_SUBTEXT)));
  label2_->SetMultiLine(true);
  label2_->SetFont(font);
  label2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label2_->SizeToFit(ps.width() - kBubblePadding * 2);
  AddChildView(label2_);

  std::wstring question_str = UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_FR_BUBBLE_QUESTION,
      GetDefaultSearchEngineName(profile)));
  label3_ = new views::Label(question_str);
  label3_->SetMultiLine(true);
  label3_->SetFont(font);
  label3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label3_->SizeToFit(ps.width() - kBubblePadding * 2);
  AddChildView(label3_);

  std::wstring keep_str = UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_FR_BUBBLE_OK,
      GetDefaultSearchEngineName(profile)));
  keep_button_ = new views::NativeButton(this, keep_str);
  keep_button_->SetIsDefault(true);
  AddChildView(keep_button_);

  std::wstring change_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_CHANGE));
  change_button_ = new views::NativeButton(this, change_str);
  AddChildView(change_button_);
}

void FirstRunBubbleView::BubbleShown() {
  keep_button_->RequestFocus();
}

void FirstRunBubbleView::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  UserMetrics::RecordAction(UserMetricsAction("FirstRunBubbleView_Clicked"),
                            profile_);
  bubble_window_->set_fade_away_on_close(true);
  bubble_window_->Close();
  if (change_button_ == sender) {
    UserMetrics::RecordAction(
                    UserMetricsAction("FirstRunBubbleView_ChangeButton"),
                    profile_);

    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      browser->OpenSearchEngineOptionsDialog();
    }
  }
}

void FirstRunBubbleView::Layout() {
  gfx::Size canvas = GetPreferredSize();

  // The multiline business that follows is dirty hacks to get around
  // bug 1325257.
  label1_->SetMultiLine(false);
  gfx::Size pref_size = label1_->GetPreferredSize();
  label1_->SetMultiLine(true);
  label1_->SizeToFit(canvas.width() - kBubblePadding * 2);
  label1_->SetBounds(kBubblePadding, kBubblePadding,
                     canvas.width() - kBubblePadding * 2,
                     pref_size.height());

  int next_v_space = label1_->y() + pref_size.height() +
                     views::kRelatedControlSmallVerticalSpacing;

  pref_size = label2_->GetPreferredSize();
  label2_->SetBounds(kBubblePadding, next_v_space,
                     canvas.width() - kBubblePadding * 2,
                     pref_size.height());

  next_v_space = label2_->y() + label2_->height() +
                 kPanelSubVerticalSpacing;

  pref_size = label3_->GetPreferredSize();
  label3_->SetBounds(kBubblePadding, next_v_space,
                     canvas.width() - kBubblePadding * 2,
                     pref_size.height());

  pref_size = change_button_->GetPreferredSize();
  change_button_->SetBounds(
      canvas.width() - pref_size.width() - kBubblePadding,
      canvas.height() - pref_size.height() - views::kButtonVEdgeMargin,
      pref_size.width(), pref_size.height());

  pref_size = keep_button_->GetPreferredSize();
  keep_button_->SetBounds(change_button_->x() - pref_size.width() -
                          views::kRelatedButtonHSpacing, change_button_->y(),
                          pref_size.width(), pref_size.height());
}

gfx::Size FirstRunBubbleView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUNBUBBLE_DIALOG_HEIGHT_LINES));
}

void FirstRunBubbleView::FocusWillChange(View* focused_before,
                                         View* focused_now) {
  if (focused_before &&
      (focused_before->GetClassName() == views::NativeButton::kViewClassName)) {
    views::NativeButton* before =
        static_cast<views::NativeButton*>(focused_before);
    before->SetIsDefault(false);
  }
  if (focused_now &&
      (focused_now->GetClassName() == views::NativeButton::kViewClassName)) {
    views::NativeButton* after = static_cast<views::NativeButton*>(focused_now);
    after->SetIsDefault(true);
  }
}

// FirstRunOEMBubbleView ------------------------------------------------------

class FirstRunOEMBubbleView : public FirstRunBubbleViewBase {
 public:
  FirstRunOEMBubbleView(FirstRunBubble* bubble_window, Profile* profile);

 private:
  virtual ~FirstRunOEMBubbleView() { }

  // FirstRunBubbleViewBase:
  virtual void BubbleShown();

  // Overridden from View:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // FocusChangeListener:
  virtual void FocusWillChange(View* focused_before, View* focused_now);

  FirstRunBubble* bubble_window_;
  views::Label* label1_;
  views::Label* label2_;
  views::Label* label3_;
  views::ImageButton* close_button_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunOEMBubbleView);
};

FirstRunOEMBubbleView::FirstRunOEMBubbleView(FirstRunBubble* bubble_window,
                                             Profile* profile)
    : bubble_window_(bubble_window),
      label1_(NULL),
      label2_(NULL),
      label3_(NULL),
      close_button_(NULL),
      profile_(profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::MediumFont);

  label1_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_OEM_BUBBLE_TITLE_1)));
  label1_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
  label1_->SetColor(SK_ColorRED);
  label1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label1_);

  label2_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_OEM_BUBBLE_TITLE_2)));
  label2_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
  label2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label2_);

  gfx::Size ps = GetPreferredSize();

  label3_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_OEM_BUBBLE_SUBTEXT)));
  label3_->SetMultiLine(true);
  label3_->SetFont(font);
  label3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label3_->SizeToFit(ps.width() - kOEMBubblePadding * 2);
  AddChildView(label3_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));

  AddChildView(close_button_);
}

void FirstRunOEMBubbleView::BubbleShown() {
  RequestFocus();
  // No button in oem_bubble to request focus.
}

void FirstRunOEMBubbleView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  UserMetrics::RecordAction(UserMetricsAction("FirstRunOEMBubbleView_Clicked"),
                            profile_);
  bubble_window_->set_fade_away_on_close(true);
  bubble_window_->Close();
}

void FirstRunOEMBubbleView::Layout() {
  gfx::Size canvas = GetPreferredSize();

  // First, draw the close button on the far right.
  gfx::Size sz = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      canvas.width() - sz.width() - kMarginRightOfCloseButton,
      kOEMBubblePadding, sz.width(), sz.height());

  gfx::Size pref_size = label1_->GetPreferredSize();
  label1_->SetBounds(kOEMBubblePadding, kOEMBubblePadding,
                     pref_size.width() + kOEMBubblePadding * 2,
                     pref_size.height());

  pref_size = label2_->GetPreferredSize();
  label2_->SetBounds(
      kOEMBubblePadding * 2 + label1_->GetPreferredSize().width(),
      kOEMBubblePadding, canvas.width() - kOEMBubblePadding * 2,
      pref_size.height());

  int next_v_space =
      label1_->y() + pref_size.height() +
          views::kRelatedControlSmallVerticalSpacing;

  pref_size = label3_->GetPreferredSize();
  label3_->SetBounds(kOEMBubblePadding, next_v_space,
                     canvas.width() - kOEMBubblePadding * 2,
                     pref_size.height());
}

gfx::Size FirstRunOEMBubbleView::GetPreferredSize() {
  // Calculate width based on font and text.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(
      ResourceBundle::MediumFont).DeriveFont(3, gfx::Font::BOLD);
  gfx::Size size = gfx::Size(
      ui::GetLocalizedContentsWidthForFont(
          IDS_FIRSTRUNOEMBUBBLE_DIALOG_WIDTH_CHARS, font),
      ui::GetLocalizedContentsHeightForFont(
          IDS_FIRSTRUNOEMBUBBLE_DIALOG_HEIGHT_LINES, font));

  // WARNING: HACK. Vista and XP calculate font size differently; this means
  // that a dialog box correctly proportioned for XP will appear too large in
  // Vista. The correct thing to do is to change font size calculations in
  // XP or Vista so that the length of a string is calculated properly. For
  // now, we force Vista to show a correctly-sized box by taking account of
  // the difference in font size calculation. The coefficient should not be
  // stored in a variable because it's a hack and should go away.
  if (views::WidgetWin::IsAeroGlassEnabled()) {
    size.set_width(static_cast<int>(size.width() * 0.85));
    size.set_height(static_cast<int>(size.height() * 0.85));
  }
  return size;
}

void FirstRunOEMBubbleView::FocusWillChange(View* focused_before,
                                            View* focused_now) {
  // No buttons in oem_bubble to register focus changes.
}

// FirstRunMinimalBubbleView --------------------------------------------------
// TODO(mirandac): combine FRBubbles more elegantly.  http://crbug.com/41353

class FirstRunMinimalBubbleView : public FirstRunBubbleViewBase {
 public:
  FirstRunMinimalBubbleView(FirstRunBubble* bubble_window, Profile* profile);

 private:
  virtual ~FirstRunMinimalBubbleView() { }

  // FirstRunBubbleViewBase:
  virtual void BubbleShown();

  // Overridden from View:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) { }
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // FocusChangeListener:
  virtual void FocusWillChange(View* focused_before, View* focused_now);

  FirstRunBubble* bubble_window_;
  Profile* profile_;
  views::Label* label1_;
  views::Label* label2_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunMinimalBubbleView);
};

FirstRunMinimalBubbleView::FirstRunMinimalBubbleView(
    FirstRunBubble* bubble_window,
    Profile* profile)
    : bubble_window_(bubble_window),
      profile_(profile),
      label1_(NULL),
      label2_(NULL) {
  const gfx::Font& font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);

  label1_ = new views::Label(UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_FR_SE_BUBBLE_TITLE,
      GetDefaultSearchEngineName(profile_))));
  label1_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
  label1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label1_);

  gfx::Size ps = GetPreferredSize();

  label2_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_SUBTEXT)));
  label2_->SetMultiLine(true);
  label2_->SetFont(font);
  label2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label2_->SizeToFit(ps.width() - kBubblePadding * 2);
  AddChildView(label2_);
}

void FirstRunMinimalBubbleView::BubbleShown() {
  RequestFocus();
}

void FirstRunMinimalBubbleView::Layout() {
  gfx::Size canvas = GetPreferredSize();

  // See comments in FirstRunOEMBubbleView::Layout explaining this hack.
  label1_->SetMultiLine(false);
  gfx::Size pref_size = label1_->GetPreferredSize();
  label1_->SetMultiLine(true);
  label1_->SizeToFit(canvas.width() - kBubblePadding * 2);
  label1_->SetBounds(kBubblePadding, kBubblePadding,
                     canvas.width() - kBubblePadding * 2,
                     pref_size.height());

  int next_v_space = label1_->y() + pref_size.height() +
                     views::kRelatedControlSmallVerticalSpacing;

  pref_size = label2_->GetPreferredSize();
  label2_->SetBounds(kBubblePadding, next_v_space,
                     canvas.width() - kBubblePadding * 2,
                     pref_size.height());
}

gfx::Size FirstRunMinimalBubbleView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_MINIMAL_BUBBLE_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUN_MINIMAL_BUBBLE_DIALOG_HEIGHT_LINES));
}

void FirstRunMinimalBubbleView::FocusWillChange(View* focused_before,
                                                View* focused_now) {
  // No buttons in minimal bubble to register focus changes.
}


// FirstRunBubble -------------------------------------------------------------

// static
FirstRunBubble* FirstRunBubble::Show(Profile* profile,
                                     views::Widget* parent,
                                     const gfx::Rect& position_relative_to,
                                     BubbleBorder::ArrowLocation arrow_location,
                                     FirstRun::BubbleType bubble_type) {
  FirstRunBubble* window = new FirstRunBubble();
  FirstRunBubbleViewBase* view = NULL;

  switch (bubble_type) {
    case FirstRun::OEM_BUBBLE:
      view = new FirstRunOEMBubbleView(window, profile);
      break;
    case FirstRun::LARGE_BUBBLE:
      view = new FirstRunBubbleView(window, profile);
      break;
    case FirstRun::MINIMAL_BUBBLE:
      view = new FirstRunMinimalBubbleView(window, profile);
      break;
    default:
      NOTREACHED();
  }
  window->set_view(view);
  window->Init(parent, position_relative_to, arrow_location, view, window);
  window->GetFocusManager()->AddFocusChangeListener(view);
  view->BubbleShown();
  return window;
}

FirstRunBubble::FirstRunBubble()
    : has_been_activated_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(enable_window_method_factory_(this)),
      view_(NULL) {
}

FirstRunBubble::~FirstRunBubble() {
  enable_window_method_factory_.RevokeAll();
  GetFocusManager()->RemoveFocusChangeListener(view_);
}

void FirstRunBubble::EnableParent() {
  ::EnableWindow(GetParent(), true);
  // Reactivate the FirstRunBubble so it responds to OnActivate messages.
  SetWindowPos(GetParent(), 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_SHOWWINDOW);
}

void FirstRunBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // Keep the bubble around for kLingerTime milliseconds, to prevent accidental
  // closure.
  const int kLingerTime = 3000;

  // We might get re-enabled right before we are closed (sequence is: we get
  // deactivated, we call close, before we are actually closed we get
  // reactivated). Don't do the disabling of the parent in such cases.
  if (action == WA_ACTIVE && !has_been_activated_) {
    has_been_activated_ = true;

    ::EnableWindow(GetParent(), false);

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        enable_window_method_factory_.NewRunnableMethod(
            &FirstRunBubble::EnableParent),
        kLingerTime);
    return;
  }

  // Keep window from automatically closing until kLingerTime has passed.
  if (::IsWindowEnabled(GetParent()))
    InfoBubble::OnActivate(action, minimized, window);
}

void FirstRunBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                       bool closed_by_escape) {
  // Make sure our parent window is re-enabled.
  if (!IsWindowEnabled(GetParent()))
    ::EnableWindow(GetParent(), true);
}
