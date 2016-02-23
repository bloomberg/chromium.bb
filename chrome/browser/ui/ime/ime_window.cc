// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ime/ime_window.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ime/ime_native_window.h"
#include "chrome/browser/ui/ime/ime_window_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/image/image.h"

namespace ui {

ImeWindow::ImeWindow(Profile* profile,
                     const extensions::Extension* extension,
                     const std::string& url,
                     Mode mode,
                     const gfx::Rect& bounds)
    : mode_(mode), native_window_(nullptr) {
  if (extension) {  // Allow nullable |extension| for testability.
    title_ = extension->name();
    icon_.reset(new extensions::IconImage(
        profile, extension, extensions::IconsInfo::GetIcons(extension),
        extension_misc::EXTENSION_ICON_SMALL, gfx::ImageSkia(), this));
  }

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  GURL gurl(url);
  if (!gurl.is_valid())
    gurl = extension->GetResourceURL(url);

  content::SiteInstance* instance =
      content::SiteInstance::CreateForURL(profile, gurl);
  content::WebContents::CreateParams create_params(profile, instance);
  web_contents_.reset(content::WebContents::Create(create_params));
  web_contents_->SetDelegate(this);
  content::OpenURLParams params(gurl, content::Referrer(), SINGLETON_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);

  native_window_ = CreateNativeWindow(this, bounds, web_contents_.get());
}

void ImeWindow::Show() {
  native_window_->Show();
}

void ImeWindow::Hide() {
  native_window_->Hide();
}

void ImeWindow::Close() {
  native_window_->Close();
}

void ImeWindow::SetBounds(const gfx::Rect& bounds) {
  native_window_->SetBounds(bounds);
}

int ImeWindow::GetFrameId() const {
  return web_contents_->GetMainFrame()->GetRoutingID();
}

void ImeWindow::OnWindowDestroyed() {
  FOR_EACH_OBSERVER(ImeWindowObserver, observers_, OnWindowDestroyed(this));
  native_window_ = nullptr;
  delete this;
  // TODO(shuchen): manages the ime window instances in ImeWindowManager.
  // e.g. for normal window, limits the max window count, and for follow cursor
  // window, limits single window instance (and reuse).
  // So at here it will callback to ImeWindowManager to delete |this|.
}

void ImeWindow::AddObserver(ImeWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void ImeWindow::OnExtensionIconImageChanged(extensions::IconImage* image) {
  if (native_window_)
    native_window_->UpdateWindowIcon();
}

ImeWindow::~ImeWindow() {}

void ImeWindow::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_TERMINATING)
    Close();
}

content::WebContents* ImeWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  source->GetController().LoadURL(params.url, params.referrer,
                                  params.transition, params.extra_headers);
  return source;
}

bool ImeWindow::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  return false;
}

void ImeWindow::CloseContents(content::WebContents* source) {
  Close();
}

void ImeWindow::MoveContents(content::WebContents* source,
                                 const gfx::Rect& pos) {
  if (native_window_)
    native_window_->SetBounds(pos);
}

bool ImeWindow::IsPopupOrPanel(const content::WebContents* source) const {
  return true;
}

}  // namespace ui
