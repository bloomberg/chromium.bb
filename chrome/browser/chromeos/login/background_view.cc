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
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/oobe_progress_bar.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/shutdown_button.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/window.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/screen.h"
#include "views/window/window.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>  // NOLINT
#include <X11/Xcursor/Xcursor.h>  // NOLINT

using views::Widget;

namespace {

const SkColor kVersionColor = 0xff5c739f;

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

// This gets rid of the ugly X default cursor.
static void ResetXCursor() {
  // TODO(sky): nuke this once new window manager is in place.
  Display* display = ui::GetXDisplay();
  Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
  XID root_window = ui::GetX11RootWindow();
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
      shutdown_button_(NULL),
      did_paint_(false),
#if defined(OFFICIAL_BUILD)
      is_official_build_(true),
#else
      is_official_build_(false),
#endif
      background_area_(NULL) {
}

BackgroundView::~BackgroundView() {}

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
  ResetXCursor();

  Widget* window = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
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

void BackgroundView::CreateModalPopup(views::WindowDelegate* view) {
  views::Window* window = browser::CreateViewsWindow(
      GetNativeWindow(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
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
  DCHECK(step < STEPS_COUNT);
  if (progress_bar_)
    progress_bar_->SetStep(GetStepId(step));
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

///////////////////////////////////////////////////////////////////////////////
// BackgroundView protected:

void BackgroundView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  if (!did_paint_) {
    did_paint_ = true;
    UpdateWindowType();
  }
}

void BackgroundView::Layout() {
  const int kCornerPadding = 5;
  const int kInfoLeftPadding = 10;
  const int kInfoBottomPadding = 10;
  const int kInfoBetweenLinesPadding = 1;
  const int kProgressBarBottomPadding = 20;
  const int kProgressBarWidth = 750;
  const int kProgressBarHeight = 70;
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
  if (progress_bar_) {
    progress_bar_->SetBounds(
        (width() - kProgressBarWidth) / 2,
        (height() - kProgressBarBottomPadding - kProgressBarHeight),
        kProgressBarWidth,
        kProgressBarHeight);
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

Profile* BackgroundView::GetProfile() const {
  return NULL;
}

bool BackgroundView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->network_view()) {
    return true;
  }
  if (button_view == status_area_->clock_view() ||
      button_view == status_area_->input_method_view()) {
    return false;
  }
  return true;
}

void BackgroundView::OpenButtonOptions(const views::View* button_view) {
  if (button_view == status_area_->network_view()) {
    if (proxy_settings_dialog_.get() == NULL) {
      proxy_settings_dialog_.reset(new ProxySettingsDialog(
          this, GetNativeWindow()));
    }
    proxy_settings_dialog_->Show();
  }
}

StatusAreaHost::ScreenMode BackgroundView::GetScreenMode() const {
  return kLoginMode;
}

StatusAreaHost::TextStyle BackgroundView::GetTextStyle() const {
  return kWhitePlain;
}

// Overridden from LoginHtmlDialog::Delegate:
void BackgroundView::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
  InitInfoLabels();
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// BackgroundView private:

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

void BackgroundView::InitInfoLabels() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  {
    int idx = GetIndexOf(os_version_label_);
    delete os_version_label_;
    os_version_label_ = new views::Label();
    os_version_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    os_version_label_->SetColor(kVersionColor);
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
    boot_times_label_->SetColor(kVersionColor);
    boot_times_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
    if (idx < 0)
      AddChildView(boot_times_label_);
    else
      AddChildViewAt(boot_times_label_, idx);
  }

  if (CrosLibrary::Get()->EnsureLoaded()) {
    version_loader_.GetVersion(
        &version_consumer_,
        NewCallback(this, &BackgroundView::OnVersion),
        is_official_build_?
            VersionLoader::VERSION_SHORT_WITH_DATE :
            VersionLoader::VERSION_FULL);
    if (!is_official_build_) {
      boot_times_loader_.GetBootTimes(
          &boot_times_consumer_,
          NewCallback(this, &BackgroundView::OnBootTimes));
    }
  } else {
    UpdateVersionLabel();
  }

  policy::CloudPolicySubsystem* cloud_policy =
      g_browser_process->browser_policy_connector()->cloud_policy_subsystem();
  if (cloud_policy) {
    // Two-step reset because we want to construct new ObserverRegistrar after
    // destruction of old ObserverRegistrar to avoid DCHECK violation because
    // of adding existing observer.
    cloud_policy_registrar_.reset();
    cloud_policy_registrar_.reset(
        new policy::CloudPolicySubsystem::ObserverRegistrar(
            cloud_policy, this));

    // Ensure that we have up-to-date enterprise info in case enterprise policy
    // is already fetched and has finished initialization.
    UpdateEnterpriseInfo();
  }
}

void BackgroundView::InitProgressBar() {
  std::vector<int> steps;
  steps.push_back(GetStepId(SELECT_NETWORK));
#if defined(OFFICIAL_BUILD)
  steps.push_back(GetStepId(EULA));
#endif
  steps.push_back(GetStepId(SIGNIN));
#if defined(OFFICIAL_BUILD)
  if (WizardController::IsRegisterScreenDefined())
    steps.push_back(GetStepId(REGISTRATION));
#endif
  steps.push_back(GetStepId(PICTURE));
  progress_bar_ = new OobeProgressBar(steps);
  AddChildView(progress_bar_);
}

void BackgroundView::UpdateWindowType() {
  std::vector<int> params;
  params.push_back(did_paint_ ? 1 : 0);
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      WM_IPC_WINDOW_LOGIN_BACKGROUND,
      &params);
}

void BackgroundView::UpdateVersionLabel() {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    os_version_label_->SetText(
        ASCIIToWide(CrosLibrary::Get()->load_error_string()));
    return;
  }

  if (version_text_.empty())
    return;

  // TODO(jungshik): Is string concatenation OK here?
  std::string label_text = l10n_util::GetStringUTF8(IDS_PRODUCT_OS_NAME);
  label_text += ' ';
  label_text += l10n_util::GetStringUTF8(IDS_VERSION_FIELD_PREFIX);
  label_text += ' ';
  label_text += version_text_;

  if (!enterprise_domain_text_.empty()) {
    label_text += ' ';
    if (enterprise_status_text_.empty()) {
      label_text += l10n_util::GetStringFUTF8(
          IDS_LOGIN_MANAGED_BY_LABEL_FORMAT,
          UTF8ToUTF16(enterprise_domain_text_));
    } else {
      label_text += l10n_util::GetStringFUTF8(
          IDS_LOGIN_MANAGED_BY_WITH_STATUS_LABEL_FORMAT,
          UTF8ToUTF16(enterprise_domain_text_),
          UTF8ToUTF16(enterprise_status_text_));
    }
  }

  // Workaround over incorrect width calculation in old fonts.
  // TODO(glotov): remove the following line when new fonts are used.
  label_text += ' ';

  os_version_label_->SetText(UTF8ToWide(label_text));
}

void BackgroundView::UpdateEnterpriseInfo() {
  policy::BrowserPolicyConnector* policy_connector =
      g_browser_process->browser_policy_connector();

  std::string status_text;
  policy::CloudPolicySubsystem* cloud_policy_subsystem =
      policy_connector->cloud_policy_subsystem();
  if (cloud_policy_subsystem) {
    switch (cloud_policy_subsystem->state()) {
      case policy::CloudPolicySubsystem::UNENROLLED:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_PENDING);
        break;
      case policy::CloudPolicySubsystem::UNMANAGED:
      case policy::CloudPolicySubsystem::BAD_GAIA_TOKEN:
      case policy::CloudPolicySubsystem::LOCAL_ERROR:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_LOST_CONNECTION);
        break;
      case policy::CloudPolicySubsystem::NETWORK_ERROR:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_NETWORK_ERROR);
        break;
      case policy::CloudPolicySubsystem::TOKEN_FETCHED:
      case policy::CloudPolicySubsystem::SUCCESS:
        break;
    }
  }

  SetEnterpriseInfo(policy_connector->GetEnterpriseDomain(), status_text);
}

