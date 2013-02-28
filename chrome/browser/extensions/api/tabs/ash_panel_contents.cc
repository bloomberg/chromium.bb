// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/ash_panel_contents.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"

// AshPanelWindowController ----------------------------------------------------

// This class enables a ShellWindow instance to be accessed (to a limited
// extent) via the chrome.windows and chrome.tabs API. This is a temporary
// bridge to support instantiating ShellWindows from v1 apps, specifically
// for creating Panels in Ash. See crbug.com/160645.
class AshPanelWindowController : public extensions::WindowController {
 public:
  AshPanelWindowController(ShellWindow* window, Profile* profile);
  virtual ~AshPanelWindowController();

  void NativeWindowChanged();

  // Overridden from extensions::WindowController.
  virtual int GetWindowId() const OVERRIDE;
  virtual std::string GetWindowTypeText() const OVERRIDE;
  virtual base::DictionaryValue* CreateWindowValueWithTabs(
      const extensions::Extension* extension) const OVERRIDE;
  virtual base::DictionaryValue* CreateTabValue(
      const extensions::Extension* extension, int tab_index) const OVERRIDE;
  virtual bool CanClose(Reason* reason) const OVERRIDE;
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const OVERRIDE;
  virtual bool IsVisibleToExtension(
      const extensions::Extension* extension) const OVERRIDE;

 private:
  ShellWindow* shell_window_;  // Weak pointer; this is owned by shell_window_
  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(AshPanelWindowController);
};

