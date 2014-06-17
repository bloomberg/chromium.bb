// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_shared_settings_update.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"

SupervisedUserSharedSettingsUpdate::SupervisedUserSharedSettingsUpdate(
    SupervisedUserSharedSettingsService* service,
    const std::string& su_id,
    const std::string& key,
    scoped_ptr<base::Value> value,
    const base::Callback<void(bool)>& success_callback)
    : service_(service),
      su_id_(su_id),
      key_(key),
      value_(value.Pass()),
      callback_(success_callback) {
  service->SetValueInternal(su_id, key, *value_, false);
  subscription_ = service->Subscribe(
      base::Bind(&SupervisedUserSharedSettingsUpdate::OnSettingChanged,
                 base::Unretained(this)));
}

SupervisedUserSharedSettingsUpdate::~SupervisedUserSharedSettingsUpdate() {}

void SupervisedUserSharedSettingsUpdate::OnSettingChanged(
    const std::string& su_id, const std::string& key) {
  if (su_id != su_id_)
    return;

  if (key != key_)
    return;

  const base::Value* value = service_->GetValue(su_id, key);
  callback_.Run(value->Equals(value_.get()));
}
