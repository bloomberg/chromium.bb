// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/web_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/input/public/accelerator_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"

namespace athena {
namespace {

class WebActivityController : public AcceleratorHandler {
 public:
  enum Command {
    CMD_BACK,
    CMD_FORWARD,
    CMD_RELOAD,
    CMD_RELOAD_IGNORE_CACHE,
  };

  explicit WebActivityController(views::WebView* web_view)
      : web_view_(web_view), reserved_accelerator_enabled_(true) {}
  virtual ~WebActivityController() {}

  // Installs accelerators for web activity.
  void InstallAccelerators() {
    accelerator_manager_ = AcceleratorManager::CreateForFocusManager(
                               web_view_->GetFocusManager()).Pass();
    const AcceleratorData accelerator_data[] = {
        {TRIGGER_ON_PRESS, ui::VKEY_R, ui::EF_CONTROL_DOWN, CMD_RELOAD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_REFRESH, ui::EF_NONE, CMD_RELOAD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN,
         CMD_RELOAD_IGNORE_CACHE, AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_FORWARD, ui::EF_NONE, CMD_FORWARD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_BACK, ui::EF_NONE, CMD_BACK,
         AF_NONE},
    };
    accelerator_manager_->RegisterAccelerators(
        accelerator_data, arraysize(accelerator_data), this);
  }

  // Methods that are called before and after key events are consumed by the web
  // contents.
  // See the documentation in WebContentsDelegate: for more details.
  bool PreHandleKeyboardEvent(content::WebContents* source,
                              const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) {
    ui::Accelerator accelerator(
        static_cast<ui::KeyboardCode>(event.windowsKeyCode),
        content::GetModifiersFromNativeWebKeyboardEvent(event));
    if (event.type == blink::WebInputEvent::KeyUp)
      accelerator.set_type(ui::ET_KEY_RELEASED);

    if (reserved_accelerator_enabled_ &&
        accelerator_manager_->IsRegistered(accelerator, AF_RESERVED)) {
      return web_view_->GetFocusManager()->ProcessAccelerator(accelerator);
    }
    *is_keyboard_shortcut =
        accelerator_manager_->IsRegistered(accelerator, AF_NONE);
    return false;
  }

  void HandleKeyboardEvent(content::WebContents* source,
                           const content::NativeWebKeyboardEvent& event) {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, web_view_->GetFocusManager());
  }

 private:
  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE {
    switch (command_id) {
      case CMD_RELOAD:
        return true;
      case CMD_BACK:
        return web_view_->GetWebContents()->GetController().CanGoBack();
      case CMD_FORWARD:
        return web_view_->GetWebContents()->GetController().CanGoForward();
    }
    return false;
  }

  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    switch (command_id) {
      case CMD_RELOAD:
        web_view_->GetWebContents()->GetController().Reload(false);
        return true;
      case CMD_RELOAD_IGNORE_CACHE:
        web_view_->GetWebContents()->GetController().ReloadIgnoringCache(false);
        return true;
      case CMD_BACK:
        web_view_->GetWebContents()->GetController().GoBack();
        return true;
      case CMD_FORWARD:
        web_view_->GetWebContents()->GetController().GoForward();
        return true;
    }
    return false;
  }

  views::WebView* web_view_;
  bool reserved_accelerator_enabled_;
  scoped_ptr<AcceleratorManager> accelerator_manager_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebActivityController);
};

// A web view for athena's web activity.
class AthenaWebView : public views::WebView {
 public:
  AthenaWebView(content::BrowserContext* context)
      : views::WebView(context), controller_(new WebActivityController(this)) {}
  virtual ~AthenaWebView() {}

  void InstallAccelerators() { controller_->InstallAccelerators(); }

 private:
  // WebContentsDelegate:
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE {
    return controller_->PreHandleKeyboardEvent(
        source, event, is_keyboard_shortcut);
  }

  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE {
    controller_->HandleKeyboardEvent(source, event);
  }

  scoped_ptr<WebActivityController> controller_;

  DISALLOW_COPY_AND_ASSIGN(AthenaWebView);
};

}  // namespace

WebActivity::WebActivity(content::BrowserContext* browser_context,
                         const GURL& url)
    : browser_context_(browser_context),
      url_(url),
      web_view_(NULL) {
}

WebActivity::~WebActivity() {
}

ActivityViewModel* WebActivity::GetActivityViewModel() {
  return this;
}

void WebActivity::Init() {
  DCHECK(web_view_);
  static_cast<AthenaWebView*>(web_view_)->InstallAccelerators();
}

SkColor WebActivity::GetRepresentativeColor() const {
  // TODO(sad): Compute the color from the favicon.
  return SK_ColorGRAY;
}

base::string16 WebActivity::GetTitle() const {
  return web_view_->GetWebContents()->GetTitle();
}

bool WebActivity::UsesFrame() const {
  return true;
}

views::View* WebActivity::GetContentsView() {
  if (!web_view_) {
    web_view_ = new AthenaWebView(browser_context_);
    web_view_->LoadInitialURL(url_);
    Observe(web_view_->GetWebContents());
  }
  return web_view_;
}

void WebActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  ActivityManager::Get()->UpdateActivity(this);
}

void WebActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  ActivityManager::Get()->UpdateActivity(this);
}

}  // namespace athena
