// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

#if defined(USE_VIRTUAL_KEYBOARD)
#include "chrome/browser/ui/virtual_keyboard/virtual_keyboard_manager.h"
#endif


using content::RenderViewHost;
using content::WebContents;

namespace {

const char kViewClassName[] = "browser/chromeos/login/WebUILoginView";

// These strings must be kept in sync with handleAccelerator() in oobe.js.
const char kAccelNameAccessibility[] = "accessibility";
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameExit[] = "exit";
const char kAccelNameVersion[] = "version";

// Observes IPC messages from the FrameSniffer and notifies JS if error
// appears.
class SnifferObserver : public content::RenderViewHostObserver {
 public:
  SnifferObserver(RenderViewHost* host, content::WebUI* webui)
      : content::RenderViewHostObserver(host), webui_(webui) {
    DCHECK(webui_);
    Send(new ChromeViewMsg_StartFrameSniffer(routing_id(),
                                             UTF8ToUTF16("gaia-frame")));
  }

  virtual ~SnifferObserver() {}

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SnifferObserver, message)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FrameLoadingError, OnError)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnError(int error) {
    base::FundamentalValue error_value(error);
    webui_->CallJavascriptFunction("login.ErrorMessageScreen.onFrameError",
                                   error_value);
  }

  content::WebUI* webui_;
};

// A View class which places its first child at the right most position.
class RightAlignedView : public views::View {
 public:
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
};

void RightAlignedView::Layout() {
  if (has_children()) {
    views::View* child = child_at(0);
    gfx::Size preferred_size = child->GetPreferredSize();
    child->SetBounds(width() - preferred_size.width(),
                     0, preferred_size.width(), preferred_size.height());
  }
}

void RightAlignedView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

}  // namespace

namespace chromeos {

// static
const int WebUILoginView::kStatusAreaCornerPadding = 5;

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : status_area_(NULL),
      webui_login_(NULL),
      login_window_(NULL),
      status_window_(NULL),
      host_window_frozen_(false),
      status_area_visibility_on_init_(true),
      login_page_is_loaded_(false),
      should_emit_login_prompt_visible_(true) {

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_WEBUI_READY,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_IMAGES_LOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_WIZARD_FIRST_SCREEN_SHOWN,
                 content::NotificationService::AllSources());

#if defined(USE_VIRTUAL_KEYBOARD)
  // Make sure the singleton VirtualKeyboardManager object is created.
  VirtualKeyboardManager::GetInstance();
#endif
  accel_map_[ui::Accelerator(ui::VKEY_Z, false, true, true)] =
      kAccelNameAccessibility;
  accel_map_[ui::Accelerator(ui::VKEY_ESCAPE, false, false, false)] =
      kAccelNameCancel;
  accel_map_[ui::Accelerator(ui::VKEY_E, false, true, true)] =
      kAccelNameEnrollment;
  // This should be kept in sync with the IDC_EXIT accelerator.
  accel_map_[ui::Accelerator(ui::VKEY_Q, true, true, false)] =
      kAccelNameExit;
  accel_map_[ui::Accelerator(ui::VKEY_V, false, false, true)] =
      kAccelNameVersion;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i)
    AddAccelerator(i->first);
}

WebUILoginView::~WebUILoginView() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->tray();
  if (tray)
    tray->SetNextFocusableView(NULL);

  if (status_window_)
    status_window_->CloseNow();
  status_window_ = NULL;
}

void WebUILoginView::Init(views::Widget* login_window) {
  login_window_ = login_window;
  webui_login_ = new DOMView();
  AddChildView(webui_login_);
  webui_login_->Init(ProfileManager::GetDefaultProfile(), NULL);
  webui_login_->SetVisible(true);

  WebContents* web_contents = webui_login_->dom_contents()->web_contents();
  web_contents->SetDelegate(this);

  tab_watcher_.reset(new TabRenderWatcher(web_contents, this));
}

std::string WebUILoginView::GetClassName() const {
  return kViewClassName;
}

bool WebUILoginView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  AccelMap::const_iterator entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;

  if (!webui_login_)
    return true;

  content::WebUI* web_ui = GetWebUI();
  if (web_ui) {
    base::StringValue accel_name(entry->second);
    web_ui->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                   accel_name);
  }

  return true;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::OnWindowCreated() {
}

void WebUILoginView::UpdateWindowType() {
}

void WebUILoginView::LoadURL(const GURL & url) {
  webui_login_->LoadURL(url);
  webui_login_->RequestFocus();
}

content::WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->dom_contents()->web_contents()->GetWebUI();
}

void WebUILoginView::OpenProxySettings() {
  ProxySettingsDialog* dialog =
      new ProxySettingsDialog(NULL, GetNativeWindow());
  dialog->Show();
}

void WebUILoginView::SetStatusAreaEnabled(bool enable) {
  if (status_area_)
    status_area_->MakeButtonsActive(enable);
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  ash::SystemTray* tray = ash::Shell::GetInstance()->tray();
  if (tray)
    tray->SetVisible(visible);
  if (status_area_)
    status_area_->SetVisible(visible);
  else
    status_area_visibility_on_init_ = visible;
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  webui_login_->SetBoundsRect(bounds());
}

