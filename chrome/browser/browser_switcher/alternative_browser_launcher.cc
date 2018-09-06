// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace browser_switcher {

AlternativeBrowserLauncher::AlternativeBrowserLauncher(PrefService* prefs)
    : AlternativeBrowserLauncher(
          prefs,
          std::make_unique<AlternativeBrowserDriverImpl>()) {}

AlternativeBrowserLauncher::AlternativeBrowserLauncher(
    PrefService* prefs,
    std::unique_ptr<AlternativeBrowserDriver> driver)
    : prefs_(prefs), driver_(std::move(driver)) {
  DCHECK(prefs_);
  DCHECK(driver_);
  change_registrar_.Init(prefs);
  change_registrar_.Add(
      prefs::kAlternativeBrowserPath,
      base::BindRepeating(&AlternativeBrowserLauncher::OnAltBrowserPathChanged,
                          base::Unretained(this)));
  change_registrar_.Add(
      prefs::kAlternativeBrowserParameters,
      base::BindRepeating(
          &AlternativeBrowserLauncher::OnAltBrowserParametersChanged,
          base::Unretained(this)));
  // Ensure |alt_browser_| is initialized.
  OnAltBrowserPathChanged();
  OnAltBrowserParametersChanged();
}

AlternativeBrowserLauncher::~AlternativeBrowserLauncher() {}

void AlternativeBrowserLauncher::OnAltBrowserPathChanged() {
  // This string could be a variable, e.g. "${ie}". Let the driver decide what
  // to do with it.
  driver_->SetBrowserPath(prefs_->GetString(prefs::kAlternativeBrowserPath));
}

void AlternativeBrowserLauncher::OnAltBrowserParametersChanged() {
  // This string could contain a placeholder, e.g. "${url}". Let the driver
  // decide what to do with it.
  driver_->SetBrowserParameters(
      prefs_->GetString(prefs::kAlternativeBrowserParameters));
}

bool AlternativeBrowserLauncher::Launch(const GURL& url) const {
  return driver_->TryLaunch(url);
}

}  // namespace browser_switcher
