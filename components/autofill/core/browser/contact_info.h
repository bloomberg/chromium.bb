// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_INFO_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/form_group.h"

namespace autofill {

// A form group that stores name information.
class NameInfo : public FormGroup {
 public:
  NameInfo();
  NameInfo(const NameInfo& info);
  virtual ~NameInfo();

  NameInfo& operator=(const NameInfo& info);

  // FormGroup:
  virtual base::string16 GetRawInfo(ServerFieldType type) const OVERRIDE;
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) OVERRIDE;

 private:
  // FormGroup:
  virtual void GetSupportedTypes(
      ServerFieldTypeSet* supported_types) const OVERRIDE;

  // Returns the full name, which can include up to the first, middle, and last
  // name.
  base::string16 FullName() const;

  // Returns the middle initial if |middle_| is non-empty.  Returns an empty
  // string otherwise.
  base::string16 MiddleInitial() const;

  const base::string16& first() const { return first_; }
  const base::string16& middle() const { return middle_; }
  const base::string16& last() const { return last_; }

  // Sets |first_|, |middle_|, and |last_| to the tokenized |full|.
  // It is tokenized on a space only.
  void SetFullName(const base::string16& full);

  base::string16 first_;
  base::string16 middle_;
  base::string16 last_;
};

class EmailInfo : public FormGroup {
 public:
  EmailInfo();
  EmailInfo(const EmailInfo& info);
  virtual ~EmailInfo();

  EmailInfo& operator=(const EmailInfo& info);

  // FormGroup:
  virtual base::string16 GetRawInfo(ServerFieldType type) const OVERRIDE;
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) OVERRIDE;

 private:
  // FormGroup:
  virtual void GetSupportedTypes(
      ServerFieldTypeSet* supported_types) const OVERRIDE;

  base::string16 email_;
};

class CompanyInfo : public FormGroup {
 public:
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  virtual ~CompanyInfo();

  CompanyInfo& operator=(const CompanyInfo& info);

  // FormGroup:
  virtual base::string16 GetRawInfo(ServerFieldType type) const OVERRIDE;
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) OVERRIDE;

 private:
  // FormGroup:
  virtual void GetSupportedTypes(
      ServerFieldTypeSet* supported_types) const OVERRIDE;

  base::string16 company_name_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_INFO_H_
