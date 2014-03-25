// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"

// This class provides access to a list of email addresses for all profiles
// connected to a Google account, from the IO thread.  The main purpose of
// this is to be a member of the ProfileIOData structure to allow access to
// the list of email addresses, and is used by the one-click sign in code to
// know which Google accounts are already in use.
//
// Instance of this class are created and initialized on the UI
// thread as part of the creation of ProfileIOData, and destroyed along with
// ProfileIOData.
class SigninNamesOnIOThread : public SigninManagerBase::Observer,
                              public SigninManagerFactory::Observer {
 public:
  typedef std::set<base::string16> EmailSet;

  // Objects should only be created on UI thread.
  SigninNamesOnIOThread();
  virtual ~SigninNamesOnIOThread();

  // Gets the set of email addresses of connected profiles.  This method should
  // only be called on the IO thread.
  const EmailSet& GetEmails() const;

  // Releases all resource held by object. Once released, GetEmails() no
  // longer works.
  void ReleaseResourcesOnUIThread();

 private:
  // SigninManagerBase::Observer:
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  // SigninManagerFactory::Observer:
  virtual void SigninManagerCreated(SigninManagerBase* manager) OVERRIDE;
  virtual void SigninManagerShutdown(SigninManagerBase* manager) OVERRIDE;

  // Checks whether the current thread is the IO thread.
  void CheckOnIOThread() const;

  // Checks whether the current thread is the UI thread.
  void CheckOnUIThread() const;

  // Posts a task to the I/O thread to add or remove the email address from
  // the set.
  void PostTaskToIOThread(bool add, const base::string16& email);

  // Updates the email set on the I/O thread.  Protected for testing.
  void UpdateOnIOThread(bool add, const base::string16& email);

  EmailSet emails_;

  // The set of SigninManagerBase instances that this instance is currently
  // observing. Can only be accessed on the UI thread.
  std::set<SigninManagerBase*> observed_managers_;
  bool resources_released_;

  DISALLOW_COPY_AND_ASSIGN(SigninNamesOnIOThread);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_
