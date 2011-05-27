// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/eula_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/views_eula_screen_actor.h"

namespace {

const char kGoogleEulaUrl[] = "about:terms";

}  // namespace

namespace chromeos {

EulaScreen::EulaScreen(::WizardScreenDelegate* delegate)
    : WizardScreen(delegate),
      actor_(new ViewsEulaScreenActor(delegate)) {
  actor_->SetDelegate(this);
}

EulaScreen::~EulaScreen() {
}

void EulaScreen::Show() {
  // Command to own the TPM.
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    chromeos::CrosLibrary::Get()->
        GetCryptohomeLibrary()->TpmCanAttemptOwnership();
  } else {
    LOG(ERROR) << "Cros library not loaded. "
               << "We must have disabled the link that led here.";
  }
  actor_->Show();
}

void EulaScreen::Hide() {
  actor_->Hide();
}

gfx::Size EulaScreen::GetScreenSize() const {
  return actor_->GetScreenSize();
}

bool EulaScreen::IsTpmEnabled() const {
  return chromeos::CrosLibrary::Get()->EnsureLoaded() &&
         chromeos::CrosLibrary::Get()->GetCryptohomeLibrary()->TpmIsEnabled();
}

GURL EulaScreen::GetGoogleEulaUrl() const {
  return GURL(kGoogleEulaUrl);
}

GURL EulaScreen::GetOemEulaUrl() const {
  const StartupCustomizationDocument* customization =
      StartupCustomizationDocument::GetInstance();
  if (customization->IsReady()) {
    std::string locale = customization->initial_locale();
    std::string eula_page = customization->GetEULAPage(locale);
    if (!eula_page.empty())
      return GURL(eula_page);

    VLOG(1) << "No eula found for locale: " << locale;
  } else {
    LOG(ERROR) << "No manifest found.";
  }
  return GURL();
}

void EulaScreen::OnExit(bool accepted) {
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->set_usage_statistics_reporting(actor_->IsUsageStatsChecked());
  observer->OnExit(accepted
                   ? ScreenObserver::EULA_ACCEPTED
                   : ScreenObserver::EULA_BACK);
}

std::string* EulaScreen::GetTpmPasswordStorage() {
  return &tpm_password_;
}

bool EulaScreen::IsUsageStatsEnabled() const {
  const ScreenObserver* observer = delegate()->GetObserver(this);
  return observer->usage_statistics_reporting();
}

}  // namespace chromeos
