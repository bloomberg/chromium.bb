// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"

#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace chromeos {

TermsOfServiceScreen::TermsOfServiceScreen(
    BaseScreenDelegate* base_screen_delegate,
    TermsOfServiceScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_TERMS_OF_SERVICE),
      view_(view) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

TermsOfServiceScreen::~TermsOfServiceScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void TermsOfServiceScreen::Show() {
  if (!view_)
    return;

  // Set the domain name whose Terms of Service are being shown.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  view_->SetDomain(connector->GetEnterpriseDisplayDomain());

  // Show the screen.
  view_->Show();

  // Start downloading the Terms of Service.
  StartDownload();
}

void TermsOfServiceScreen::Hide() {
  if (view_)
    view_->Hide();
}

void TermsOfServiceScreen::OnDecline() {
  Finish(ScreenExitCode::TERMS_OF_SERVICE_DECLINED);
}

void TermsOfServiceScreen::OnAccept() {
  Finish(ScreenExitCode::TERMS_OF_SERVICE_ACCEPTED);
}

void TermsOfServiceScreen::OnViewDestroyed(TermsOfServiceScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

void TermsOfServiceScreen::StartDownload() {
  const PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  // If an URL from which the Terms of Service can be downloaded has not been
  // set, show an error message to the user.
  std::string terms_of_service_url =
      prefs->GetString(prefs::kTermsOfServiceURL);
  if (terms_of_service_url.empty()) {
    if (view_)
      view_->OnLoadError();
    return;
  }

  // Start downloading the Terms of Service.
  terms_of_service_fetcher_ = net::URLFetcher::Create(
      GURL(terms_of_service_url), net::URLFetcher::GET, this);
  terms_of_service_fetcher_->SetRequestContext(
      g_browser_process->system_request_context());
  // Request a text/plain MIME type as only plain-text Terms of Service are
  // accepted.
  terms_of_service_fetcher_->AddExtraRequestHeader("Accept: text/plain");
  // Retry up to three times if network changes are detected during the
  // download.
  terms_of_service_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  terms_of_service_fetcher_->Start();

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(1),
                        this, &TermsOfServiceScreen::OnDownloadTimeout);
}

void TermsOfServiceScreen::OnDownloadTimeout() {
  // Abort the download attempt.
  terms_of_service_fetcher_.reset();

  // Show an error message to the user.
  if (view_)
    view_->OnLoadError();
}

void TermsOfServiceScreen::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source != terms_of_service_fetcher_.get()) {
    NOTREACHED() << "Callback from foreign URL fetcher";
    return;
  }

  download_timer_.Stop();

  // Destroy the fetcher when this method returns.
  std::unique_ptr<net::URLFetcher> fetcher(
      std::move(terms_of_service_fetcher_));
  if (!view_)
    return;

  std::string terms_of_service;
  std::string mime_type;
  // If the Terms of Service could not be downloaded, do not have a MIME type of
  // text/plain or are empty, show an error message to the user.
  if (!source->GetStatus().is_success() ||
      !source->GetResponseHeaders()->GetMimeType(&mime_type) ||
      mime_type != "text/plain" ||
      !source->GetResponseAsString(&terms_of_service)) {
    view_->OnLoadError();
  } else {
    // If the Terms of Service were downloaded successfully, show them to the
    // user.
    view_->OnLoadSuccess(terms_of_service);
  }
}

}  // namespace chromeos
