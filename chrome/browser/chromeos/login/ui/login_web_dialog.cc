// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"

#include <deque>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const double kDefaultWidthRatio = 0.6;
const double kDefaultHeightRatio = 0.6;

// Default width/height ratio of minimal dialog size.
const double kMinimumWidthRatio = 0.25;
const double kMinimumHeightRatio = 0.25;

static base::LazyInstance<std::deque<content::WebContents*> >
    g_web_contents_stack = LAZY_INSTANCE_INITIALIZER;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, public:

void LoginWebDialog::Delegate::OnDialogClosed() {
}

LoginWebDialog::LoginWebDialog(content::BrowserContext* browser_context,
                               Delegate* delegate,
                               gfx::NativeWindow parent_window,
                               const base::string16& title,
                               const GURL& url,
                               Style style)
    : browser_context_(browser_context),
      parent_window_(parent_window),
      delegate_(delegate),
      title_(title),
      url_(url),
      style_(style),
      is_open_(false) {
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  width_ = static_cast<int>(kDefaultWidthRatio * screen_bounds.width());
  height_ = static_cast<int>(kDefaultHeightRatio * screen_bounds.height());
}

LoginWebDialog::~LoginWebDialog() {
  delegate_ = NULL;
}

void LoginWebDialog::Show() {
  chrome::ShowWebDialog(parent_window_, browser_context_, this);
  is_open_ = true;
}

void LoginWebDialog::SetDialogSize(int width, int height) {
  DCHECK(width >= 0 && height >= 0);
  width_ = width;
  height_ = height;
}

void LoginWebDialog::SetDialogTitle(const base::string16& title) {
  title_ = title;
}

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, protected:

ui::ModalType LoginWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 LoginWebDialog::GetDialogTitle() const {
  return title_;
}

GURL LoginWebDialog::GetDialogContentURL() const {
  return url_;
}

void LoginWebDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void LoginWebDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(width_, height_);
}

void LoginWebDialog::GetMinimumDialogSize(gfx::Size* size) const {
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  size->SetSize(kMinimumWidthRatio * screen_bounds.width(),
                kMinimumHeightRatio * screen_bounds.height());
}

std::string LoginWebDialog::GetDialogArgs() const {
  return std::string();
}

// static.
content::WebContents* LoginWebDialog::GetCurrentWebContents() {
  if (!g_web_contents_stack.Pointer()->size())
    return NULL;

  return g_web_contents_stack.Pointer()->front();
}

void LoginWebDialog::OnDialogShown(content::WebUI* webui,
                                   content::RenderViewHost* render_view_host) {
  g_web_contents_stack.Pointer()->push_front(webui->GetWebContents());
}

void LoginWebDialog::OnDialogClosed(const std::string& json_retval) {
  is_open_ = false;
  notification_registrar_.RemoveAll();
  if (delegate_)
    delegate_->OnDialogClosed();
  delete this;
}

void LoginWebDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;

  if (g_web_contents_stack.Pointer()->size() &&
      source == g_web_contents_stack.Pointer()->front()) {
    g_web_contents_stack.Pointer()->pop_front();
  } else {
    NOTREACHED();
  }
}

bool LoginWebDialog::ShouldShowDialogTitle() const {
  return true;
}

bool LoginWebDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void LoginWebDialog::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  // TODO(saintlou): Do we need a throbber for Aura?
  NOTIMPLEMENTED();
}

}  // namespace chromeos
