// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"

class PrefService;

namespace arc {

class ArcOptInPreferenceHandler;

// Handles the Terms-of-service agreement user action.
class ArcTermsOfServiceNegotiator : public ArcSupportHost::Observer,
                                    public ArcOptInPreferenceHandlerObserver {
 public:
  ArcTermsOfServiceNegotiator(PrefService* pref_service,
                              ArcSupportHost* support_host);
  ~ArcTermsOfServiceNegotiator() override;

  // Shows "Terms of service" page on ARC support Chrome App. Invokes the
  // |callback| asynchronously with "|agreed| = true" if user agrees it.
  // Otherwise (e.g., user clicks "Cancel" or closes the window), invokes
  // |callback| with |agreed| = false.
  // Deleting this instance cancels the operation, so |callback| will never
  // be invoked then.
  using NegotiationCallback = base::Callback<void(bool accepted)>;
  void StartNegotiation(const NegotiationCallback& callback);

 private:
  // ArcSupportHost::Observer:
  void OnWindowClosed() override;
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnAuthSucceeded(const std::string& auth_code) override;
  void OnRetryClicked() override;
  void OnSendFeedbackClicked() override;

  // ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  PrefService* const pref_service_;
  // Owned by ArcSessionManager.
  ArcSupportHost* const support_host_;

  NegotiationCallback pending_callback_;
  std::unique_ptr<ArcOptInPreferenceHandler> preference_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceNegotiator);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_
