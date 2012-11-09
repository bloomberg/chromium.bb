// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_names_io_thread.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

SigninNamesOnIOThread::SigninNamesOnIOThread() {
  CheckOnUIThread();

  // We want to register for all profiles, not just for the current profile.
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                  content::NotificationService::AllSources());
  registrar_->Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                  content::NotificationService::AllSources());

  // Get list of profiles and record the email addresses of any connected
  // accounts.
  if (g_browser_process) {
    ProfileManager* manager = g_browser_process->profile_manager();
    if (manager) {
      const ProfileInfoCache& cache = manager->GetProfileInfoCache();
      for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
        string16 email = cache.GetUserNameOfProfileAtIndex(i);
        if (!email.empty())
          emails_.insert(email);
      }
    }
  }
}

SigninNamesOnIOThread::~SigninNamesOnIOThread() {
  CheckOnIOThread();
  DCHECK(!registrar_) << "Must call ReleaseResourcesOnUIThread() first";
}

const SigninNamesOnIOThread::EmailSet&
    SigninNamesOnIOThread::GetEmails() const {
  CheckOnIOThread();
  return emails_;
}

void SigninNamesOnIOThread::ReleaseResourcesOnUIThread() {
  CheckOnUIThread();
  registrar_.reset();
}

void SigninNamesOnIOThread::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CheckOnUIThread();

  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* signin_details =
          content::Details<GoogleServiceSigninSuccessDetails>(details).ptr();
      PostTaskToIOThread(type, UTF8ToUTF16(signin_details->username));
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT: {
      const GoogleServiceSignoutDetails* signout_details =
          content::Details<GoogleServiceSignoutDetails>(details).ptr();
      PostTaskToIOThread(type, UTF8ToUTF16(signout_details->username));
      break;
    }
    default:
      NOTREACHED() << "Unexpected type=" << type;
  }
}

void SigninNamesOnIOThread::CheckOnIOThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void SigninNamesOnIOThread::CheckOnUIThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void SigninNamesOnIOThread::PostTaskToIOThread(
    int type,
    const string16& email) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    UpdateOnIOThread(type, email);
  } else {
    bool may_run = content::BrowserThread::PostTask(
            content::BrowserThread::IO,
            FROM_HERE,
            base::Bind(&SigninNamesOnIOThread::UpdateOnIOThread,
                       base::Unretained(this), type, email));
    DCHECK(may_run);
  }
}

void SigninNamesOnIOThread::UpdateOnIOThread(
    int type,
    const string16& email) {
  CheckOnIOThread();
  if (type == chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL) {
    emails_.insert(email);
  } else {
    emails_.erase(email);
  }
}
