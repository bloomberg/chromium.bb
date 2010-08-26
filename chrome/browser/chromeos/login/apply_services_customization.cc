// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/apply_services_customization.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile_manager.h"
#include "googleurl/src/gurl.h"

namespace {

// URL where to fetch OEM services customization manifest from.
// TODO(denisromanov): Change this to real URL when it becomes available.
const char kServicesCustomizationManifestUrl[] =
    "file:///mnt/partner_partition/etc/chromeos/services_manifest.json";

// Name of local state option that tracks if services customization has been
// applied.
const char kServicesCustomizationAppliedPref[] = "ServicesCustomizationApplied";

// Maximum number of retries to fetch file if network is not available.
const int kMaxFetchRetries = 3;

// Delay between file fetch retries if network is not available.
const int kRetriesDelayInSec = 2;

}  // namespace

namespace chromeos {


// static
void ApplyServicesCustomization::StartIfNeeded() {
  if (!IsApplied()) {
    ApplyServicesCustomization* object =
        new ApplyServicesCustomization(kServicesCustomizationManifestUrl);
    if (!object->Init()) {
      delete object;
    }
    // |object| will be deleted on download complete.
  }
}

// static
void ApplyServicesCustomization::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterBooleanPref(kServicesCustomizationAppliedPref, false);
}

// static
bool ApplyServicesCustomization::IsApplied() {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(kServicesCustomizationAppliedPref);
}

// static
void ApplyServicesCustomization::SetApplied(bool val) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(kServicesCustomizationAppliedPref, val);
}

ApplyServicesCustomization::ApplyServicesCustomization(
    const std::string& url_str) : url_(url_str), num_retries_(0) {
}

bool ApplyServicesCustomization::Init() {
  DCHECK(url_.is_valid());
  if (!url_.is_valid()) {
    return false;
  }

  if (url_.SchemeIsFile()) {
    std::string manifest;
    if (file_util::ReadFileToString(FilePath(url_.path()), &manifest)) {
      Apply(manifest);
    } else {
      LOG(ERROR) << "Failed to load services customization manifest from: "
                 << url_.path();
    }

    return false;
  }

  StartFileFetch();
  return true;
}

void ApplyServicesCustomization::StartFileFetch() {
  url_fetcher_.reset(new URLFetcher(url_, URLFetcher::GET, this));
  url_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  url_fetcher_->Start();
}

void ApplyServicesCustomization::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (response_code == 200) {
    Apply(data);
  } else {
    NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
    if (!network->Connected() && num_retries_ < kMaxFetchRetries) {
      num_retries_++;
      retry_timer_.Start(base::TimeDelta::FromSeconds(kRetriesDelayInSec),
                         this, &ApplyServicesCustomization::StartFileFetch);
      return;
    }
    LOG(ERROR) << "URL fetch for services customization failed:"
               << " response code = " << response_code
               << " URL = " << url.spec();
  }
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ApplyServicesCustomization::Apply(const std::string& manifest) {
  chromeos::ServicesCustomizationDocument customization;
  if (!customization.LoadManifestFromString(manifest)) {
    LOG(ERROR) << "Failed to partner parse services customizations manifest";
    return;
  }

  LOG(INFO) << "Partner services customizations manifest loaded successfully";
  if (!customization.initial_start_page_url().empty()) {
    // Append partner's start page url to command line so it gets opened
    // on browser startup.
    CommandLine::ForCurrentProcess()->AppendArg(
        customization.initial_start_page_url());
    LOG(INFO) << "initial_start_page_url: "
              << customization.initial_start_page_url();
  }
  // TODO(dpolukhin): apply customized apps, exts and support page.

  SetApplied(true);
}

}