void BackgroundView::SetEnterpriseInfo(const std::string& domain_name,
                                       const std::string& status_text) {
  if (domain_name != enterprise_domain_text_ ||
      status_text != enterprise_status_text_) {
    enterprise_domain_text_ = domain_name;
    enterprise_status_text_ = status_text;
    UpdateVersionLabel();
  }
}

void BackgroundView::OnVersion(
    VersionLoader::Handle handle, std::string version) {
  version_text_.swap(version);
  UpdateVersionLabel();
}

void BackgroundView::OnBootTimes(
    BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times) {
  const char* kBootTimesNoChromeExec =
      "Non-firmware boot took %.2f seconds (kernel %.2fs, system %.2fs)";
  const char* kBootTimesChromeExec =
      "Non-firmware boot took %.2f seconds "
      "(kernel %.2fs, system %.2fs, chrome %.2fs)";
  std::string boot_times_text;

  if (boot_times.chrome > 0) {
    boot_times_text =
        base::StringPrintf(
            kBootTimesChromeExec,
            boot_times.total,
            boot_times.pre_startup,
            boot_times.system,
            boot_times.chrome);
  } else {
    boot_times_text =
        base::StringPrintf(
            kBootTimesNoChromeExec,
            boot_times.total,
            boot_times.pre_startup,
            boot_times.system);
  }
  // Use UTF8ToWide once this string is localized.
  boot_times_label_->SetText(ASCIIToWide(boot_times_text));
}

void BackgroundView::OnPolicyStateChanged(
    policy::CloudPolicySubsystem::PolicySubsystemState state,
    policy::CloudPolicySubsystem::ErrorDetails error_details) {
  UpdateEnterpriseInfo();
}

}  // namespace chromeos
