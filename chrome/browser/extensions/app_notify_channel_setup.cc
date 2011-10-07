// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_setup.h"

#include "content/browser/browser_thread.h"

AppNotifyChannelSetup::AppNotifyChannelSetup(
    int request_id,
    const std::string& client_id,
    const GURL& requestor_url,
    base::WeakPtr<AppNotifyChannelSetup::Delegate> delegate)
    : request_id_(request_id),
      client_id_(client_id),
      requestor_url_(requestor_url),
      delegate_(delegate) {}

AppNotifyChannelSetup::~AppNotifyChannelSetup() {}

void AppNotifyChannelSetup::Start() {
  AddRef(); // Balanced in ReportResult.


  // TODO(asargent) - We will eventually check here whether the user is logged
  // in to the browser or not. If they are, we'll make a request to a server
  // with the browser login credentials to get a channel id for the app to use
  // in server pushed notifications. If they are not logged in, we'll prompt
  // for login and if they sign in, then continue as in the first case.
  // Otherwise we'll return an error message.

  // For now, just reply with an error of 'not_implemented'.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this,
                        &AppNotifyChannelSetup::ReportResult,
                        std::string(),
                        std::string("not_implemented")));
}

void AppNotifyChannelSetup::ReportResult(
    const std::string& channel_id,
    const std::string& error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_.get())
    delegate_->AppNotifyChannelSetupComplete(request_id_, channel_id, error);
  Release(); // Matches AddRef in Start.
}
