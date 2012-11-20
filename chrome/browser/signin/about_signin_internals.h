// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_
#define CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/common/chrome_version_info.h"

class Profile;
class SigninObserver;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
//
// It is componentized within SigninManager and is typically accessed
// using SigninManagerFactory::GetForProfile(profile)->about_signin_internals()
class AboutSigninInternals  {
 public:
  AboutSigninInternals();
  virtual ~AboutSigninInternals();

  // Each instance of SigninInternalsUI adds itself as an observer to this
  // and receives updates on all changes that AboutSigninInternals receives.
  void AddSigninObserver(SigninObserver* observer);
  void RemoveSigninObserver(SigninObserver* observer);

  void Initialize(Profile* profile) {
    profile_ = profile;
  }

 private:
  Profile* profile_;

  ObserverList<SigninObserver> signin_observers_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_
