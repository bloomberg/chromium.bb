// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/background_view.h"

#include <string>
#include <vector>
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/x11_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/oobe_progress_bar.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/feedback_menu_button.h"
#include "chrome/browser/chromeos/status/language_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/dom_view.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

using views::WidgetGtk;

namespace {

// The same as TextButton but switches cursor to hand cursor when mouse
// is over the button.
class TextButtonWithHandCursorOver : public views::TextButton {
 public:
  TextButtonWithHandCursorOver(views::ButtonListener* listener,
                               const std::wstring& text)
      : views::TextButton(listener, text) {
  }

  virtual ~TextButtonWithHandCursorOver() {}

  virtual gfx::NativeCursor GetCursorForPoint(
      views::Event::EventType event_type,
      const gfx::Point& p) {
    if (!IsEnabled()) {
      return NULL;
    }
    return gdk_cursor_new(GDK_HAND2);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TextButtonWithHandCursorOver);
};

// This gets rid of the ugly X default cursor.
static void ResetXCursor() {
  // TODO(sky): nuke this once new window manager is in place.
  Display* display = x11_util::GetXDisplay();
  Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
  XID root_window = x11_util::GetX11RootWindow();
  XSetWindowAttributes attr;
  attr.cursor = cursor;
  XChangeWindowAttributes(display, root_window, CWCursor, &attr);
}

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// BackgroundView public:

BackgroundView::BackgroundView()
    : status_area_(NULL),
      os_version_label_(NULL),
      boot_times_label_(NULL),
      progress_bar_(NULL),
      go_incognito_button_(NULL),
      did_paint_(false),
      delegate_(NULL),
      background_area_(NULL) {
}

void BackgroundView::Init(const GURL& background_url) {
  views::Painter* painter = CreateBackgroundPainter();
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  InitStatusArea();
  InitInfoLabels();
  if (!background_url.is_empty()) {
    Profile* profile = ProfileManager::GetDefaultProfile();
    background_area_ = new DOMView();
    AddChildView(background_area_);
    background_area_->Init(profile, NULL);
    background_area_->SetVisible(false);
    background_area_->LoadURL(background_url);
  }
}

// static
views::Widget* BackgroundView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    BackgroundView** view) {
  ResetXCursor();

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, bounds);
  *view = new BackgroundView();
  (*view)->Init(GURL());
  window->SetContentsView(*view);

  (*view)->UpdateWindowType();

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  return window;
}

void BackgroundView::SetStatusAreaVisible(bool visible) {
  status_area_->SetVisible(visible);
}

void BackgroundView::SetOobeProgressBarVisible(bool visible) {
  if (!progress_bar_ && visible)
    InitProgressBar();

  if (progress_bar_)
    progress_bar_->SetVisible(visible);
}

bool BackgroundView::IsOobeProgressBarVisible() {
  return progress_bar_ && progress_bar_->IsVisible();
}

void BackgroundView::SetOobeProgress(LoginStep step) {
  DCHECK(step <= PICTURE);
  if (progress_bar_)
    progress_bar_->SetProgress(step);
}

// Toggles GoIncognito button visibility.
void BackgroundView::SetGoIncognitoButtonVisible(bool visible,
                                                 Delegate *delegate) {
  // Set delegate to handle button pressing.
  delegate_ = delegate;
  bool currently_visible =
      go_incognito_button_ && go_incognito_button_->IsVisible();
  if (currently_visible != visible) {
    if (!go_incognito_button_) {
      InitGoIncognitoButton();
    }
    go_incognito_button_->SetVisible(visible);
  }
}

void BackgroundView::ShowScreenSaver() {
  SetStatusAreaVisible(false);
  background_area_->SetVisible(true);
}

void BackgroundView::HideScreenSaver() {
  SetStatusAreaVisible(true);
  // TODO(oshima): we need a way to suspend screen saver
  // to save power when it's not visible.
  background_area_->SetVisible(false);
}

bool BackgroundView::IsScreenSaverVisible() {
  return ScreenSaverEnabled() && background_area_->IsVisible();
}

bool BackgroundView::ScreenSaverEnabled() {
  return background_area_ != NULL;
}

