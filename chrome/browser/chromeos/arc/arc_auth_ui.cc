// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_ui.h"

#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_states.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

const int kDefaultWidth = 480;
const int kDefaultHeight = 640;

}  // namespace

ArcAuthUI::ArcAuthUI(content::BrowserContext* browser_context,
                     Delegate* delegate)
    : browser_context_(browser_context),
      delegate_(delegate),
      target_url_(ArcAuthFetcher::CreateURL()) {
  DCHECK(browser_context_ && delegate_);
  views::WebDialogView* view = new views::WebDialogView(
      browser_context_, this, new ChromeWebContentsHandler);
  widget_ = views::Widget::CreateWindow(view);
  widget_->Show();
}

ArcAuthUI::~ArcAuthUI() {}

void ArcAuthUI::Close() {
  widget_->CloseNow();
}

ui::ModalType ArcAuthUI::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 ArcAuthUI::GetDialogTitle() const {
  return base::string16();
}

GURL ArcAuthUI::GetDialogContentURL() const {
  return target_url_;
}

void ArcAuthUI::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void ArcAuthUI::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string ArcAuthUI::GetDialogArgs() const {
  return std::string();
}

void ArcAuthUI::OnDialogClosed(const std::string& json_retval) {
  delegate_->OnAuthUIClosed();
  delete this;
}

void ArcAuthUI::OnCloseContents(content::WebContents* source,
                                bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool ArcAuthUI::ShouldShowDialogTitle() const {
  return false;
}

bool ArcAuthUI::HandleContextMenu(const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void ArcAuthUI::OnLoadingStateChanged(content::WebContents* source) {
  if (source->IsLoading())
    return;

  // Check if current page may contain required cookies and skip intermediate
  // steps.
  const GURL& current_url = source->GetVisibleURL();
  if (target_url_.GetOrigin() != current_url.GetOrigin() ||
      target_url_.path() != current_url.path()) {
    return;
  }

  auth_fetcher_.reset(
      new ArcAuthFetcher(browser_context_->GetRequestContext(), this));
}

void ArcAuthUI::OnAuthCodeFetched(const std::string& auth_code) {
  ArcAuthService::Get()->SetAuthCodeAndStartArc(auth_code);
}

void ArcAuthUI::OnAuthCodeNeedUI() {}

void ArcAuthUI::OnAuthCodeFailed() {}

}  // namespace arc
