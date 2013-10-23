// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_MODEL_H_

#include <string>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/form_group.h"

namespace autofill {

class AutofillType;

// This class is an interface for the primary data models that back Autofill.
// The information in objects of this class is managed by the
// PersonalDataManager.
class AutofillDataModel : public FormGroup {
 public:
  AutofillDataModel(const std::string& guid, const std::string& origin);
  virtual ~AutofillDataModel();

  // Returns the string that should be auto-filled into a text field given the
  // |type| of that field, localized to the given |app_locale| if appropriate.
  // If the data model supports multiple values for the given |type|, returns
  // the |variant|th value for the |type|.
  virtual base::string16 GetInfoForVariant(const AutofillType& type,
                                           size_t variant,
                                           const std::string& app_locale) const;

  // Returns true if the data in this model was entered directly by the user,
  // rather than automatically aggregated.
  bool IsVerified() const;

  std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

  std::string origin() const { return origin_; }
  void set_origin(const std::string& origin) { origin_ = origin; }

 private:
  // A globally unique ID for this object.
  std::string guid_;

  // The origin of this data.  This should be
  //   (a) a web URL for the domain of the form from which the data was
  //       automatically aggregated, e.g. https://www.example.com/register,
  //   (b) some other non-empty string, which cannot be interpreted as a web
  //       URL, identifying the origin for non-aggregated data, or
  //   (c) an empty string, indicating that the origin for this data is unknown.
  std::string origin_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_MODEL_H_