void BackgroundView::OnOwnerChanged() {
  if (go_incognito_button_) {
    // BackgroundView is passed among multiple controllers, so they should
    // explicitly enable "Go incognito" button if needed.
    RemoveChildView(go_incognito_button_);
    delete go_incognito_button_;
    go_incognito_button_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BackgroundView protected:

void BackgroundView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);
  if (!did_paint_) {
    did_paint_ = true;
    UpdateWindowType();
  }
}

void BackgroundView::Layout() {
  const int kCornerPadding = 5;
  const int kInfoLeftPadding = 60;
  const int kInfoBottomPadding = 10;
  const int kInfoBetweenLinesPadding = 4;
  const int kProgressBarBottomPadding = 20;
  const int kProgressBarWidth = 750;
  const int kProgressBarHeight = 70;
  const int kGoIncognitoButtonBottomPadding = 12;
  const int kGoIncognitoButtonRightPadding = 12;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - kCornerPadding,
      kCornerPadding,
      status_area_size.width(),
      status_area_size.height());
  gfx::Size version_size = os_version_label_->GetPreferredSize();
  os_version_label_->SetBounds(
      kInfoLeftPadding,
      height() -
          ((2 * version_size.height()) +
          kInfoBottomPadding +
          kInfoBetweenLinesPadding),
      width() - 2 * kInfoLeftPadding,
      version_size.height());
  boot_times_label_->SetBounds(
      kInfoLeftPadding,
      height() - (version_size.height() + kInfoBottomPadding),
      width() - 2 * kCornerPadding,
      version_size.height());
  if (progress_bar_) {
    progress_bar_->SetBounds(
        (width() - kProgressBarWidth) / 2,
        (height() - kProgressBarBottomPadding - kProgressBarHeight),
        kProgressBarWidth,
        kProgressBarHeight);
  }
  if (go_incognito_button_) {
    gfx::Size go_button_size = go_incognito_button_->GetPreferredSize();
    go_incognito_button_->SetBounds(
          width() - go_button_size.width()- kGoIncognitoButtonRightPadding,
          height() - go_button_size.height() - kGoIncognitoButtonBottomPadding,
          go_button_size.width(),
          go_button_size.height());
  }
  if (background_area_)
    background_area_->SetBounds(this->bounds());
}

void BackgroundView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void BackgroundView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
  SchedulePaint();
}

gfx::NativeWindow BackgroundView::GetNativeWindow() const {
  return
      GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

bool BackgroundView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->clock_view() ||
      button_view == status_area_->feedback_view() ||
      button_view == status_area_->language_view() ||
      button_view == status_area_->network_view()) {
    return false;
  }
  return true;
}

void BackgroundView::OpenButtonOptions(const views::View* button_view) const {
  // TODO(avayvod): Add some dialog for options or remove them completely.
}

bool BackgroundView::IsBrowserMode() const {
  return false;
}

bool BackgroundView::IsScreenLockerMode() const {
  return false;
}

