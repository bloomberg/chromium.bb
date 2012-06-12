// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/views/web_dialog_view.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const int kBaseWidth = 1252;
const int kBaseHeight = 516;
const int kHorizontalMargin = 28;

// A message handler for detecting the timing when the web contents is painted.
class PaintMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<PaintMessageHandler> {
 public:
  explicit PaintMessageHandler(views::Widget* widget) : widget_(widget) {}
  virtual ~PaintMessageHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void DidPaint(const ListValue* args);

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(PaintMessageHandler);
};

void PaintMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "didPaint",
      base::Bind(&PaintMessageHandler::DidPaint, base::Unretained(this)));
}

void PaintMessageHandler::DidPaint(const ListValue* args) {
  // Show the widget after the web content has been painted.
  widget_->Show();
}

}  // namespace

KeyboardOverlayDelegate::KeyboardOverlayDelegate(const string16& title)
    : title_(title),
      view_(NULL) {
}

KeyboardOverlayDelegate::~KeyboardOverlayDelegate() {
}

void KeyboardOverlayDelegate::Show(WebDialogView* view) {
  view_ = view;

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view;
  widget->Init(params);

  // Show the widget at the bottom of the work area.
  gfx::Size size;
  GetDialogSize(&size);
  const gfx::Rect& rect = gfx::Screen::GetMonitorNearestWindow(
      widget->GetNativeView()).work_area();
  gfx::Rect bounds((rect.width() - size.width()) / 2,
                   rect.height() - size.height(),
                   size.width(),
                   size.height());
  widget->SetBounds(bounds);

  // The widget will be shown when the web contents gets ready to display.
}

ui::ModalType KeyboardOverlayDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 KeyboardOverlayDelegate::GetDialogTitle() const {
  return title_;
}

GURL KeyboardOverlayDelegate::GetDialogContentURL() const {
  std::string url_string(chrome::kChromeUIKeyboardOverlayURL);
  return GURL(url_string);
}

void KeyboardOverlayDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new PaintMessageHandler(view_->GetWidget()));
}

void KeyboardOverlayDelegate::GetDialogSize(
    gfx::Size* size) const {
  using std::min;
  DCHECK(view_);
  gfx::Rect rect = gfx::Screen::GetMonitorNearestWindow(
      view_->GetWidget()->GetNativeView()).bounds();
  const int width = min(kBaseWidth, rect.width() - kHorizontalMargin);
  const int height = width * kBaseHeight / kBaseWidth;
  size->SetSize(width, height);
}

std::string KeyboardOverlayDelegate::GetDialogArgs() const {
  return "[]";
}

void KeyboardOverlayDelegate::OnDialogClosed(
    const std::string& json_retval) {
  delete this;
  return;
}

void KeyboardOverlayDelegate::OnCloseContents(WebContents* source,
                                              bool* out_close_dialog) {
}

bool KeyboardOverlayDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool KeyboardOverlayDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}
