// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// Provides help content during OOBE / login.
// Based on connectivity state (offline/online) shows help topic dialog
// or launches HelpApp in BWSI mode.
class HelpAppLauncher : public LoginHtmlDialog::Delegate,
                        public base::RefCountedThreadSafe<HelpAppLauncher> {
 public:
  // IDs of help topics available from HelpApp.
  enum HelpTopic {
    // Showed on basic connectivity issues.
    HELP_CONNECTIVITY = 188752,
    // Showed at EULA screen as "Learn more" about stats/crash reports.
    HELP_STATS_USAGE = 183078,
    // Showed whenever there're troubles signing in (offline case).
    HELP_CANT_ACCESS_ACCOUNT_OFFLINE = 188755,
    // Showed whenever there're troubles signing in (online case).
    HELP_CANT_ACCESS_ACCOUNT = 188036,
    // Showed in case when account was disabled.
    HELP_ACCOUNT_DISABLED = 188756,
    // Showed in case when hosted account is used.
    HELP_HOSTED_ACCOUNT = 1054228,
  };

  // Parent window is used to show dialog.
  explicit HelpAppLauncher(gfx::NativeWindow parent_window);

  // Shows specified help topic.
  void ShowHelpTopic(HelpTopic help_topic_id);

  // Returns true if the dialog is currently open.
  bool is_open() const { return dialog_.get() && dialog_->is_open(); }

 protected:
  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() {}

 private:
  // Shows help topic dialog for specified GURL.
  void ShowHelpTopicDialog(const GURL& topic_url);

  // Dialog used to display help like "Can't access your account".
  scoped_ptr<LoginHtmlDialog> dialog_;

  // Parent window which is passed to help dialog.
  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(HelpAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELP_APP_LAUNCHER_H_
