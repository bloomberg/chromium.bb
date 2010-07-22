// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/background_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/x11_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/feedback_menu_button.h"
#include "chrome/browser/chromeos/status/language_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/label.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

using views::WidgetGtk;

namespace chromeos {

BackgroundView::BackgroundView()
    : status_area_(NULL),
      os_version_label_(NULL),
      boot_times_label_(NULL),
      did_paint_(false) {
  views::Painter* painter = CreateBackgroundPainter();
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  InitStatusArea();
  InitInfoLabels();
}

static void ResetXCursor() {
  // TODO(sky): nuke this once new window manager is in place.
  // This gets rid of the ugly X default cursor.
  Display* display = x11_util::GetXDisplay();
  Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
  XID root_window = x11_util::GetX11RootWindow();
  XSetWindowAttributes attr;
  attr.cursor = cursor;
  XChangeWindowAttributes(display, root_window, CWCursor, &attr);
}

// static
views::Widget* BackgroundView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    BackgroundView** view) {
  ResetXCursor();

  WidgetGtk* window = new WidgetGtk(WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, bounds);
  *view = new BackgroundView();
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

void BackgroundView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);
  if (!did_paint_) {
    did_paint_ = true;
    UpdateWindowType();
  }
}

void BackgroundView::Layout() {
  int corner_padding = 5;
  int kInfoLeftPadding = 60;
  int kInfoBottomPadding = 40;
  int kInfoBetweenLinesPadding = 4;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - corner_padding,
      corner_padding,
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
      width() - 2 * corner_padding,
      version_size.height());
}

void BackgroundView::ChildPreferredSizeChanged(View* child) {
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

bool BackgroundView::IsButtonVisible(const views::View* button_view) const {
  if (button_view == status_area_->feedback_view())
    return false;
  return true;
}

bool BackgroundView::IsBrowserMode() const {
  return false;
}

bool BackgroundView::IsScreenLockerMode() const {
  return false;
}

void BackgroundView::LocaleChanged() {
  Layout();
  SchedulePaint();
}

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
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
        StringPrintf(
            kBootTimesChromeExec,
            boot_times.total,
            boot_times.firmware,
            boot_times.pre_startup,
            boot_times.system,
            boot_times.chrome);
  } else {
    boot_times_text =
        StringPrintf(
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
