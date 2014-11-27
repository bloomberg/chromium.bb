// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_WEB_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_WEB_DIALOG_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace views {
class Widget;
}

namespace chromeos {

// This is the modal Web dialog to display captive portal login page.
// It is automatically closed when successful authorization is detected.
class NetworkPortalWebDialog : public ui::WebDialogDelegate {
 public:
  explicit NetworkPortalWebDialog(
      base::WeakPtr<NetworkPortalNotificationController> controller);
  virtual ~NetworkPortalWebDialog();

  void SetWidget(views::Widget* widget);
  void Close();

 private:
  // ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const override;
  virtual base::string16 GetDialogTitle() const override;
  virtual GURL GetDialogContentURL() const override;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  virtual void GetDialogSize(gfx::Size* size) const override;
  virtual std::string GetDialogArgs() const override;
  virtual bool CanResizeDialog() const override;
  virtual void OnDialogClosed(const std::string& json_retval) override;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) override;
  virtual bool ShouldShowDialogTitle() const override;

  base::WeakPtr<NetworkPortalNotificationController> controller_;

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalWebDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_WEB_DIALOG_H_
