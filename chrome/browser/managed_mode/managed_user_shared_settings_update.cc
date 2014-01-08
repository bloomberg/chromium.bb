// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_shared_settings_update.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"

ManagedUserSharedSettingsUpdate::ManagedUserSharedSettingsUpdate(
    ManagedUserSharedSettingsService* service,
    const std::string& mu_id,
    const std::string& key,
    scoped_ptr<base::Value> value,
    const base::Callback<void(bool)>& success_callback)
    : service_(service),
      mu_id_(mu_id),
      key_(key),
      value_(value.Pass()),
      callback_(success_callback) {
  service->SetValueInternal(mu_id, key, *value_, false);
  subscription_ = service->Subscribe(
      base::Bind(&ManagedUserSharedSettingsUpdate::OnSettingChanged,
                 base::Unretained(this)));
}

ManagedUserSharedSettingsUpdate::~ManagedUserSharedSettingsUpdate() {}

void ManagedUserSharedSettingsUpdate::OnSettingChanged(const std::string& mu_id,
                                                       const std::string& key) {
  if (mu_id != mu_id_)
    return;

  if (key != key_)
    return;

  const base::Value* value = service_->GetValue(mu_id, key);
  callback_.Run(value->Equals(value_.get()));
}
