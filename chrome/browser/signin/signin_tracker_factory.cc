// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker_factory.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "services/identity/public/cpp/identity_manager.h"

SigninTrackerFactory::SigninTrackerFactory() {}
SigninTrackerFactory::~SigninTrackerFactory() {}

// static
std::unique_ptr<SigninTracker> SigninTrackerFactory::CreateForProfile(
    Profile* profile,
    SigninTracker::Observer* observer) {
  return std::unique_ptr<SigninTracker>(new SigninTracker(
      IdentityManagerFactory::GetForProfile(profile), observer));
}
