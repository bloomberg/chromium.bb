// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_CONTROLLER_H_
#pragma once

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class Browser;

namespace content {
class BrowserContext;
}

// This provides the common functionality for WebDialogs of notifying the
// dialog that it should close when the browser that created it has closed to
// avoid using an old Profile object.
class WebDialogController : public content::NotificationObserver {
 public:
   WebDialogController(ui::WebDialogDelegate* delegate,
                       content::BrowserContext* context,
                       Browser* browser);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // The delegate controlled by this instance. This class is owned by the
  // delegate.
  ui::WebDialogDelegate* dialog_delegate_;

  // Used for notification of parent browser closing.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_CONTROLLER_H_
