// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/update_observer.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

UpdateObserver::UpdateObserver(Profile* profile)
    : notification_(profile, "update.chromeos", IDR_NOTIFICATION_UPDATE,
                    l10n_util::GetStringUTF16(IDS_UPDATE_TITLE)) {}

UpdateObserver::~UpdateObserver() {
  notification_.Hide();
}

void UpdateObserver::UpdateStatusChanged(UpdateLibrary* library) {
#if 0
  // TODO seanparent@chromium.org : This update should only be shown when an
  // update is critical and should include a restart button using the
  // update_engine restart API. Currently removed entirely per Kan's request.

  if (library->status().status == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    notification_.Show(l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED), true);
  }
#endif
}

}  // namespace chromeos
