// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/password_manager/password_manager_infobar_delegate.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

namespace password_manager {
enum class CredentialSourceType;
}

// After a successful *new* login attempt, we take the PasswordFormManager in
// provisional_save_manager_ and move it to a SavePasswordInfoBarDelegate while
// the user makes up their mind with the "save password" infobar. Note if the
// login is one we already know about, the end of the line is
// provisional_save_manager_ because we just update it on success and so such
// forms never end up in an infobar.
class SavePasswordInfoBarDelegate : public PasswordManagerInfoBarDelegate {
 public:
  // If we won't be showing the one-click signin infobar, creates a save
  // password infobar and delegate and adds the infobar to the InfoBarService
  // for |web_contents|.  |uma_histogram_suffix| is empty, or one of the
  // "group_X" suffixes used in the histogram names for infobar usage reporting;
  // if empty, the usage is not reported, otherwise the suffix is used to choose
  // the right histogram.
  static void Create(
      content::WebContents* web_contents,
      scoped_ptr<password_manager::PasswordFormManager> form_to_save,
      const std::string& uma_histogram_suffix,
      password_manager::CredentialSourceType source_type);

  ~SavePasswordInfoBarDelegate() override;

  base::string16 GetFirstRunExperienceMessage();

  // ConfirmInfoBarDelegate:
  void InfoBarDismissed() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 protected:
  // Makes a ctor available in tests.
  SavePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      scoped_ptr<password_manager::PasswordFormManager> form_to_save,
      const std::string& uma_histogram_suffix,
      password_manager::CredentialSourceType source_type,
      bool is_smartlock_branding_enabled,
      bool should_show_first_run_experience);

 private:
  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per her decision.
  scoped_ptr<password_manager::PasswordFormManager> form_to_save_;

  // Used to track the results we get from the info bar.
  password_manager::metrics_util::ResponseType infobar_response_;

  // Measures the "Save password?" prompt lifetime. Used to report an UMA
  // signal.
  base::ElapsedTimer timer_;

  // The group name corresponding to the domain name of |form_to_save_| if the
  // form is on a monitored domain. Otherwise, an empty string.
  const std::string uma_histogram_suffix_;

  // Records source from where infobar was triggered.
  // Infobar appearance (message, buttons) depends on value of this parameter.
  password_manager::CredentialSourceType source_type_;

  bool should_show_first_run_experience_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

// Creates the platform-specific SavePassword InfoBar. This function is defined
// in platform-specific .cc (or .mm) files.
scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
