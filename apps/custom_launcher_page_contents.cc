// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/custom_launcher_page_contents.h"

#include <string>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/app_window/app_web_contents_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_messages.h"

namespace apps {

CustomLauncherPageContents::CustomLauncherPageContents(
    scoped_ptr<extensions::AppDelegate> app_delegate,
    const std::string& extension_id)
    : app_delegate_(app_delegate.Pass()), extension_id_(extension_id) {
}

CustomLauncherPageContents::~CustomLauncherPageContents() {
}

void CustomLauncherPageContents::Initialize(content::BrowserContext* context,
                                            const GURL& url) {
  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(context, this));

  web_contents_.reset(
      content::WebContents::Create(content::WebContents::CreateParams(
          context, content::SiteInstance::CreateForURL(context, url))));

  Observe(web_contents());
  web_contents_->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  helper_.reset(new extensions::AppWebContentsHelper(
      context, extension_id_, web_contents_.get(), app_delegate_.get()));
  web_contents_->SetDelegate(this);

  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_LAUNCHER_PAGE);

  // This observer will activate the extension when it is navigated to, which
  // allows Dispatcher to give it the proper context and makes it behave like an
  // extension.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents());

  web_contents_->GetController().LoadURL(url,
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                         std::string());
}

content::WebContents* CustomLauncherPageContents::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK_EQ(web_contents_.get(), source);
  return helper_->OpenURLFromTab(params);
}

void CustomLauncherPageContents::AddNewContents(
    content::WebContents* source,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  app_delegate_->AddNewContents(new_contents->GetBrowserContext(),
                                new_contents,
                                disposition,
                                initial_pos,
                                user_gesture,
                                was_blocked);
}

bool CustomLauncherPageContents::IsPopupOrPanel(
    const content::WebContents* source) const {
  return true;
}

bool CustomLauncherPageContents::ShouldSuppressDialogs() {
  return true;
}

bool CustomLauncherPageContents::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  return extensions::AppWebContentsHelper::ShouldSuppressGestureEvent(event);
}

content::ColorChooser* CustomLauncherPageContents::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestionss) {
  return app_delegate_->ShowColorChooser(web_contents, initial_color);
}

void CustomLauncherPageContents::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  app_delegate_->RunFileChooser(tab, params);
}

void CustomLauncherPageContents::RequestToLockMouse(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  helper_->RequestToLockMouse();
}

void CustomLauncherPageContents::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  helper_->RequestMediaAccessPermission(request, callback);
}

bool CustomLauncherPageContents::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  return helper_->CheckMediaAccessPermission(security_origin, type);
}

bool CustomLauncherPageContents::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CustomLauncherPageContents, message)
  IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

extensions::WindowController*
CustomLauncherPageContents::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* CustomLauncherPageContents::GetAssociatedWebContents()
    const {
  return web_contents();
}

void CustomLauncherPageContents::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(params,
                                           web_contents_->GetRenderViewHost());
}

}  // namespace apps
