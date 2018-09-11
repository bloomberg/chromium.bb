// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_creation_util_mac.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_service.h"

namespace os_crypt {

KeyCreationUtilMac::KeyCreationUtilMac(
    PrefService* local_state,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : local_state_(local_state),
      main_thread_task_runner_(main_thread_task_runner),
      key_already_created_(local_state_->GetBoolean(prefs::kKeyCreated)) {}

KeyCreationUtilMac::~KeyCreationUtilMac() = default;

void KeyCreationUtilMac::OnKeyWasStored() {
  if (key_already_created_)
    return;
  key_already_created_ = true;
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PrefService* local_state) {
                       local_state->SetBoolean(prefs::kKeyCreated, true);
                     },
                     local_state_));
}

}  // namespace os_crypt
