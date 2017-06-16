// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_ACTIVE_DIRECTORY_AUTH_NEGOTIATOR_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_ACTIVE_DIRECTORY_AUTH_NEGOTIATOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"

namespace arc {

// Notifies the user that they will have to authenticate with Active Directory.
class ArcActiveDirectoryAuthNegotiator
    : public ArcTermsOfServiceNegotiator,
      public ArcSupportHost::TermsOfServiceDelegate {
 public:
  explicit ArcActiveDirectoryAuthNegotiator(ArcSupportHost* support_host);
  ~ArcActiveDirectoryAuthNegotiator() override;

 private:
  // ArcSupportHost::TermsOfServiceDelegate:
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnTermsRejected() override;
  void OnTermsRetryClicked() override;

  // ArcTermsOfServiceNegotiator:
  // Shows "AD Login Notification" page on ARC support Chrome App.
  void StartNegotiationImpl() override;

  // Owned by ArcSessionManager.
  ArcSupportHost* const support_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryAuthNegotiator);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_ACTIVE_DIRECTORY_AUTH_NEGOTIATOR_H_
