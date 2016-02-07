// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_message.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

ConstrainedWebDialogDelegateBase::ConstrainedWebDialogDelegateBase(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : WebDialogWebContentsDelegate(browser_context,
                                   new ChromeWebContentsHandler),
      web_dialog_delegate_(delegate),
      closed_via_webui_(false),
      release_contents_on_close_(false) {
  CHECK(delegate);
  web_contents_.reset(
      WebContents::Create(WebContents::CreateParams(browser_context)));
  ui_zoom::ZoomController::CreateForWebContents(web_contents_.get());
  if (tab_delegate) {
    override_tab_delegate_.reset(tab_delegate);
    web_contents_->SetDelegate(tab_delegate);
  } else {
    web_contents_->SetDelegate(this);
  }
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, Profile::FromBrowserContext(browser_context), web_contents_.get());

  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  // Set |this| as a delegate so the ConstrainedWebDialogUI can retrieve it.
  ConstrainedWebDialogUI::SetConstrainedDelegate(web_contents_.get(), this);

  web_contents_->GetController().LoadURL(delegate->GetDialogContentURL(),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                         std::string());
}

ConstrainedWebDialogDelegateBase::~ConstrainedWebDialogDelegateBase() {
  if (release_contents_on_close_)
    ignore_result(web_contents_.release());
}

const WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() const {
  return web_dialog_delegate_.get();
}

WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() {
  return web_dialog_delegate_.get();
}

void ConstrainedWebDialogDelegateBase::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  CloseContents(web_contents_.get());
}

bool ConstrainedWebDialogDelegateBase::closed_via_webui() const {
  return closed_via_webui_;
}

void ConstrainedWebDialogDelegateBase::ReleaseWebContentsOnDialogClose() {
  release_contents_on_close_ = true;
}

gfx::NativeWindow ConstrainedWebDialogDelegateBase::GetNativeDialog() {
  NOTREACHED();
  return NULL;
}

WebContents* ConstrainedWebDialogDelegateBase::GetWebContents() {
  return web_contents_.get();
}

void ConstrainedWebDialogDelegateBase::HandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event) {
}

gfx::Size ConstrainedWebDialogDelegateBase::GetMinimumSize() const {
  NOTREACHED();
  return gfx::Size();
}

gfx::Size ConstrainedWebDialogDelegateBase::GetMaximumSize() const {
  NOTREACHED();
  return gfx::Size();
}

gfx::Size ConstrainedWebDialogDelegateBase::GetPreferredSize() const {
  NOTREACHED();
  return gfx::Size();
}
