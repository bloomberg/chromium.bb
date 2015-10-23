// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/browser/crashes_ui_util.h"

#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/upload_list/upload_list.h"
#include "grit/components_chromium_strings.h"
#include "grit/components_google_chrome_strings.h"
#include "grit/components_strings.h"

namespace crash {

const CrashesUILocalizedString kCrashesUILocalizedStrings[] = {
    {"bugLinkText", IDS_CRASH_BUG_LINK_LABEL},
    {"crashCountFormat", IDS_CRASH_CRASH_COUNT_BANNER_FORMAT},
    {"crashHeaderFormat", IDS_CRASH_CRASH_HEADER_FORMAT},
    {"crashTimeFormat", IDS_CRASH_CRASH_TIME_FORMAT},
    {"crashesTitle", IDS_CRASH_TITLE},
    {"disabledHeader", IDS_CRASH_DISABLED_HEADER},
    {"disabledMessage", IDS_CRASH_DISABLED_MESSAGE},
    {"noCrashesMessage", IDS_CRASH_NO_CRASHES_MESSAGE},
    {"uploadCrashesLinkText", IDS_CRASH_UPLOAD_MESSAGE},
};

const size_t kCrashesUILocalizedStringsCount =
    arraysize(kCrashesUILocalizedStrings);

const char kCrashesUICrashesJS[] = "crashes.js";
const char kCrashesUIRequestCrashList[] = "requestCrashList";
const char kCrashesUIRequestCrashUpload[] = "requestCrashUpload";
const char kCrashesUIShortProductName[] = "shortProductName";
const char kCrashesUIUpdateCrashList[] = "updateCrashList";

void UploadListToValue(UploadList* upload_list, base::ListValue* out_value) {
  std::vector<UploadList::UploadInfo> crashes;
  upload_list->GetUploads(50, &crashes);

  for (const auto& info : crashes) {
    base::DictionaryValue* crash = new base::DictionaryValue();
    crash->SetString("id", info.upload_id);
    crash->SetString("time",
                     base::TimeFormatFriendlyDateAndTime(info.upload_time));
    crash->SetString("local_id", info.local_id);
    out_value->Append(crash);
  }
}

}  // namespace crash
