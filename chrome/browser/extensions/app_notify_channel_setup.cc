// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_setup.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

AppNotifyChannelSetup::AppNotifyChannelSetup(
    Profile* profile,
    const std::string& extension_id,
    const std::string& client_id,
    const GURL& requestor_url,
    int return_route_id,
    int callback_id,
    AppNotifyChannelUI* ui,
    base::WeakPtr<AppNotifyChannelSetup::Delegate> delegate)
    : profile_(profile),
      extension_id_(extension_id),
      client_id_(client_id),
      requestor_url_(requestor_url),
      return_route_id_(return_route_id),
      callback_id_(callback_id),
      delegate_(delegate),
      ui_(ui) {}

AppNotifyChannelSetup::~AppNotifyChannelSetup() {}

static GURL GetChannelServerURL() {
  // TODO(asargent) - Eventually we'll have a hardcoded url baked in and this
  // will just be an override, but for now we just have this flag as the client
  // and server are both works-in-progress.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppNotifyChannelServerURL)) {
    std::string switch_value = command_line->GetSwitchValueASCII(
        switches::kAppNotifyChannelServerURL);
    GURL result(switch_value);
    if (result.is_valid()) {
      return result;
    } else {
      LOG(ERROR) << "Invalid value for " <<
          switches::kAppNotifyChannelServerURL;
    }
  }
  return GURL();
}

void AppNotifyChannelSetup::Start() {
  AddRef(); // Balanced in ReportResult.

  // Check if the user is logged in to the browser.
  std::string username = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  if (username.empty()) {
    ui_->PromptSyncSetup(this);
    return; // We'll get called back in OnSyncSetupResult
  }

  BeginFetch();
}

void AppNotifyChannelSetup::OnURLFetchComplete(
    const content::URLFetcher* source) {
  CHECK(source);
  net::URLRequestStatus status = source->GetStatus();

  if (status.status() == net::URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == 200) {
    // TODO(asargent) - we need to parse the response from |source| here.
    ReportResult("dummy_do_not_use", "");
  } else {
    ReportResult("", "channel_service_error");
  }
}

void AppNotifyChannelSetup::OnSyncSetupResult(bool enabled) {
  if (enabled) {
    BeginFetch();
  } else {
    ReportResult("", "not_available");
  }
}

void AppNotifyChannelSetup::BeginFetch() {
  GURL channel_server_url = GetChannelServerURL();

  // We return the error via PostTask instead of immediately calling back the
  // delegate because it simplifies tests.
  if (!channel_server_url.is_valid()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AppNotifyChannelSetup::ReportResult, this,
                   std::string(), std::string("not_available")));
    return;
  }

  url_fetcher_.reset(content::URLFetcher::Create(
      0, channel_server_url, content::URLFetcher::POST, this));

  // TODO(asargent) - we eventually want this to use the browser login
  // credentials instead of the regular cookie store, but for now to aid server
  // development, we're just using the regular cookie store.
  url_fetcher_->SetRequestContext(profile_->GetRequestContext());
  std::string data = "client_id=" + EscapeUrlEncodedData(client_id_, true);
  url_fetcher_->SetUploadData("application/x-www-form-urlencoded", data);
  url_fetcher_->Start();
}

void AppNotifyChannelSetup::ReportResult(
    const std::string& channel_id,
    const std::string& error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_.get())
    delegate_->AppNotifyChannelSetupComplete(
        channel_id, error, return_route_id_, callback_id_);
  Release(); // Matches AddRef in Start.
}
