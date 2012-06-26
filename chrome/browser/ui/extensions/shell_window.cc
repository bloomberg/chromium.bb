// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/shell_window.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/renderer_preferences.h"

using content::BrowserThread;
using content::ConsoleMessageLevel;
using content::RenderViewHost;
using content::ResourceDispatcherHost;
using content::SiteInstance;
using content::WebContents;

namespace {
const int kDefaultWidth = 512;
const int kDefaultHeight = 384;

void SuspendRenderViewHost(RenderViewHost* rvh) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourceDispatcherHost::BlockRequestsForRoute,
          base::Unretained(ResourceDispatcherHost::Get()),
          rvh->GetProcess()->GetID(), rvh->GetRoutingID()));
}

}  // namespace

ShellWindow::CreateParams::CreateParams()
  : frame(ShellWindow::CreateParams::FRAME_CHROME),
    bounds(10, 10, kDefaultWidth, kDefaultHeight) {
}

ShellWindow* ShellWindow::Create(Profile* profile,
                                 const extensions::Extension* extension,
                                 const GURL& url,
                                 const ShellWindow::CreateParams& params) {
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

  // Block the created RVH from loading anything until the background page
  // has had a chance to do any initialization it wants.
  SuspendRenderViewHost(web_contents_->GetRenderViewHost());

  // TODO(jeremya): there's a bug where navigating a web contents to an
  // extension URL causes it to create a new RVH and discard the old (perfectly
  // usable) one. To work around this, we watch for a RVH_CHANGED message from
  // the web contents (which will be sent during LoadURL) and suspend resource
  // requests on the new RVH to ensure that we block the new RVH from loading
  // anything. It should be okay to remove the NOTIFICATION_RVH_CHANGED
  // registration once http://crbug.com/123007 is fixed.
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &web_contents_->GetController()));
  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  registrar_.RemoveAll();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // Close when the browser is exiting.
  // TODO(mihaip): we probably don't want this in the long run (when platform
  // apps are no longer tied to the browser process).
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Automatically dismiss all infobars.
  TabContents* tab_contents = TabContents::FromWebContents(web_contents_);
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  infobar_helper->set_infobars_enabled(false);

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

void ShellWindow::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
  content::MediaStreamDevices devices;

  content::MediaStreamDeviceMap::const_iterator iter =
      request->devices.find(content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE);
  if (iter != request->devices.end() &&
      extension()->HasAPIPermission(ExtensionAPIPermission::kAudioCapture) &&
      !iter->second.empty()) {
    devices.push_back(iter->second[0]);
  }

  iter = request->devices.find(content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE);
  if (iter != request->devices.end() &&
      extension()->HasAPIPermission(ExtensionAPIPermission::kVideoCapture) &&
      !iter->second.empty()) {
    devices.push_back(iter->second[0]);
  }

  callback.Run(devices);
}

WebContents* ShellWindow::OpenURLFromTab(WebContents* source,
                                         const content::OpenURLParams& params) {
  DCHECK(source == web_contents_);

  if (params.url.host() == extension_->id()) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
    return NULL;
  }

  // Don't allow the current tab to be navigated. It would be nice to map all
  // anchor tags (even those without target="_blank") to new tabs, but right
  // now we can't distinguish between those and <meta> refreshes, which we
  // don't want to allow.
  // TOOD(mihaip): Can we check for user gestures instead?
  WindowOpenDisposition disposition = params.disposition;
  if (disposition == CURRENT_TAB) {
    AddMessageToDevToolsConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        base::StringPrintf(
            "Can't open same-window link to \"%s\"; try target=\"_blank\".",
            params.url.spec().c_str()));
    return NULL;
  }

  // These dispositions aren't really navigations.
  if (disposition == SUPPRESS_OPEN || disposition == SAVE_TO_DISK ||
      disposition == IGNORE_ACTION) {
    return NULL;
  }

  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  content::OpenURLParams new_tab_params = params;
  new_tab_params.disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  Browser* browser = browser::FindOrCreateTabbedBrowser(profile_);
  WebContents* new_tab = browser->OpenURL(new_tab_params);
  browser->window()->Show();
  return new_tab;
}

void ShellWindow::AddNewContents(WebContents* source,
                                 WebContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  DCHECK(source == web_contents_);
  DCHECK(Profile::FromBrowserContext(new_contents->GetBrowserContext()) ==
      profile_);
  Browser* browser = browser::FindOrCreateTabbedBrowser(profile_);
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  browser->AddWebContents(
      new_contents, disposition, initial_pos, user_gesture);
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
    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      // TODO(jeremya): once http://crbug.com/123007 is fixed, we'll no longer
      // need to suspend resource requests here (the call in the constructor
      // should be enough).
      content::Details<std::pair<RenderViewHost*, RenderViewHost*> >
          host_details(details);
      SuspendRenderViewHost(host_details->second);
      break;
    }
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

void ShellWindow::AddMessageToDevToolsConsole(ConsoleMessageLevel level,
                                              const std::string& message) {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AddMessageToConsole(
      rvh->GetRoutingID(), level, message));
}
