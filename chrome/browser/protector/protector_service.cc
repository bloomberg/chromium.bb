// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/protector/keys.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"
#include "crypto/hmac.h"

namespace protector {

ProtectorService::ProtectorService(Profile* profile)
    : profile_(profile) {
}

ProtectorService::~ProtectorService() {
  DCHECK(!IsShowingChange());  // Should have been dismissed by Shutdown.
}

void ProtectorService::ShowChange(BaseSettingChange* change) {
  DCHECK(change);
  change_.reset(change);
  DVLOG(1) << "Init change";
  if (!change->Init(profile_)) {
    LOG(WARNING) << "Error while initializing, dismissing change";
    change_.reset();
    return;
  }
  error_.reset(new SettingsChangeGlobalError(change, this));
  error_->ShowForProfile(profile_);
}

bool ProtectorService::IsShowingChange() const {
  return change_.get() != NULL;
}

void ProtectorService::ApplyChange(Browser* browser) {
  DCHECK(IsShowingChange());
  change_->Apply(browser);
  DismissChange();
}

void ProtectorService::DiscardChange(Browser* browser) {
  DCHECK(IsShowingChange());
  change_->Discard(browser);
  DismissChange();
}

void ProtectorService::DismissChange() {
  DCHECK(IsShowingChange());
  error_->RemoveFromProfile();
  DCHECK(!IsShowingChange());
}

void ProtectorService::OpenTab(const GURL& url, Browser* browser) {
  DCHECK(browser);
  browser->ShowSingletonTab(url);
}

void ProtectorService::Shutdown() {
  if (IsShowingChange())
    DismissChange();
}

void ProtectorService::OnApplyChange(Browser* browser) {
  DVLOG(1) << "Apply change";
  DCHECK(IsShowingChange());
  change_->Apply(browser);
}

void ProtectorService::OnDiscardChange(Browser* browser) {
  DVLOG(1) << "Discard change";
  DCHECK(IsShowingChange());
  change_->Discard(browser);
}

void ProtectorService::OnDecisionTimeout() {
  DVLOG(1) << "Timeout";
  DCHECK(IsShowingChange());
  change_->Timeout();
}

void ProtectorService::OnRemovedFromProfile() {
  DCHECK(IsShowingChange());
  error_.reset();
  change_.reset();
}


std::string SignSetting(const std::string& value) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kProtectorSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for signing";
    return std::string();
  }

  std::vector<unsigned char> digest(hmac.DigestLength());
  if (!hmac.Sign(value, &digest[0], digest.size())) {
    LOG(WARNING) << "Failed to sign setting";
    return std::string();
  }

  return std::string(&digest[0], &digest[0] + digest.size());
}

bool IsSettingValid(const std::string& value, const std::string& signature) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kProtectorSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for verification.";
    return false;
  }
  return hmac.Verify(value, signature);
}

bool IsEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoProtector);
}

}  // namespace protector
