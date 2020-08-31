// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigation_correction_tab_observer.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/navigation_corrector.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/embedder_support/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::RenderFrameHost;
using content::WebContents;

namespace {

// Disabled due to a server-side issue with returned results.
// TODO(https://crbug.com/1030281): Either find an owner to fix this, or remove
// the feature.
bool g_allow_enable_corrections_for_testing = false;

}  // namespace

NavigationCorrectionTabObserver::NavigationCorrectionTabObserver(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        embedder_support::kAlternateErrorPagesEnabled,
        base::Bind(&NavigationCorrectionTabObserver::OnEnabledChanged,
                   base::Unretained(this)));
  }
}

NavigationCorrectionTabObserver::~NavigationCorrectionTabObserver() {}

void NavigationCorrectionTabObserver::SetAllowEnableCorrectionsForTesting(
    bool allow_enable_corrections_for_testing) {
  g_allow_enable_corrections_for_testing = allow_enable_corrections_for_testing;
}

// static
void NavigationCorrectionTabObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(embedder_support::kAlternateErrorPagesEnabled,
                             true,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void NavigationCorrectionTabObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Ignore subframe creation - only main frame error pages can request and
  // display nagivation correction information.
  if (render_frame_host->GetParent())
    return;
  UpdateNavigationCorrectionInfo(render_frame_host);
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

GURL NavigationCorrectionTabObserver::GetNavigationCorrectionURL() const {
  // Disable navigation corrections when the preference is disabled or when in
  // Incognito mode.
  if (!profile_->GetPrefs()->GetBoolean(
          embedder_support::kAlternateErrorPagesEnabled) ||
      profile_->IsOffTheRecord() || !g_allow_enable_corrections_for_testing) {
    return GURL();
  }

  return google_util::LinkDoctorBaseURL();
}

void NavigationCorrectionTabObserver::OnEnabledChanged() {
  UpdateNavigationCorrectionInfo(web_contents()->GetMainFrame());
}

void NavigationCorrectionTabObserver::UpdateNavigationCorrectionInfo(
    RenderFrameHost* render_frame_host) {
  GURL google_base_url(UIThreadSearchTermsData().GoogleBaseURLValue());
  mojo::AssociatedRemote<chrome::mojom::NavigationCorrector> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetNavigationCorrectionInfo(
      GetNavigationCorrectionURL(),
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale()),
      google_util::GetGoogleCountryCode(google_base_url),
      google_apis::GetAPIKey(),
      google_util::GetGoogleSearchURL(google_base_url));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationCorrectionTabObserver)
