// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_setup.h"

#include "content/browser/browser_thread.h"

AppNotifyChannelSetup::AppNotifyChannelSetup(
    const std::string& client_id,
    const GURL& requestor_url,
    int return_route_id,
    int callback_id,
    base::WeakPtr<AppNotifyChannelSetup::Delegate> delegate)
    : client_id_(client_id),
      requestor_url_(requestor_url),
      return_route_id_(return_route_id),
      callback_id_(callback_id),
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
    delegate_->AppNotifyChannelSetupComplete(
        channel_id, error, return_route_id_, callback_id_);
  Release(); // Matches AddRef in Start.
}
