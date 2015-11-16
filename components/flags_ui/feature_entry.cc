// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/feature_entry.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace flags_ui {

std::string FeatureEntry::NameForChoice(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE);
  DCHECK_LT(index, num_choices);
  return std::string(internal_name) + testing::kMultiSeparator +
         base::IntToString(index);
}

base::string16 FeatureEntry::DescriptionForChoice(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE);
  DCHECK_LT(index, num_choices);
  int description_id;
  if (type == FeatureEntry::ENABLE_DISABLE_VALUE ||
      type == FeatureEntry::FEATURE_VALUE) {
    const int kEnableDisableDescriptionIds[] = {
        IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT,
        IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
        IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    };
    description_id = kEnableDisableDescriptionIds[index];
  } else {
    description_id = choices[index].description_id;
  }
  return l10n_util::GetStringUTF16(description_id);
}

namespace testing {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparator[] = "@";

}  // namespace testing

}  // namespace flags_ui
