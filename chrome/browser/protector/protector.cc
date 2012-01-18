// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector.h"

#include "base/bind.h"
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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "crypto/hmac.h"

using content::BrowserThread;

namespace protector {

Protector::Protector(Profile* profile)
    : profile_(profile) {
}

Protector::~Protector() {
}

void Protector::OpenTab(const GURL& url) {
  if (!error_.get() || !error_->browser()) {
    LOG(WARNING) << "Don't have browser to show tab in.";
    return;
  }
  error_->browser()->ShowSingletonTab(url);
}

void Protector::ShowChange(BaseSettingChange* change) {
  DCHECK(change);
  change_.reset(change);
  DVLOG(1) << "Init change";
  if (!change->Init(this)) {
    LOG(WARNING) << "Error while initializing, removing ourselves";
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
    return;
  }
  error_.reset(new SettingsChangeGlobalError(change, this));
  error_->ShowForProfile(profile_);
}

void Protector::DismissChange() {
  DCHECK(error_.get());
  error_->RemoveFromProfile();
}

void Protector::OnApplyChange() {
  DVLOG(1) << "Apply change";
  change_->Apply();
}

void Protector::OnDiscardChange() {
  DVLOG(1) << "Discard change";
  change_->Discard();
}

void Protector::OnDecisionTimeout() {
  DVLOG(1) << "Timeout";
  change_->Timeout();
}

void Protector::OnRemovedFromProfile() {
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
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

void RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kProtectorEnabled, false);
}

bool IsEnabled() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableProtector))
    return false;
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(prefs::kProtectorEnabled);
}

}  // namespace protector
