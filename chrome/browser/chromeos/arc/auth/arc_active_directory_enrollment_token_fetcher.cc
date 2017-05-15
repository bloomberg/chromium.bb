// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace {

constexpr char kSamlAuthErrorMessage[] = "SAML authentication failed. ";

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->device_management_service();
}

std::string GetClientId() {
  policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetInstallAttributes()->GetDeviceId();
}

}  // namespace

namespace arc {

ArcActiveDirectoryEnrollmentTokenFetcher::
    ArcActiveDirectoryEnrollmentTokenFetcher()
    : weak_ptr_factory_(this) {}

ArcActiveDirectoryEnrollmentTokenFetcher::
    ~ArcActiveDirectoryEnrollmentTokenFetcher() = default;

void ArcActiveDirectoryEnrollmentTokenFetcher::Fetch(
    const FetchCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  dm_token_storage_ = base::MakeUnique<policy::DMTokenStorage>(
      g_browser_process->local_state());
  dm_token_storage_->RetrieveDMToken(
      base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable(
    const std::string& dm_token) {
  if (dm_token.empty()) {
    LOG(ERROR) << "Retrieving the DMToken failed.";
    base::ResetAndReturn(&callback_)
        .Run(Status::FAILURE, std::string(), std::string());
    return;
  }

  DCHECK(dm_token_.empty());
  dm_token_ = dm_token;
  DoFetchEnrollmentToken();
}

void ArcActiveDirectoryEnrollmentTokenFetcher::DoFetchEnrollmentToken() {
  DCHECK(!dm_token_.empty());
  DCHECK(!fetch_request_job_);

  policy::DeviceManagementService* service = GetDeviceManagementService();
  fetch_request_job_.reset(
      service->CreateJob(policy::DeviceManagementRequestJob::
                             TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER,
                         g_browser_process->system_request_context()));

  fetch_request_job_->SetDMToken(dm_token_);
  fetch_request_job_->SetClientID(GetClientId());
  em::ActiveDirectoryEnrollPlayUserRequest* enroll_request =
      fetch_request_job_->GetRequest()
          ->mutable_active_directory_enroll_play_user_request();
  if (!auth_session_id_.empty()) {
    // Happens after going through SAML flow. Call DM server again with the
    // given |auth_session_id_|.
    enroll_request->set_auth_session_id(auth_session_id_);
    auth_session_id_.clear();
  }

  fetch_request_job_->Start(
      base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcher::
                     OnEnrollmentTokenResponseReceived,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::
    OnEnrollmentTokenResponseReceived(
        policy::DeviceManagementStatus dm_status,
        int net_error,
        const em::DeviceManagementResponse& response) {
  fetch_request_job_.reset();

  Status fetch_status;
  std::string enrollment_token;
  std::string user_id;

  switch (dm_status) {
    case policy::DM_STATUS_SUCCESS: {
      if (!response.has_active_directory_enroll_play_user_response()) {
        LOG(WARNING) << "Invalid Active Directory enroll Play user response.";
        fetch_status = Status::FAILURE;
        break;
      }
      const em::ActiveDirectoryEnrollPlayUserResponse& enroll_response =
          response.active_directory_enroll_play_user_response();

      if (enroll_response.has_saml_parameters()) {
        // SAML authentication required.
        const em::SamlParametersProto& saml_params =
            enroll_response.saml_parameters();
        auth_session_id_ = saml_params.auth_session_id();
        InitiateSamlFlow(saml_params.auth_redirect_url());
        return;  // SAML flow eventually calls |callback_| or this function.
      }

      DCHECK(enroll_response.has_enrollment_token());
      fetch_status = Status::SUCCESS;
      enrollment_token = enroll_response.enrollment_token();
      user_id = enroll_response.user_id();
      break;
    }
    case policy::DM_STATUS_SERVICE_ARC_DISABLED: {
      fetch_status = Status::ARC_DISABLED;
      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Fetching an enrollment token failed. DM Status: "
                 << dm_status;
      fetch_status = Status::FAILURE;
      break;
    }
  }

  dm_token_.clear();
  base::ResetAndReturn(&callback_).Run(fetch_status, enrollment_token, user_id);
}

void ArcActiveDirectoryEnrollmentTokenFetcher::InitiateSamlFlow(
    const std::string& auth_redirect_url) {
  // We must have an auth session id. Otherwise, we might end up in a loop.
  if (auth_session_id_.empty()) {
    LOG(ERROR) << kSamlAuthErrorMessage << "No auth session id.";
    CancelSamlFlow(true /* close_saml_page */);
    return;
  }

  // Check if URL is valid.
  GURL url(auth_redirect_url);
  if (!url.is_valid()) {
    LOG(ERROR) << kSamlAuthErrorMessage << "Redirect URL invalid.";
    CancelSamlFlow(true /* close_saml_page */);
    return;
  }

  // Open a browser window and navigate to the URL.
  Profile* const profile = ArcSessionManager::Get()->profile();
  if (!profile) {
    LOG(ERROR) << kSamlAuthErrorMessage << "No user profile found.";
    CancelSamlFlow(true /* close_saml_page */);
    return;
  }
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  DCHECK(displayer.browser());
  browser_ = displayer.browser();
  content::WebContents* web_contents =
      chrome::AddSelectedTabWithURL(browser_, url, ui::PAGE_TRANSITION_LINK);

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      browser_->window()->GetNativeWindow());

  // Observe web contents to receive the DidFinishNavigation() event.
  Observe(web_contents);
}

void ArcActiveDirectoryEnrollmentTokenFetcher::CancelSamlFlow(
    bool close_saml_page) {
  dm_token_.clear();
  auth_session_id_.clear();
  FinalizeSamlPage(close_saml_page);
  DCHECK(!callback_.is_null());
  base::ResetAndReturn(&callback_)
      .Run(Status::FAILURE, std::string(), std::string());
}

void ArcActiveDirectoryEnrollmentTokenFetcher::FinalizeSamlPage(
    bool close_saml_page) {
  content::WebContents* web_contents =
      content::WebContentsObserver::web_contents();
  DCHECK(browser_);
  DCHECK(web_contents);
  Observe(nullptr);
  if (close_saml_page) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&chrome::CloseWebContents, browser_, web_contents,
                   false /* add_to_history */));
  }
  browser_ = nullptr;
}

void ArcActiveDirectoryEnrollmentTokenFetcher::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Set close_saml_page to false here since the browser might show the actual
  // reason for the failure.
  if (navigation_handle->GetNetErrorCode() != net::OK) {
    LOG(ERROR) << kSamlAuthErrorMessage << "Got net error code "
               << navigation_handle->GetNetErrorCode() << ".";
    CancelSamlFlow(false /* close_saml_page */);
    return;
  }

