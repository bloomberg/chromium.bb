// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals.h"

#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "google_apis/gaia/gaia_constants.h"

AboutSigninInternals::AboutSigninInternals() : profile_(NULL) {}

AboutSigninInternals::~AboutSigninInternals() {
  DCHECK(signin_observers_.size() == 0);
}

void AboutSigninInternals::AddSigninObserver(SigninObserver* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveSigninObserver(SigninObserver* observer) {
  signin_observers_.RemoveObserver(observer);
}
