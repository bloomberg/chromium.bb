// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/feature_entry.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace flags_ui {
namespace {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparatorChar = '@';

}  // namespace

const char kGenericExperimentChoiceDefault[] = "Default";
const char kGenericExperimentChoiceEnabled[] = "Enabled";
const char kGenericExperimentChoiceDisabled[] = "Disabled";
const char kGenericExperimentChoiceAutomatic[] = "Automatic";

bool FeatureEntry::InternalNameMatches(const std::string& name) const {
  if (!base::StartsWith(name, internal_name, base::CompareCase::SENSITIVE))
    return false;

  const size_t internal_name_length = strlen(internal_name);
  switch (type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
    case FeatureEntry::ORIGIN_LIST_VALUE:
      return name.size() == internal_name_length;

    case FeatureEntry::MULTI_VALUE:
    case FeatureEntry::ENABLE_DISABLE_VALUE:
    case FeatureEntry::FEATURE_VALUE:
    case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
      // Check that the pattern matches what's produced by NameForOption().
      int index = -1;
      return name.size() > internal_name_length + 1 &&
             name[internal_name_length] == kMultiSeparatorChar &&
             base::StringToInt(name.substr(internal_name_length + 1), &index) &&
             index >= 0 && index < num_options;
  }
}

std::string FeatureEntry::NameForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  DCHECK_LT(index, num_options);
  return std::string(internal_name) + testing::kMultiSeparator +
         base::NumberToString(index);
}

base::string16 FeatureEntry::DescriptionForOption(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  DCHECK_LT(index, num_options);
  const char* description = nullptr;
  if (type == FeatureEntry::ENABLE_DISABLE_VALUE ||
      type == FeatureEntry::FEATURE_VALUE) {
    const char* kEnableDisableDescriptions[] = {
        kGenericExperimentChoiceDefault, kGenericExperimentChoiceEnabled,
        kGenericExperimentChoiceDisabled,
    };
    description = kEnableDisableDescriptions[index];
  } else if (type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE) {
    if (index == 0) {
      description = kGenericExperimentChoiceDefault;
    } else if (index == 1) {
      description = kGenericExperimentChoiceEnabled;
    } else if (index < num_options - 1) {
      // First two options do not have variations params.
      int variation_index = index - 2;
      return base::ASCIIToUTF16(
                 base::StringPiece(kGenericExperimentChoiceEnabled)) +
             base::ASCIIToUTF16(" ") +
             base::ASCIIToUTF16(
                 feature_variations[variation_index].description_text);
    } else {
      DCHECK_EQ(num_options - 1, index);
      description = kGenericExperimentChoiceDisabled;
    }
  } else {
    description = choices[index].description;
  }
  return base::ASCIIToUTF16(base::StringPiece(description));
}

const FeatureEntry::Choice& FeatureEntry::ChoiceForOption(int index) const {
  DCHECK_EQ(FeatureEntry::MULTI_VALUE, type);
  DCHECK_LT(index, num_options);

  return choices[index];
}

FeatureEntry::FeatureState FeatureEntry::StateForOption(int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  DCHECK_LT(index, num_options);

  if (index == 0)
    return FeatureEntry::FeatureState::DEFAULT;
  if (index == num_options - 1)
    return FeatureEntry::FeatureState::DISABLED;
  return FeatureEntry::FeatureState::ENABLED;
}

const FeatureEntry::FeatureVariation* FeatureEntry::VariationForOption(
    int index) const {
  DCHECK(type == FeatureEntry::FEATURE_VALUE ||
         type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  DCHECK_LT(index, num_options);

  if (type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE && index > 1 &&
      index < num_options - 1) {
    // We have no variations for FEATURE_VALUE type. Option at |index|
    // corresponds to variation at |index| - 2 as the list starts with "Default"
    // and "Enabled" (with default parameters).
    return &feature_variations[index - 2];
  }
  return nullptr;
}

namespace testing {

const char kMultiSeparator[] = {kMultiSeparatorChar, '\0'};

}  // namespace testing

}  // namespace flags_ui
