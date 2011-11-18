// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/protector/keys.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
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

TemplateURLService* Protector::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

void Protector::ShowChange(SettingChange* change) {
  DCHECK(change);
  SettingChangeVector changes(1, change);

  error_.reset(new SettingsChangeGlobalError(changes, this));
  error_->ShowForProfile(profile_);
}

void Protector::OnApplyChanges() {
  OnChangesAction(&SettingChange::Accept);
}

void Protector::OnDiscardChanges() {
  OnChangesAction(&SettingChange::Revert);
}

void Protector::OnDecisionTimeout() {
  OnChangesAction(&SettingChange::DoDefault);
}

void Protector::OnRemovedFromProfile() {
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

void Protector::OnChangesAction(SettingChangeAction action) {
  DCHECK(error_.get());
  SettingChangeVector* changes = error_->mutable_changes();
  for (SettingChangeVector::iterator it = changes->begin();
       it != changes->end(); ++it)
    ((*it)->*action)(this);
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

}  // namespace protector
