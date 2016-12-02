// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler.h"

namespace arc {

ArcTermsOfServiceNegotiator::ArcTermsOfServiceNegotiator(
    PrefService* pref_service,
    ArcSupportHost* support_host)
    : pref_service_(pref_service), support_host_(support_host) {
  DCHECK(pref_service_);
  DCHECK(support_host_);
}

ArcTermsOfServiceNegotiator::~ArcTermsOfServiceNegotiator() {
  support_host_->RemoveObserver(this);
}

void ArcTermsOfServiceNegotiator::StartNegotiation(
    const NegotiationCallback& callback) {
  DCHECK(pending_callback_.is_null());
  DCHECK(!preference_handler_);
  pending_callback_ = callback;
  preference_handler_ =
      base::MakeUnique<ArcOptInPreferenceHandler>(this, pref_service_);
  // This automatically updates all preferences.
  preference_handler_->Start();

  support_host_->AddObserver(this);
  support_host_->ShowTermsOfService();
}

void ArcTermsOfServiceNegotiator::OnWindowClosed() {
  DCHECK(!pending_callback_.is_null());
  DCHECK(preference_handler_);
  support_host_->RemoveObserver(this);
  preference_handler_.reset();

  // User cancels terms-of-service agreement UI by clicking "Cancel" button
  // or closing the window directly.
  base::ResetAndReturn(&pending_callback_).Run(false);
}

void ArcTermsOfServiceNegotiator::OnTermsAgreed(
    bool is_metrics_enabled,
    bool is_backup_and_restore_enabled,
    bool is_location_service_enabled) {
  DCHECK(!pending_callback_.is_null());
  DCHECK(preference_handler_);
  support_host_->RemoveObserver(this);

  // Update the preferences with the value passed from UI.
  preference_handler_->EnableMetrics(is_metrics_enabled);
  preference_handler_->EnableBackupRestore(is_backup_and_restore_enabled);
  preference_handler_->EnableLocationService(is_location_service_enabled);
  preference_handler_.reset();

  base::ResetAndReturn(&pending_callback_).Run(true);
}

void ArcTermsOfServiceNegotiator::OnAuthSucceeded(
    const std::string& auth_code) {
  NOTREACHED();
}

void ArcTermsOfServiceNegotiator::OnRetryClicked() {
  support_host_->ShowTermsOfService();
}

void ArcTermsOfServiceNegotiator::OnSendFeedbackClicked() {
  NOTREACHED();
}

void ArcTermsOfServiceNegotiator::OnMetricsModeChanged(bool enabled,
                                                       bool managed) {
  support_host_->SetMetricsPreferenceCheckbox(enabled, managed);
}

void ArcTermsOfServiceNegotiator::OnBackupAndRestoreModeChanged(bool enabled,
                                                                bool managed) {
  support_host_->SetBackupAndRestorePreferenceCheckbox(enabled, managed);
}

void ArcTermsOfServiceNegotiator::OnLocationServicesModeChanged(bool enabled,
                                                                bool managed) {
  support_host_->SetLocationServicesPreferenceCheckbox(enabled, managed);
}

}  // namespace arc
