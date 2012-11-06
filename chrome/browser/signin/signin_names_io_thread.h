// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class NotificationDetails;
class NotificationSource;
}


// This class provides access to a list of email addresses for all profiles
// connected to a Google account, from the IO thread.  The main purpose of
// this is to be a member of the ProfileIOData structure to allow access to
// the list of email addresses, and is used by the one-click sign in code to
// know which Google accounts are already in use.
//
// Instance of this class are created, initialized, and destroyed on the UI
// thread, as part of the creation/destruction of ProfileIOData.
class SigninNamesOnIOThread {
 public:
  typedef std::set<string16> EmailSet;

  SigninNamesOnIOThread();
  virtual ~SigninNamesOnIOThread();

  // Should be called on the UI thread to initialize the object, right after it
  // is constructed.
  void Initialize();

  // Gets the set of email addresses of connected profiles.  This method should
  // only be called on the IO thread.
  const EmailSet& GetEmails() const;

  // Releases all resource held by object. Once released, GetEmails() no
  // longer works.
  void ReleaseResources();

  // Internal implementation.  This class is public for testing.
  class Internal : public base::RefCountedThreadSafe<Internal>,
                   public content::NotificationObserver {
   public:
    Internal();

    // Should be called on the UI thread to initialize the object, right after
    // it is constructed.
    void Initialize();

    const EmailSet& GetEmails() const;

   protected:
    virtual ~Internal();

    // Updates the email set on the I/O thread.  Protected for testing.
    void UpdateOnIOThread(int type, const string16& email);

   private:
    friend class base::RefCountedThreadSafe<Internal>;

    // content::NotificationObserver
    virtual void Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) OVERRIDE;

    // Checks whether the current thread is the IO thread.
    virtual void CheckOnIOThread() const;

    // Checks whether the current thread is the UI thread.
    virtual void CheckOnUIThread() const;

    // Posts a task to the I/O thread to add or remove the email address from
    // the set.
    virtual void PostTaskToIOThread(int type, const string16& email);

    content::NotificationRegistrar registrar_;
    EmailSet emails_;

    DISALLOW_COPY_AND_ASSIGN(Internal);
  };

 private:
  // Creates an instance of Internal.  Overridden in tests.
  virtual Internal* CreateInternal();

  scoped_refptr<Internal> internal_;

  DISALLOW_COPY_AND_ASSIGN(SigninNamesOnIOThread);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_NAMES_IO_THREAD_H_