void WebUILoginView::OnLocaleChanged() {
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void WebUILoginView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->dom_contents()->web_contents()->
      FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

// Overridden from StatusAreaButton::Delegate:

bool WebUILoginView::ShouldExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) const {
  if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS)
    return true;
  return false;
}

void WebUILoginView::ExecuteStatusAreaCommand(
    const views::View* button_view, int command_id) {
  if (command_id == StatusAreaButton::Delegate::SHOW_NETWORK_OPTIONS)
    OpenProxySettings();
}

StatusAreaButton::TextStyle WebUILoginView::GetStatusAreaTextStyle() const {
  return StatusAreaButton::GRAY_PLAIN_LIGHT;
}

void WebUILoginView::ButtonVisibilityChanged(views::View* button_view) {
  if (status_area_)
    status_area_->UpdateButtonVisibility();
}

void WebUILoginView::OnRenderHostCreated(RenderViewHost* host) {
  new SnifferObserver(host, GetWebUI());
}

void WebUILoginView::OnTabMainFrameLoaded() {
  VLOG(1) << "WebUI login main frame loaded.";
}

void WebUILoginView::OnTabMainFrameRender() {
  if (!login_page_is_loaded_)
    return;

  VLOG(1) << "WebUI login main frame rendered.";
  tab_watcher_.reset();

  StatusAreaViewChromeos::SetScreenMode(GetScreenMode());
  // In aura there's a global status area shown already.
  status_area_ = ChromeShellDelegate::instance()->GetStatusArea();
  status_area_->SetVisible(status_area_visibility_on_init_);

  if (should_emit_login_prompt_visible_) {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        EmitLoginPromptVisible();
  }

  OobeUI* oobe_ui = static_cast<OobeUI*>(GetWebUI()->GetController());
  // Notify OOBE that the login frame has been rendered. Currently
  // this is used to start camera presence check.
  oobe_ui->OnLoginPromptVisible();
}

void WebUILoginView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  DCHECK(status_window_ == NULL);
  StatusAreaViewChromeos* status_area_chromeos = new StatusAreaViewChromeos();
  status_area_chromeos->Init(this);
  status_area_ = status_area_chromeos;
  status_area_->SetVisible(status_area_visibility_on_init_);

  // Width of |status_window| is meant to be large enough.
  // The current value of status_area_->GetPreferredSize().width()
  // will be too small when button status is changed.
  // (e.g. when CapsLock indicator appears)
  gfx::Size widget_size(width()/2,
                        status_area_->GetPreferredSize().height());
  const int widget_x = base::i18n::IsRTL() ?
      kStatusAreaCornerPadding :
      width() - widget_size.width() - kStatusAreaCornerPadding;
  gfx::Rect widget_bounds(widget_x, kStatusAreaCornerPadding,
                          widget_size.width(), widget_size.height());
  // TODO(nkostylev|oshima): Make status area in the same window as
  // |webui_login_| once RenderWidgetHostViewViews and compositor are
  // ready. This will also avoid having to override the status area
  // widget type for the lock screen.
  views::Widget::InitParams widget_params(GetStatusAreaWidgetType());
  widget_params.bounds = widget_bounds;
  widget_params.transparent = true;
  widget_params.parent_widget = login_window_;
  status_window_ = new views::Widget;
  status_window_->Init(widget_params);

  views::View* contents_view = new RightAlignedView;
  contents_view->AddChildView(status_area_);
  status_window_->SetContentsView(contents_view);
  status_window_->Show();
}

StatusAreaViewChromeos::ScreenMode WebUILoginView::GetScreenMode() {
  return StatusAreaViewChromeos::LOGIN_MODE_WEBUI;
}

views::Widget::InitParams::Type WebUILoginView::GetStatusAreaWidgetType() {
  return views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

void WebUILoginView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_WEBUI_READY:
    case chrome::NOTIFICATION_LOGIN_USER_IMAGES_LOADED:
    case chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN:
    case chrome::NOTIFICATION_WIZARD_FIRST_SCREEN_SHOWN:
      login_page_is_loaded_ = true;
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Do not show the context menu.
#ifndef NDEBUG
  return false;
#else
  return true;
#endif
}

void WebUILoginView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());

  // Make sure error bubble is cleared on keyboard event. This is needed
  // when the focus is inside an iframe. Only clear on KeyDown to prevent hiding
  // an immediate authentication error (See crbug.com/103643).
  if (event.type == WebKit::WebInputEvent::KeyDown) {
    content::WebUI* web_ui = GetWebUI();
    if (web_ui)
      web_ui->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
  }
}

bool WebUILoginView::IsPopupOrPanel(const WebContents* source) const {
  return true;
}

bool WebUILoginView::TakeFocus(bool reverse) {
  if (status_area_ && status_area_->visible()) {
    // Forward the focus to the status area.
    base::Callback<void(bool)> return_focus_cb =
        base::Bind(&WebUILoginView::ReturnFocus, base::Unretained(this));
    status_area_->TakeFocus(reverse, return_focus_cb);
    status_area_->GetWidget()->Activate();

    ash::SystemTray* tray = ash::Shell::GetInstance()->tray();
    if (tray)
      tray->SetNextFocusableView(this);
  }
  return true;
}

void WebUILoginView::ReturnFocus(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->dom_contents()->web_contents()->
      FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

}  // namespace chromeos
