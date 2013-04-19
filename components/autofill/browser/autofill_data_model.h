// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_DATA_MODEL_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_DATA_MODEL_H_

#include <string>

#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/form_group.h"

namespace autofill {

class AutofillField;
struct FormFieldData;

// This class is an interface for the primary data models that back Autofill.
// The information in objects of this class is managed by the
// PersonalDataManager.
class AutofillDataModel : public FormGroup {
 public:
  explicit AutofillDataModel(const std::string& guid);
  virtual ~AutofillDataModel();

  // Set |field_data|'s value based on |field| and contents of |this| (using
  // data variant |variant|).
  virtual void FillFormField(const AutofillField& field,
                             size_t variant,
                             const std::string& app_locale,
                             FormFieldData* field_data) const = 0;

  // Fills in select control with data matching |type| from |this|.
  // Public for testing purposes.
  void FillSelectControl(AutofillFieldType type,
                         const std::string& app_locale,
                         FormFieldData* field_data) const;

  std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

 protected:
  // Fills in a select control for a country from data in |this|. Returns true
  // for success.
  virtual bool FillCountrySelectControl(const std::string& app_locale,
                                        FormFieldData* field_data) const;

 private:
  // A globally unique ID for this object.
  std::string guid_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_DATA_MODEL_H_
