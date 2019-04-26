// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_utils.h"

#include <utility>

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AccessorySheetData CreateAccessorySheetData(base::string16 title,
                                            std::vector<UserInfo> user_info) {
  // TODO(crbug.com/902425): Remove hard-coded enum value
  AccessorySheetData data(FallbackSheetType::PASSWORD, std::move(title));
  for (auto& i : user_info) {
    data.add_user_info(std::move(i));
  }

  // TODO(crbug.com/902425): Generalize options (both adding to footer, and
  // handling selection).
  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  data.add_footer_command(FooterCommand(manage_passwords_title));

  return data;
}

}  // namespace autofill
