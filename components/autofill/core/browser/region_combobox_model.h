// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

// A model for country regions (aka state/provinces) to be used to enter
// addresses. Note that loading these regions can happen asynchronously so a
// ui::ComboboxModelObserver should be attached to this model to be updated when
// the regions load is completed.
class RegionComboboxModel : public ui::ComboboxModel {
 public:
  // |source| and |storage| are needed to initialize the
  // ::i18n::addressinput::PreloadSupplier, |app_locale| is needed for
  // ::i18n::addressinput::RegionDataBuilder and |country_code| identifies which
  // country's region to load into the model.
  RegionComboboxModel(
      std::unique_ptr<const ::i18n::addressinput::Source> source,
      std::unique_ptr<::i18n::addressinput::Storage> storage,
      const std::string& app_locale,
      const std::string& country_code);
  ~RegionComboboxModel() override;

  // ui::ComboboxModel implementation:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  void AddObserver(ui::ComboboxModelObserver* observer) override;
  void RemoveObserver(ui::ComboboxModelObserver* observer) override;

  // Callback for ::i18n::addressinput::PreloadSupplier::LoadRules
  void RegionDataLoaded(bool success, const std::string&, int rule_count);

 private:
  // Whether the region data load failed or not.
  bool failed_to_load_data_{false};

  // The application locale.
  const std::string app_locale_;

  // The callback to give to |region_data_supplier_| for async operations.
  ::i18n::addressinput::scoped_ptr<
      ::i18n::addressinput::PreloadSupplier::Callback>
      region_data_supplier_callback_;

  // A supplier of region data.
  ::i18n::addressinput::PreloadSupplier region_data_supplier_;

  // List of <code, name> pairs for ADDRESS_HOME_STATE combobox values;
  std::vector<std::pair<std::string, std::string>> regions_;

  // To be called when the data for the given country code was loaded.
  base::ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RegionComboboxModel);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_REGION_COMBOBOX_MODEL_H_
