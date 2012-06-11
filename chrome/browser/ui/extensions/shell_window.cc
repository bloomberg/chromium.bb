// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/shell_window.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/renderer_preferences.h"

using content::SiteInstance;
using content::WebContents;

namespace {
static const int kDefaultWidth = 512;
static const int kDefaultHeight = 384;
}  // namespace

ShellWindow::CreateParams::CreateParams()
  : frame(ShellWindow::CreateParams::FRAME_CHROME),
    bounds(10, 10, kDefaultWidth, kDefaultHeight) {
}

ShellWindow* ShellWindow::Create(Profile* profile,
                                 const extensions::Extension* extension,
                                 const GURL& url,
                                 const ShellWindow::CreateParams params) {
  // This object will delete itself when the window is closed.
  ShellWindow* window =
      ShellWindow::CreateImpl(profile, extension, url, params);
  ShellWindowRegistry::Get(profile)->AddShellWindow(window);
  return window;
}

ShellWindow::ShellWindow(Profile* profile,
                         const extensions::Extension* extension,
                         const GURL& url)
    : profile_(profile),
      extension_(extension),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile, this)) {
  // TODO(jeremya) this should all be done in an Init() method, not in the
  // constructor. During this code, WebContents will be calling
  // WebContentsDelegate methods, but at this point the vftables for the
  // subclass are not yet in place, since it's still halfway through its
  // constructor. As a result, overridden virtual methods won't be called.
  web_contents_ = WebContents::Create(
      profile, SiteInstance::CreateForURL(profile, url), MSG_ROUTING_NONE, NULL,
      NULL);
  contents_.reset(new TabContents(web_contents_));
  content::WebContentsObserver::Observe(web_contents_);
  web_contents_->SetDelegate(this);
  chrome::SetViewType(web_contents_, chrome::VIEW_TYPE_APP_SHELL);
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // Close when the browser is exiting.
  // TODO(mihaip): we probably don't want this in the long run (when platform
  // apps are no longer tied to the browser process).
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Prevent the browser process from shutting down while this window is open.
  browser::StartKeepAlive();
}

ShellWindow::~ShellWindow() {
  // Unregister now to prevent getting NOTIFICATION_APP_TERMINATING if we're the
  // last window open.
  registrar_.RemoveAll();

  // Remove shutdown prevention.
  browser::EndKeepAlive();
}

bool ShellWindow::IsFullscreenOrPending() const {
  return false;
}

void ShellWindow::OnNativeClose() {
  ShellWindowRegistry::Get(profile_)->RemoveShellWindow(this);
  delete this;
}

string16 ShellWindow::GetTitle() const {
  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  if (!web_contents()->GetController().GetActiveEntry() ||
      web_contents()->GetController().GetActiveEntry()->GetTitle().empty())
    return UTF8ToUTF16(extension()->name());
  return web_contents()->GetTitle();
}

bool ShellWindow::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellWindow, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShellWindow::CloseContents(WebContents* contents) {
  Close();
}

bool ShellWindow::ShouldSuppressDialogs() {
  return true;
}

void ShellWindow::WebIntentDispatch(
    content::WebContents* web_contents,
    content::WebIntentsDispatcher* intents_dispatcher) {
  if (!web_intents::IsWebIntentsEnabledForProfile(profile_))
    return;

  contents_->web_intent_picker_controller()->SetIntentsDispatcher(
      intents_dispatcher);
  contents_->web_intent_picker_controller()->ShowDialog(
      intents_dispatcher->GetIntent().action,
      intents_dispatcher->GetIntent().type);
}

void ShellWindow::RunFileChooser(WebContents* tab,
                                 const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

bool ShellWindow::IsPopupOrPanel(const WebContents* source) const {
  DCHECK(source == web_contents_);
  return true;
}

void ShellWindow::MoveContents(WebContents* source, const gfx::Rect& pos) {
  DCHECK(source == web_contents_);
  SetBounds(pos);
}

void ShellWindow::NavigationStateChanged(
    const content::WebContents* source, unsigned changed_flags) {
  DCHECK(source == web_contents_);
  if (changed_flags & content::INVALIDATE_TYPE_TITLE)
    UpdateWindowTitle();
}

void ShellWindow::ToggleFullscreenModeForTab(content::WebContents* source,
                                             bool enter_fullscreen) {
  DCHECK(source == web_contents_);
  SetFullscreen(enter_fullscreen);
}

bool ShellWindow::IsFullscreenForTabOrPending(
    const content::WebContents* source) const {
  DCHECK(source == web_contents_);
  return IsFullscreenOrPending();
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::Extension* unloaded_extension =
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension;
      if (extension_ == unloaded_extension)
        Close();
      break;
    }
    case content::NOTIFICATION_APP_TERMINATING:
      Close();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

ExtensionWindowController* ShellWindow::GetExtensionWindowController() const {
  return NULL;
}

void ShellWindow::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_.Dispatch(params,
                                          web_contents_->GetRenderViewHost());
}
