// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;

namespace policy {
class PolicyOAuth2TokenFetcher;
}

namespace chromeos {

class EnterpriseEnrollmentHelperImpl : public EnterpriseEnrollmentHelper,
                                       public BrowsingDataRemover::Observer {
 public:
  EnterpriseEnrollmentHelperImpl(
      EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain);
  ~EnterpriseEnrollmentHelperImpl() override;

  // Overridden from EnterpriseEnrollmentHelper:
  void EnrollUsingProfile(Profile* profile,
                          bool fetch_additional_token) override;
  void EnrollUsingToken(const std::string& token) override;
  void ClearAuth(const base::Closure& callback) override;

 private:
  void DoEnrollUsingToken(const std::string& token);

  // Handles completion of the OAuth2 token fetch attempt.
  void OnTokenFetched(size_t fetcher_index,
                      const std::string& token,
                      const GoogleServiceAuthError& error);

  // Handles completion of the enrollment attempt.
  void OnEnrollmentFinished(policy::EnrollmentStatus status);

  void ReportAuthStatus(const GoogleServiceAuthError& error);
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on |enrollment_mode_|.
  void UMA(policy::MetricEnrollment sample);

  // Overridden from BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone() override;

  const policy::EnrollmentConfig enrollment_config_;
  const std::string enrolling_user_domain_;
  Profile* profile_;
  bool fetch_additional_token_;

  bool started_;
  size_t oauth_fetchers_finished_;
  GoogleServiceAuthError last_auth_error_;
  std::string additional_token_;
  bool finished_;
  bool success_;
  bool auth_data_cleared_;

  // The browsing data remover instance currently active, if any.
  BrowsingDataRemover* browsing_data_remover_;

  // The callbacks to invoke after browsing data has been cleared.
  std::vector<base::Closure> auth_clear_callbacks_;

  ScopedVector<policy::PolicyOAuth2TokenFetcher> oauth_fetchers_;

  base::WeakPtrFactory<EnterpriseEnrollmentHelperImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentHelperImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
