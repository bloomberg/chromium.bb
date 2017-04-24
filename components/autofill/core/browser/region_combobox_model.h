// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/models/combobox_model.h"

namespace i18n {
namespace addressinput {
class RegionData;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

// An interface to wrap the loading of region data so it can be mocked in tests.
class RegionDataLoader {
 public:
  // The signature of the function to be called when the region data is loaded.
  // When the loading request timee out or other failure occure, |regions| is
  // empty.
  typedef base::Callback<void(
      const std::vector<const ::i18n::addressinput::RegionData*>& regions)>
      RegionDataLoaded;

  virtual ~RegionDataLoader() {}
  // Calls |loaded_callback| when the region data for |country_code| is ready or
  // when |timeout_ms| miliseconds have passed. This may happen synchronously.
  virtual void LoadRegionData(const std::string& country_code,
                              RegionDataLoaded callback,
                              int64_t timeout_ms) = 0;
  // To forget about the |callback| givent to LoadRegionData, in cases where
  // callback owner is destroyed before loader.
  virtual void ClearCallback() = 0;
};

// A model for country regions (aka state/provinces) to be used to enter
// addresses. Note that loading these regions can happen asynchronously so a
// ui::ComboboxModelObserver should be attached to this model to be updated when
// the regions load is completed.
class RegionComboboxModel : public ui::ComboboxModel {
 public:
  RegionComboboxModel();
  ~RegionComboboxModel() override;

  void LoadRegionData(const std::string& country_code,
                      RegionDataLoader* region_data_loader,
                      int64_t timeout_ms);

  bool IsPendingRegionDataLoad() const {
    return region_data_loader_ != nullptr;
  }

  bool failed_to_load_data() const { return failed_to_load_data_; }

  // ui::ComboboxModel implementation:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  void AddObserver(ui::ComboboxModelObserver* observer) override;
  void RemoveObserver(ui::ComboboxModelObserver* observer) override;

 private:
  // Callback for the RegionDataLoader.
  void OnRegionDataLoaded(
      const std::vector<const ::i18n::addressinput::RegionData*>& regions);

  // Whether the region data load failed or not.
  bool failed_to_load_data_;

  // Lifespan not owned by RegionComboboxModel, but guaranteed to be alive up to
  // a call to OnRegionDataLoaded where soft ownership must be released.
  RegionDataLoader* region_data_loader_;

  // List of <code, name> pairs for ADDRESS_HOME_STATE combobox values;
  std::vector<std::pair<std::string, std::string>> regions_;

  // To be called when the data for the given country code was loaded.
  base::ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RegionComboboxModel);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_
