// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"

ChromeTabRestoreServiceClient::ChromeTabRestoreServiceClient(Profile* profile)
    : profile_(profile) {}

ChromeTabRestoreServiceClient::~ChromeTabRestoreServiceClient() {}

bool ChromeTabRestoreServiceClient::HasLastSession() {
#if defined(ENABLE_SESSION_SERVICE)
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  Profile::ExitType exit_type = profile_->GetLastSessionExitType();
  // The previous session crashed and wasn't restored, or was a forced
  // shutdown. Both of which won't have notified us of the browser close so
  // that we need to load the windows from session service (which will have
  // saved them).
  return (!profile_->restored_last_session() && session_service &&
          (exit_type == Profile::EXIT_CRASHED ||
           exit_type == Profile::EXIT_SESSION_ENDED));
#else
  return false;
#endif
}

void ChromeTabRestoreServiceClient::GetLastSession(
    const sessions::GetLastSessionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(HasLastSession());
#if defined(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetForProfile(profile_)
      ->GetLastSession(callback, tracker);
#endif
}
