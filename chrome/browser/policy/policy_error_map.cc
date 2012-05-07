// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_error_map.h"

#include <utility>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

struct PolicyErrorMap::PendingError {
  PendingError(const std::string& policy,
               const std::string& subkey,
               int index,
               int message_id,
               const std::string& replacement)
      : policy(policy),
        subkey(subkey),
        index(index),
        message_id(message_id),
        has_replacement(true),
        replacement(replacement) {}

  PendingError(const std::string& policy,
               const std::string& subkey,
               int index,
               int message_id)
      : policy(policy),
        subkey(subkey),
        index(index),
        message_id(message_id),
        has_replacement(false) {}

  std::string policy;
  std::string subkey;
  int index;
  int message_id;
  bool has_replacement;
  std::string replacement;
};

PolicyErrorMap::PolicyErrorMap() {
}

PolicyErrorMap::~PolicyErrorMap() {
}

bool PolicyErrorMap::IsReady() const {
  return ui::ResourceBundle::HasSharedInstance();
}

void PolicyErrorMap::AddError(const std::string& policy, int message_id) {
  AddError(PendingError(policy, std::string(), -1, message_id));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id) {
  AddError(PendingError(policy, subkey, -1, message_id));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id) {
  AddError(PendingError(policy, std::string(), index, message_id));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement) {
  AddError(PendingError(policy, std::string(), -1, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id,
                              const std::string& replacement) {
  AddError(PendingError(policy, subkey, -1, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id,
                              const std::string& replacement) {
  AddError(PendingError(policy, std::string(), index, message_id, replacement));
}

string16 PolicyErrorMap::GetErrors(const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<string16> list;
  for (const_iterator it = range.first; it != range.second; ++it)
    list.push_back(it->second);
  return JoinString(list, '\n');
}

bool PolicyErrorMap::empty() {
  CheckReadyAndConvert();
  return map_.empty();
}

size_t PolicyErrorMap::size() {
  CheckReadyAndConvert();
  return map_.size();
}

PolicyErrorMap::const_iterator PolicyErrorMap::begin() {
  CheckReadyAndConvert();
  return map_.begin();
}

PolicyErrorMap::const_iterator PolicyErrorMap::end() {
  CheckReadyAndConvert();
  return map_.end();
}

void PolicyErrorMap::Clear() {
  CheckReadyAndConvert();
  map_.clear();
}

void PolicyErrorMap::AddError(const PendingError& error) {
  if (IsReady()) {
    Convert(error);
  } else {
    pending_.push_back(error);
  }
}

void PolicyErrorMap::Convert(const PendingError& error) {
  string16 submessage;
  if (error.has_replacement) {
    submessage = l10n_util::GetStringFUTF16(error.message_id,
                                            ASCIIToUTF16(error.replacement));
  } else {
    submessage = l10n_util::GetStringUTF16(error.message_id);
  }
  string16 message;
  if (!error.subkey.empty()) {
    message = l10n_util::GetStringFUTF16(IDS_POLICY_SUBKEY_ERROR,
                                         ASCIIToUTF16(error.subkey),
                                         submessage);
  } else if (error.index >= 0) {
    message = l10n_util::GetStringFUTF16(IDS_POLICY_LIST_ENTRY_ERROR,
                                         base::IntToString16(error.index),
                                         submessage);
  } else {
    message = submessage;
  }
  map_.insert(std::make_pair(error.policy, message));
}

void PolicyErrorMap::CheckReadyAndConvert() {
  DCHECK(IsReady());
  for (size_t i = 0; i < pending_.size(); ++i) {
    Convert(pending_[i]);
  }
  pending_.clear();
}

}  // namespace policy
