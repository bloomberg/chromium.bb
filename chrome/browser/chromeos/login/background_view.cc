// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/background_view.h"

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/shutdown_button.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_version_info.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_util.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

using views::Widget;

namespace {

const SkColor kVersionColor = 0xff5c739f;

// Tentative background color that matches WebUI login.
const SkColor kBackgroundColor = 0xfffefefe;

// Returns the corresponding step id for step constant.
int GetStepId(size_t step) {
  switch (step) {
    case chromeos::BackgroundView::SELECT_NETWORK:
      return IDS_OOBE_SELECT_NETWORK;
    case chromeos::BackgroundView::EULA:
      return IDS_OOBE_EULA;
    case chromeos::BackgroundView::SIGNIN:
      return IDS_OOBE_SIGNIN;
    case chromeos::BackgroundView::REGISTRATION:
      return IDS_OOBE_REGISTRATION;
    case chromeos::BackgroundView::PICTURE:
      return IDS_OOBE_PICTURE;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// BackgroundView public:

BackgroundView::BackgroundView()
    : status_area_(NULL),
      os_version_label_(NULL),
      boot_times_label_(NULL),
      shutdown_button_(NULL),
#if defined(OFFICIAL_BUILD)
      is_official_build_(true),
#else
      is_official_build_(false),
#endif
      background_area_(NULL),
      version_info_updater_(this) {
}

BackgroundView::~BackgroundView() {
  version_info_updater_.set_delegate(NULL);
}

void BackgroundView::Init(const GURL& background_url) {
  set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
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

void BackgroundView::EnableShutdownButton(bool enable) {
  if (enable) {
    if (shutdown_button_)
      return;
    shutdown_button_ = new ShutdownButton();
    shutdown_button_->Init();
    AddChildView(shutdown_button_);
  } else {
    if (!shutdown_button_)
      return;
    delete shutdown_button_;
    shutdown_button_ = NULL;
    SchedulePaint();
  }
}

// static
views::Widget* BackgroundView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    const GURL& background_url,
    BackgroundView** view) {
  Widget* window = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = bounds;
  window->Init(params);
  *view = new BackgroundView();
  (*view)->Init(background_url);

  if ((*view)->ScreenSaverEnabled())
    (*view)->ShowScreenSaver();

  window->SetContentsView(*view);

  (*view)->UpdateWindowType();

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  LoginUtils::Get()->SetBackgroundView(*view);

  return window;
}

void BackgroundView::CreateModalPopup(views::WidgetDelegate* view) {
  views::Widget* window = browser::CreateViewsWindow(
      GetNativeWindow(), view);
  window->SetAlwaysOnTop(true);
  window->Show();
}

gfx::NativeWindow BackgroundView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void BackgroundView::SetStatusAreaVisible(bool visible) {
  status_area_->SetVisible(visible);
}

void BackgroundView::SetStatusAreaEnabled(bool enable) {
  status_area_->MakeButtonsActive(enable);
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

void BackgroundView::SetDefaultUse24HourClock(bool use_24hour_clock) {
  DCHECK(status_area_);
  status_area_->SetDefaultUse24HourClock(use_24hour_clock);
}

///////////////////////////////////////////////////////////////////////////////
// BackgroundView protected:

void BackgroundView::Layout() {
  const int kCornerPadding = 5;
  const int kInfoLeftPadding = 10;
  const int kInfoBottomPadding = 10;
  const int kInfoBetweenLinesPadding = 1;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - kCornerPadding,
      kCornerPadding,
      status_area_size.width(),
      status_area_size.height());
  gfx::Size version_size = os_version_label_->GetPreferredSize();
  int os_version_y = height() - version_size.height() - kInfoBottomPadding;
  if (!is_official_build_)
    os_version_y -= (version_size.height() + kInfoBetweenLinesPadding);
  os_version_label_->SetBounds(
      kInfoLeftPadding,
      os_version_y,
      width() - 2 * kInfoLeftPadding,
      version_size.height());
  if (!is_official_build_) {
    boot_times_label_->SetBounds(
        kInfoLeftPadding,
        height() - (version_size.height() + kInfoBottomPadding),
        width() - 2 * kInfoLeftPadding,
        version_size.height());
  }
  if (shutdown_button_) {
    shutdown_button_->LayoutIn(this);
  }
  if (background_area_)
    background_area_->SetBoundsRect(this->bounds());
}

void BackgroundView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

// Overridden from StatusAreaButton::Delegate:

bool BackgroundView::ShouldExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) const {
  if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS)
    return true;
  return false;
}

void BackgroundView::ExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) {
  if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS) {
    if (proxy_settings_dialog_.get() == NULL) {
      proxy_settings_dialog_.reset(new ProxySettingsDialog(
          this, GetNativeWindow()));
    }
    proxy_settings_dialog_->Show();
  }
}

gfx::Font BackgroundView::GetStatusAreaFont(const gfx::Font& font) const {
  return font.DeriveFont(0, gfx::Font::BOLD);
}

StatusAreaButton::TextStyle BackgroundView::GetStatusAreaTextStyle() const {
  return StatusAreaButton::GRAY_PLAIN;
}

void BackgroundView::ButtonVisibilityChanged(views::View* button_view) {
  status_area_->UpdateButtonVisibility();
}

// Overridden from LoginHtmlDialog::Delegate:

void BackgroundView::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
  InitInfoLabels();
  SchedulePaint();
}

void BackgroundView::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  os_version_label_->SetText(UTF8ToUTF16(os_version_label_text));
}

void BackgroundView::OnBootTimesLabelTextUpdated(
    const std::string& boot_times_label_text) {
  boot_times_label_->SetText(UTF8ToUTF16(boot_times_label_text));
}

///////////////////////////////////////////////////////////////////////////////
// BackgroundView private:

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaViewChromeos();
  status_area_->Init(this);
  AddChildView(status_area_);
}

void BackgroundView::InitInfoLabels() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  {
    int idx = GetIndexOf(os_version_label_);
    delete os_version_label_;
    os_version_label_ = new views::Label();
    os_version_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    os_version_label_->SetEnabledColor(kVersionColor);
    os_version_label_->SetBackgroundColor(background()->get_color());
    os_version_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    if (idx < 0)
      AddChildView(os_version_label_);
    else
      AddChildViewAt(os_version_label_, idx);
  }
  if (!is_official_build_) {
    int idx = GetIndexOf(boot_times_label_);
    delete boot_times_label_;
    boot_times_label_ = new views::Label();
    boot_times_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    boot_times_label_->SetEnabledColor(kVersionColor);
    boot_times_label_->SetBackgroundColor(background()->get_color());
    boot_times_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    if (idx < 0)
      AddChildView(boot_times_label_);
    else
      AddChildViewAt(boot_times_label_, idx);
  }

  version_info_updater_.StartUpdate(is_official_build_);
}

void BackgroundView::UpdateWindowType() {
#if defined(TOOLKIT_USES_GTK)
  std::vector<int> params;
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      WM_IPC_WINDOW_LOGIN_BACKGROUND,
      &params);
#endif
}

}  // namespace chromeos