  if (navigation_handle->IsErrorPage()) {
    LOG(ERROR) << kSamlAuthErrorMessage << "Is error page.";
    CancelSamlFlow(false /* close_saml_page */);
    return;
  }

  // No response headers is fine, could be intermediate state.
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (!response_headers)
    return;

  if (response_headers->response_code() != net::HTTP_OK) {
    LOG(ERROR) << kSamlAuthErrorMessage
               << "Bad HTTP response: " << response_headers->response_code();
    CancelSamlFlow(false /* close_saml_page */);
    return;
  }

  policy::DeviceManagementService* service = GetDeviceManagementService();
  GURL server_url(service->GetServerUrl());
  const GURL& url = navigation_handle->GetURL();

  if (url.scheme() == server_url.scheme() && url.host() == server_url.host() &&
      base::StartsWith(url.path(), server_url.path(),
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // SAML flow succeeded! Close the page, no need to keep it open.
    FinalizeSamlPage(true /* close_saml_page */);
    // Fetch enrollment token again, this time with non-empty session id.
    DCHECK(!auth_session_id_.empty());
    DoFetchEnrollmentToken();
  }
}

void ArcActiveDirectoryEnrollmentTokenFetcher::WebContentsDestroyed() {
  LOG(ERROR) << kSamlAuthErrorMessage << "Web contents destroyed.";
  // No need to close the page, it's already being destroyed.
  CancelSamlFlow(false /* close_saml_page */);
}

}  // namespace arc
