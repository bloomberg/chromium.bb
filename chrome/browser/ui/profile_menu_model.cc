// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_menu_model.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ProfileMenuModel::ProfileMenuModel(ui::SimpleMenuModel::Delegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(delegate)) {
  AddItem(COMMAND_CREATE_NEW_PROFILE, l10n_util::GetStringUTF16(
      IDS_PROFILES_CREATE_NEW_PROFILE_OPTION));
}

ProfileMenuModel::~ProfileMenuModel() {
}
