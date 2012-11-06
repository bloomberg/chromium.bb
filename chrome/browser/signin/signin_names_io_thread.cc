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

SigninNamesOnIOThread::Internal::Internal() {
}

void SigninNamesOnIOThread::Internal::Initialize() {
  CheckOnUIThread();

  // We want to register for all profiles, not just for the current profile.
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
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

const SigninNamesOnIOThread::EmailSet&
    SigninNamesOnIOThread::Internal::GetEmails() const {
  CheckOnIOThread();
  return emails_;
}

SigninNamesOnIOThread::Internal::~Internal() {
  registrar_.RemoveAll();
}

void SigninNamesOnIOThread::Internal::UpdateOnIOThread(
    int type,
    const string16& email) {
  CheckOnIOThread();
  if (type == chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL) {
    emails_.insert(email);
  } else {
    emails_.erase(email);
  }
}

void SigninNamesOnIOThread::Internal::Observe(
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

void SigninNamesOnIOThread::Internal::CheckOnIOThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void SigninNamesOnIOThread::Internal::CheckOnUIThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void SigninNamesOnIOThread::Internal::PostTaskToIOThread(
    int type,
    const string16& email) {
  bool may_run = content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&SigninNamesOnIOThread::Internal::UpdateOnIOThread, this,
                     type, email));
  DCHECK(may_run);
}

SigninNamesOnIOThread::SigninNamesOnIOThread() {
}

SigninNamesOnIOThread::~SigninNamesOnIOThread() {
  DCHECK(!internal_) << "Forgot to call ReleaseResources()";
}

const SigninNamesOnIOThread::EmailSet&
    SigninNamesOnIOThread::GetEmails() const {
  DCHECK(internal_);
  return internal_->GetEmails();
}

void SigninNamesOnIOThread::ReleaseResources() {
  DCHECK(internal_);
  internal_ = NULL;
}

void SigninNamesOnIOThread::Initialize() {
  internal_ = CreateInternal();
  internal_->Initialize();
}

SigninNamesOnIOThread::Internal* SigninNamesOnIOThread::CreateInternal() {
  return new Internal();
}