void BackgroundView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (sender == go_incognito_button_) {
    DCHECK(delegate_);
    if (delegate_) {
      delegate_->OnGoIncognitoButton();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// BackgroundView private:

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  // Feedback button shoudn't be visible on OOBE/login/screen lock.
  status_area_->feedback_view()->SetVisible(false);
  AddChildView(status_area_);
}

void BackgroundView::InitInfoLabels() {
  const SkColor kVersionColor = 0xff8eb1f4;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  os_version_label_ = new views::Label();
  os_version_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  os_version_label_->SetColor(kVersionColor);
  os_version_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  AddChildView(os_version_label_);
  boot_times_label_ = new views::Label();
  boot_times_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  boot_times_label_->SetColor(kVersionColor);
  boot_times_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  AddChildView(boot_times_label_);

  if (CrosLibrary::Get()->EnsureLoaded()) {
    version_loader_.GetVersion(
        &version_consumer_, NewCallback(this, &BackgroundView::OnVersion));
    boot_times_loader_.GetBootTimes(
        &boot_times_consumer_, NewCallback(this, &BackgroundView::OnBootTimes));
  } else {
    os_version_label_->SetText(
        ASCIIToWide(CrosLibrary::Get()->load_error_string()));
  }
}

void BackgroundView::InitProgressBar() {
  std::vector<int> steps;
  steps.push_back(IDS_OOBE_SELECT_NETWORK);
#if defined(OFFICIAL_BUILD)
  steps.push_back(IDS_OOBE_EULA);
#endif
  steps.push_back(IDS_OOBE_SIGNIN);
#if defined(OFFICIAL_BUILD)
  steps.push_back(IDS_OOBE_REGISTRATION);
#endif
  steps.push_back(IDS_OOBE_PICTURE);
  progress_bar_ = new OobeProgressBar(steps);
  AddChildView(progress_bar_);
}

void BackgroundView::InitGoIncognitoButton() {
  SkColor kButtonColor = 0xFF4F6985;
  SkColor kStrokeColor = 0xFF657A91;
  int kStrokeWidth = 1;
  int kVerticalPadding = 8;
  int kHorizontalPadding = 12;
  int kCornerRadius = 4;

  go_incognito_button_ =
      new TextButtonWithHandCursorOver(this, std::wstring());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  go_incognito_button_->SetIcon(*rb.GetBitmapNamed(IDR_INCOGNITO_GUY));
  go_incognito_button_->SetFocusable(true);
  // Set label colors.
  go_incognito_button_->SetEnabledColor(SK_ColorWHITE);
  go_incognito_button_->SetDisabledColor(SK_ColorWHITE);
  go_incognito_button_->SetHighlightColor(SK_ColorWHITE);
  go_incognito_button_->SetHoverColor(SK_ColorWHITE);
  // Disable throbbing and make border always visible.
  go_incognito_button_->SetAnimationDuration(0);
  go_incognito_button_->SetNormalHasBorder(true);
  // Setup round shapes.
  go_incognito_button_->set_background(
      CreateRoundedBackground(
          kCornerRadius, kStrokeWidth, kButtonColor, kStrokeColor));

  go_incognito_button_->set_border(
      views::Border::CreateEmptyBorder(kVerticalPadding,
                                       kHorizontalPadding,
                                       kVerticalPadding,
                                       kHorizontalPadding));
  // Set button text.
  UpdateLocalizedStrings();
  // Enable and add to the views hierarchy.
  go_incognito_button_->SetEnabled(true);
  AddChildView(go_incognito_button_);
}

void BackgroundView::UpdateLocalizedStrings() {
  if (go_incognito_button_) {
    go_incognito_button_->SetText(
        UTF8ToWide(l10n_util::GetStringUTF8(IDS_GO_INCOGNITO_BUTTON)));
  }
}

void BackgroundView::UpdateWindowType() {
  std::vector<int> params;
  params.push_back(did_paint_ ? 1 : 0);
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      WM_IPC_WINDOW_LOGIN_BACKGROUND,
      &params);
}

void BackgroundView::OnVersion(
    VersionLoader::Handle handle, std::string version) {
  // TODO(jungshik): Is string concatenation OK here?
  std::string version_text = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  version_text += ' ';
  version_text += l10n_util::GetStringUTF8(IDS_VERSION_FIELD_PREFIX);
  version_text += ' ';
  version_text += version;
  os_version_label_->SetText(UTF8ToWide(version_text));
}

void BackgroundView::OnBootTimes(
    BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times) {
  // TODO(davemoore) if we decide to keep these times visible we will need
  // to localize the strings.
  const char* kBootTimesNoChromeExec =
      "Boot took %.2f seconds (firmware %.2fs, kernel %.2fs, system %.2fs)";
  const char* kBootTimesChromeExec =
      "Boot took %.2f seconds "
      "(firmware %.2fs, kernel %.2fs, system %.2fs, chrome %.2fs)";
  std::string boot_times_text;

  if (boot_times.chrome > 0) {
    boot_times_text =
        base::StringPrintf(
            kBootTimesChromeExec,
            boot_times.total,
            boot_times.firmware,
            boot_times.pre_startup,
            boot_times.system,
            boot_times.chrome);
  } else {
    boot_times_text =
        base::StringPrintf(
            kBootTimesNoChromeExec,
            boot_times.total,
            boot_times.firmware,
            boot_times.pre_startup,
            boot_times.system);
  }
  // Use UTF8ToWide once this string is localized.
  boot_times_label_->SetText(ASCIIToWide(boot_times_text));
}

}  // namespace chromeos
