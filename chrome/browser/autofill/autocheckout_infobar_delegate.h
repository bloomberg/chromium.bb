// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"

class AutocheckoutManager;

namespace content {
struct LoadCommittedDetails;
}

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class AutocheckoutInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an autofillflow infobar delegate and adds it to |infobar_service|.
  static void Create(const AutofillMetrics& metric_logger,
                     const GURL& source_url,
                     const content::SSLStatus& ssl_status,
                     AutocheckoutManager* autocheckout_manager,
                     InfoBarService* infobar_service);

#if defined(UNIT_TEST)
  static scoped_ptr<ConfirmInfoBarDelegate> Create(
      const AutofillMetrics& metric_logger,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      AutocheckoutManager* autocheckout_manager) {
    return scoped_ptr<ConfirmInfoBarDelegate>(new AutocheckoutInfoBarDelegate(
        metric_logger, source_url, ssl_status, autocheckout_manager, NULL));
  }
#endif

 private:
  AutocheckoutInfoBarDelegate(const AutofillMetrics& metric_logger,
                              const GURL& source_url,
                              const content::SSLStatus& ssl_status,
                              AutocheckoutManager* autocheckout_manager,
                              InfoBarService* infobar_service);

  virtual ~AutocheckoutInfoBarDelegate();

  // Logs UMA metric for user action type.
  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // For logging UMA metrics.
  // Weak reference. Owned by the AutofillManager that initiated this infobar.
  const AutofillMetrics& metric_logger_;

  // To callback AutocheckoutManager's ShowAutocheckoutDialog.
  AutocheckoutManager* autocheckout_manager_;

  // URL of the page which triggered infobar.
  GURL source_url_;

  // SSL status of the page which triggered infobar.
  content::SSLStatus ssl_status_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutocheckoutInfoBar);

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutInfoBarDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_INFOBAR_DELEGATE_H_
