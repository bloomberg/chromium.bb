// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/auth/arc_fetcher_base.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/web_contents_observer.h"

namespace enterprise_management {
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace policy {
class DeviceManagementRequestJob;
class DMTokenStorage;
}  // namespace policy

class Browser;

namespace arc {

// Fetches an enrollment token and user id for a new managed Google Play account
// when using ARC with Active Directory.
class ArcActiveDirectoryEnrollmentTokenFetcher
    : public ArcFetcherBase,
      public content::WebContentsObserver {
 public:
  ArcActiveDirectoryEnrollmentTokenFetcher();
  ~ArcActiveDirectoryEnrollmentTokenFetcher() override;

  enum class Status {
    SUCCESS,       // The fetch was successful.
    FAILURE,       // The request failed.
    ARC_DISABLED,  // ARC is not enabled.
  };

  // Fetches the enrollment token and user id in the background and calls
  // |callback| when done. |status| indicates whether the operation was
  // successful. In case of success, |enrollment_token| and |user_id| are set to
  // the fetched values.
  // Fetch() should be called once per instance, and it is expected that the
  // inflight operation is cancelled without calling the |callback| when the
  // instance is deleted.
  using FetchCallback = base::Callback<void(Status status,
                                            const std::string& enrollment_token,
                                            const std::string& user_id)>;
  void Fetch(const FetchCallback& callback);

 private:
  // Called when the |dm_token| is retrieved from policy::DMTokenStorage.
  // Triggers DoFetchEnrollmentToken().
  void OnDMTokenAvailable(const std::string& dm_token);

  // Sends a request to fetch an enrollment token from DM server.
  void DoFetchEnrollmentToken();

  // Response from DM server. Calls the stored FetchCallback or initiates the
  // SAML flow.
  void OnEnrollmentTokenResponseReceived(
      policy::DeviceManagementStatus dm_status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Opens up a web browser, follows |auth_redirect_url| and observes the web
  // contents (hooks up DidFinishNavigation()). Calls CancelSamlFlow() if the
  // url is invalid.
  void InitiateSamlFlow(const std::string& auth_redirect_url);

  // Cleans up SAML resources and calls callback_ with an error status and
  // resets state.
  void CancelSamlFlow(bool close_saml_page);

  // Stops observing and optionally closes the page opened by
  // InitiateSamlFlow().
  void FinalizeSamlPage(bool close_saml_page);

  // content::WebContentsObserver:
  // Checks whether the server URL of the device management service was hit. If
  // it was, triggers another enrollment token fetch, this time passing the now
  // non-empty |auth_session_id_|. Cancels the SAML flow if anything went wrong.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // content::WebContentsObserver:
  // Cancels SAML flow.
  void WebContentsDestroyed() override;

  std::unique_ptr<policy::DeviceManagementRequestJob> fetch_request_job_;
  std::unique_ptr<policy::DMTokenStorage> dm_token_storage_;
  FetchCallback callback_;

  std::string dm_token_;

  // Web browser the SAML page was created in, not owned. Lives while the page
  // is observed and gets reset in FinalizeSamlPage(). Note that closing the
  // browser triggers WebContentsDestroyed(), which calls FinalizeSamlPage().
  Browser* browser_;

  // Current SAML auth session id, stored during SAML authentication.
  std::string auth_session_id_;

  base::WeakPtrFactory<ArcActiveDirectoryEnrollmentTokenFetcher>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryEnrollmentTokenFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
