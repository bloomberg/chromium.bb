// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WEB_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WEB_DIALOG_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/web_dialog_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace chromeos {

class BubbleFrameView;

// Launches web dialog during OOBE/Login with specified URL and title.
class LoginWebDialog : public WebDialogDelegate,
                       public content::NotificationObserver {
 public:
  // Delegate class to get notifications from the dialog.
  class Delegate {
   public:
    // Called when dialog has been closed.
    virtual void OnDialogClosed();

   protected:
    virtual ~Delegate() {}
  };

  enum Style {
    STYLE_GENERIC, // Use generic CreateChromeWindow as a host.
    STYLE_BUBBLE   // Use chromeos::BubbleWindow as a host.
  };

  LoginWebDialog(Delegate* delegate,
                 gfx::NativeWindow parent_window,
                 const string16& title,
                 const GURL& url,
                 Style style);
  virtual ~LoginWebDialog();

  void Show();

  // Overrides default width/height for dialog.
  void SetDialogSize(int width, int height);

  // Overrides dialog title.
  void SetDialogTitle(const string16& title);

  void set_url(const GURL& url) { url_ = url; }

  bool is_open() const { return is_open_; }

 protected:
  // WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Notifications receiver.
  Delegate* delegate_;

  gfx::NativeWindow parent_window_;
  string16 title_;
  GURL url_;
  Style style_;
  content::NotificationRegistrar notification_registrar_;
  BubbleFrameView* bubble_frame_view_;
  bool is_open_;

  // Dialog display size.
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(LoginWebDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WEB_DIALOG_H_
