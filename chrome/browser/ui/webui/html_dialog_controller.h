// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_CONTROLLER_H_
#pragma once

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class Profile;

// This provides the common functionality for HtmlDialogs of notifying the
// dialog that it should close when the browser that created it has closed to
// avoid using an old Profile object.
class HtmlDialogController : public content::NotificationObserver {
 public:
  HtmlDialogController(HtmlDialogUIDelegate* delegate,
                       Profile* profile,
                       Browser* browser);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // The delegate controlled by this instance. This class is owned by the
  // delegate.
  HtmlDialogUIDelegate* dialog_delegate_;

  // Used for notification of parent browser closing.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_CONTROLLER_H_
