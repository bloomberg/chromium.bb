// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_manager.h"

#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

FieldInfoManagerImpl::FieldInfoManagerImpl(
    scoped_refptr<password_manager::PasswordStore> store)
    : store_(store) {
  store_->GetAllFieldInfo(this);
}

FieldInfoManagerImpl::~FieldInfoManagerImpl() = default;

void FieldInfoManagerImpl::AddFieldType(uint64_t form_signature,
                                        uint32_t field_signature,
                                        autofill::ServerFieldType field_type) {
  field_types_[std::make_pair(form_signature, field_signature)] = field_type;
  store_->AddFieldInfo(
      {form_signature, field_signature, field_type, base::Time::Now()});
}

autofill::ServerFieldType FieldInfoManagerImpl::GetFieldType(
    uint64_t form_signature,
    uint32_t field_signature) const {
  auto it = field_types_.find(std::make_pair(form_signature, field_signature));
  return it == field_types_.end() ? autofill::UNKNOWN_TYPE : it->second;
}

void FieldInfoManagerImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  NOTREACHED();
}

void FieldInfoManagerImpl::OnGetAllFieldInfo(
    std::vector<FieldInfo> field_infos) {
  base::UmaHistogramCounts100("PasswordManager.FieldInfoTableRows",
                              field_infos.size());
  for (const auto& field : field_infos) {
    field_types_[std::make_pair(field.form_signature, field.field_signature)] =
        field.field_type;
  }
}

}  // namespace password_manager
