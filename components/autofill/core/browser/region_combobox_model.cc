// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/region_combobox_model.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {

RegionComboboxModel::RegionComboboxModel(
    std::unique_ptr<const ::i18n::addressinput::Source> source,
    std::unique_ptr<::i18n::addressinput::Storage> storage,
    const std::string& app_locale,
    const std::string& country_code)
    : app_locale_(app_locale),
      region_data_supplier_(source.release(), storage.release()) {
  region_data_supplier_callback_.reset(::i18n::addressinput::BuildCallback(
      this, &RegionComboboxModel::RegionDataLoaded));
  region_data_supplier_.LoadRules(country_code,
                                  *region_data_supplier_callback_.get());
}

RegionComboboxModel::~RegionComboboxModel() {}

int RegionComboboxModel::GetItemCount() const {
  // The combobox view needs to always have at least one item. If the regions
  // have not been completely loaded yet, we display a single "loading" item.
  // But if we failed to load, we return 0 so that the view can be identified
  // as empty and potentially replaced by another view during ReLayout.
  if (regions_.size() == 0 && !failed_to_load_data_)
    return 1;
  return regions_.size();
}

base::string16 RegionComboboxModel::GetItemAt(int index) {
  DCHECK_GE(index, 0);
  // This might happen because of the asynchonous nature of the data.
  if (static_cast<size_t>(index) >= regions_.size())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_LOADING_REGIONS);

  if (!regions_[index].first.empty())
    return base::UTF8ToUTF16(regions_[index].second);

  // The separator item. Implemented for platforms that don't yet support
  // IsItemSeparatorAt().
  return base::ASCIIToUTF16("---");
}

bool RegionComboboxModel::IsItemSeparatorAt(int index) {
  // This might happen because of the asynchonous nature of the data.
  DCHECK_GE(index, 0);
  if (static_cast<size_t>(index) >= regions_.size())
    return false;
  return regions_[index].first.empty();
}

void RegionComboboxModel::AddObserver(ui::ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void RegionComboboxModel::RemoveObserver(ui::ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RegionComboboxModel::RegionDataLoaded(bool success,
                                           const std::string& country_code,
                                           int rule_count) {
  if (success) {
    failed_to_load_data_ = false;
    std::string best_region_tree_language_tag;
    ::i18n::addressinput::RegionDataBuilder builder(&region_data_supplier_);
    const std::vector<const ::i18n::addressinput::RegionData*>& regions =
        builder.Build(country_code, app_locale_, &best_region_tree_language_tag)
            .sub_regions();
    for (auto* const region : regions) {
      regions_.push_back(std::make_pair(region->key(), region->name()));
    }
  } else {
    // TODO(mad): Maybe use a static list as is done for countries in
    // components\autofill\core\browser\country_data.cc
    failed_to_load_data_ = true;
  }

  for (auto& observer : observers_) {
    observer.OnComboboxModelChanged(this);
  }
}

}  // namespace autofill
