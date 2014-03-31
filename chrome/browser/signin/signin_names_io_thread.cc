// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_names_io_thread.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"

SigninNamesOnIOThread::SigninNamesOnIOThread() : resources_released_(false) {
  CheckOnUIThread();

  SigninManagerFactory::GetInstance()->AddObserver(this);

  // Get list of profiles and record the email addresses of any connected
  // accounts.
  if (g_browser_process) {
    ProfileManager* manager = g_browser_process->profile_manager();
    if (manager) {
      const ProfileInfoCache& cache = manager->GetProfileInfoCache();
      for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
        base::string16 email = cache.GetUserNameOfProfileAtIndex(i);
        if (!email.empty())
          emails_.insert(email);
      }
    }
  }
}

SigninNamesOnIOThread::~SigninNamesOnIOThread() {
  CheckOnIOThread();
  DCHECK(resources_released_) << "Must call ReleaseResourcesOnUIThread() first";
  DCHECK(!observed_managers_.size())
      << "Shouldn't be observing any SigninManagers";
}

const SigninNamesOnIOThread::EmailSet&
    SigninNamesOnIOThread::GetEmails() const {
  CheckOnIOThread();
  return emails_;
}

void SigninNamesOnIOThread::ReleaseResourcesOnUIThread() {
  CheckOnUIThread();
  DCHECK(!resources_released_);
  SigninManagerFactory::GetInstance()->RemoveObserver(this);

  for (std::set<SigninManagerBase*>::iterator i = observed_managers_.begin();
       i != observed_managers_.end();
       ++i) {
    (*i)->RemoveObserver(this);
  }
  observed_managers_.clear();

  resources_released_ = true;
}

void SigninNamesOnIOThread::SigninManagerCreated(SigninManagerBase* manager) {
  manager->AddObserver(this);
  observed_managers_.insert(manager);
}

void SigninNamesOnIOThread::SigninManagerShutdown(SigninManagerBase* manager) {
  manager->RemoveObserver(this);
  observed_managers_.erase(manager);
}

void SigninNamesOnIOThread::GoogleSigninSucceeded(const std::string& username,
                                                  const std::string& password) {
  PostTaskToIOThread(true, base::UTF8ToUTF16(username));
}

void SigninNamesOnIOThread::GoogleSignedOut(const std::string& username) {
  PostTaskToIOThread(false, base::UTF8ToUTF16(username));
}

void SigninNamesOnIOThread::CheckOnIOThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void SigninNamesOnIOThread::CheckOnUIThread() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void SigninNamesOnIOThread::PostTaskToIOThread(bool add,
                                               const base::string16& email) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    UpdateOnIOThread(add, email);
  } else {
    bool may_run = content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SigninNamesOnIOThread::UpdateOnIOThread,
                   base::Unretained(this),
                   add,
                   email));
    DCHECK(may_run);
  }
}

void SigninNamesOnIOThread::UpdateOnIOThread(bool add,
                                             const base::string16& email) {
  CheckOnIOThread();
  if (add) {
    emails_.insert(email);
  } else {
    emails_.erase(email);
  }
}
