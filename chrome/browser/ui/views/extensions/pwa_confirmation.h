// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/web_application_info.h"
#include "ui/views/window/dialog_delegate.h"

// Provides the core UI for confirming the installation of a PWA for the
// |PWAConfirmationDialogView| and |PWAConfirmationBubbleView| form factors.
class PWAConfirmation {
 public:
  static base::string16 GetWindowTitle();
  static base::string16 GetDialogButtonLabel(ui::DialogButton button);

  PWAConfirmation(views::DialogDelegateView* dialog,
                  const WebApplicationInfo& web_app_info,
                  chrome::AppInstallationAcceptanceCallback callback);
  ~PWAConfirmation();

  void Accept();
  void WindowClosing();

 private:
  WebApplicationInfo web_app_info_;
  chrome::AppInstallationAcceptanceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PWAConfirmation);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_