AshPanelWindowController::AshPanelWindowController(
    ShellWindow* shell_window, Profile* profile)
    : extensions::WindowController(shell_window->GetBaseWindow(), profile),
      shell_window_(shell_window),
      is_active_(shell_window->GetBaseWindow()->IsActive()) {
  extensions::WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

AshPanelWindowController::~AshPanelWindowController() {
  extensions::WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int AshPanelWindowController::GetWindowId() const {
  return static_cast<int>(shell_window_->session_id().id());
}

std::string AshPanelWindowController::GetWindowTypeText() const {
  return extensions::tabs_constants::kWindowTypeValuePanel;
}

base::DictionaryValue* AshPanelWindowController::CreateWindowValueWithTabs(
    const extensions::Extension* extension) const {
  DCHECK(IsVisibleToExtension(extension));
  base::DictionaryValue* result = CreateWindowValue();
  DictionaryValue* tab_value = CreateTabValue(extension, 0);
  if (tab_value) {
    base::ListValue* tab_list = new ListValue();
    tab_list->Append(tab_value);
    result->Set(extensions::tabs_constants::kTabsKey, tab_list);
  }
  return result;
}

base::DictionaryValue* AshPanelWindowController::CreateTabValue(
    const extensions::Extension* extension, int tab_index) const {
  if ((extension && !IsVisibleToExtension(extension)) ||
      (tab_index > 0)) {
    return NULL;
  }
  content::WebContents* web_contents = shell_window_->web_contents();
  if (!web_contents)
    return NULL;

  DictionaryValue* tab_value = new DictionaryValue();
  tab_value->SetInteger(extensions::tabs_constants::kIdKey,
                        SessionID::IdForTab(web_contents));
  tab_value->SetInteger(extensions::tabs_constants::kIndexKey, 0);
  const int window_id = GetWindowId();
  tab_value->SetInteger(extensions::tabs_constants::kWindowIdKey, window_id);
  tab_value->SetString(
      extensions::tabs_constants::kUrlKey, web_contents->GetURL().spec());
  tab_value->SetString(
      extensions::tabs_constants::kStatusKey,
      ExtensionTabUtil::GetTabStatusText(web_contents->IsLoading()));
  tab_value->SetBoolean(
      extensions::tabs_constants::kActiveKey,
      shell_window_->GetBaseWindow()->IsActive());
  // ShellWindow only ever contains one tab, so that tab is always effectively
  // selcted and highlighted (for purposes of the chrome.tabs API).
  tab_value->SetInteger(extensions::tabs_constants::kWindowIdKey, window_id);
  tab_value->SetInteger(extensions::tabs_constants::kIdKey, window_id);
  tab_value->SetBoolean(extensions::tabs_constants::kSelectedKey, true);
  tab_value->SetBoolean(extensions::tabs_constants::kHighlightedKey, true);
  tab_value->SetBoolean(extensions::tabs_constants::kPinnedKey, false);
  tab_value->SetString(
      extensions::tabs_constants::kTitleKey, web_contents->GetTitle());
  tab_value->SetBoolean(
      extensions::tabs_constants::kIncognitoKey,
      web_contents->GetBrowserContext()->IsOffTheRecord());
  return tab_value;
}

bool AshPanelWindowController::CanClose(Reason* reason) const {
  return true;
}

void AshPanelWindowController::SetFullscreenMode(
    bool is_fullscreen, const GURL& extension_url) const {
  // Do nothing. Panels cannot be fullscreen.
}

bool AshPanelWindowController::IsVisibleToExtension(
    const extensions::Extension* extension) const {
  return shell_window_->extension() &&
      extension->id() == shell_window_->extension()->id();
}

void AshPanelWindowController::NativeWindowChanged() {
  bool active = shell_window_->GetBaseWindow()->IsActive();
  if (active == is_active_)
    return;
  is_active_ = active;
  // Let the extension API know that the active window changed.
  extensions::TabsWindowsAPI* tabs_windows_api =
      extensions::TabsWindowsAPI::Get(profile());
  if (!tabs_windows_api)
    return;
  tabs_windows_api->windows_event_router()->OnActiveWindowChanged(
      active ? this : NULL);
}

// AshPanelContents -----------------------------------------------------

AshPanelContents::AshPanelContents(ShellWindow* host)
    : host_(host) {
}

AshPanelContents::~AshPanelContents() {
}

void AshPanelContents::Initialize(Profile* profile, const GURL& url) {
  url_ = url;

  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(profile, this));

  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(
          profile, content::SiteInstance::CreateForURL(profile, url_))));

  // Needed to give the web contents a Window ID. Extension APIs expect web
  // contents to have a Window ID. Also required for FaviconTabHelper to
  // correctly set the window icon and title.
  SessionTabHelper::CreateForWebContents(web_contents_.get());
  SessionTabHelper::FromWebContents(web_contents_.get())->SetWindowID(
      host_->session_id());

  // Responsible for loading favicons for the Launcher, which uses different
  // logic than the FaviconTabHelper associated with web_contents_
  // (instantiated in ShellWindow::Init())
  launcher_favicon_loader_.reset(
      new LauncherFaviconLoader(this, web_contents_.get()));

  content::WebContentsObserver::Observe(web_contents_.get());
}

void AshPanelContents::LoadContents(int32 creator_process_id) {
  // This must be created after the native window has been created.
  window_controller_.reset(
      new AshPanelWindowController(host_, host_->profile()));

  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
}

void AshPanelContents::NativeWindowChanged(NativeAppWindow* native_app_window) {
  if (window_controller_)
    window_controller_->NativeWindowChanged();
}

void AshPanelContents::NativeWindowClosed() {
}

content::WebContents* AshPanelContents::GetWebContents() const {
  return web_contents_.get();
}

void AshPanelContents::FaviconUpdated() {
  gfx::Image new_image = gfx::Image::CreateFrom1xBitmap(
      launcher_favicon_loader_->GetFavicon());
  host_->UpdateAppIcon(new_image);
}

bool AshPanelContents::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AshPanelContents, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

extensions::WindowController*
AshPanelContents::GetExtensionWindowController() const {
  return window_controller_.get();
}

content::WebContents* AshPanelContents::GetAssociatedWebContents() const {
  return web_contents_.get();
}

void AshPanelContents::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, web_contents_->GetRenderViewHost());
}
