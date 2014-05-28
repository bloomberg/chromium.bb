// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

class PolicyErrorMap::PendingError {
 public:
  explicit PendingError(const std::string& policy_name)
      : policy_name_(policy_name) {}
  virtual ~PendingError() {}

  const std::string& policy_name() const { return policy_name_; }

  virtual base::string16 GetMessage() const = 0;

 private:
  std::string policy_name_;

  DISALLOW_COPY_AND_ASSIGN(PendingError);
};

namespace {

class SimplePendingError : public PolicyErrorMap::PendingError {
 public:
  SimplePendingError(const std::string& policy_name,
                     int message_id,
                     const std::string& replacement)
      : PendingError(policy_name),
        message_id_(message_id),
        replacement_(replacement) {}
  virtual ~SimplePendingError() {}

  virtual base::string16 GetMessage() const OVERRIDE {
    if (message_id_ >= 0) {
      if (replacement_.empty())
        return l10n_util::GetStringUTF16(message_id_);
      return l10n_util::GetStringFUTF16(message_id_,
                                        base::ASCIIToUTF16(replacement_));
    }
    return base::ASCIIToUTF16(replacement_);
  }

 private:
  int message_id_;
  std::string replacement_;

  DISALLOW_COPY_AND_ASSIGN(SimplePendingError);
};

class DictSubkeyPendingError : public SimplePendingError {
 public:
  DictSubkeyPendingError(const std::string& policy_name,
                         const std::string& subkey,
                         int message_id,
                         const std::string& replacement)
      : SimplePendingError(policy_name, message_id, replacement),
        subkey_(subkey) {}
  virtual ~DictSubkeyPendingError() {}

  virtual base::string16 GetMessage() const OVERRIDE {
    return l10n_util::GetStringFUTF16(IDS_POLICY_SUBKEY_ERROR,
                                      base::ASCIIToUTF16(subkey_),
                                      SimplePendingError::GetMessage());
  }

 private:
  std::string subkey_;

  DISALLOW_COPY_AND_ASSIGN(DictSubkeyPendingError);
};

class ListItemPendingError : public SimplePendingError {
 public:
  ListItemPendingError(const std::string& policy_name,
                       int index,
                       int message_id,
                       const std::string& replacement)
      : SimplePendingError(policy_name, message_id, replacement),
        index_(index) {}
  virtual ~ListItemPendingError() {}

  virtual base::string16 GetMessage() const OVERRIDE {
    return l10n_util::GetStringFUTF16(IDS_POLICY_LIST_ENTRY_ERROR,
                                      base::IntToString16(index_),
                                      SimplePendingError::GetMessage());
  }

 private:
  int index_;

  DISALLOW_COPY_AND_ASSIGN(ListItemPendingError);
};

class SchemaValidatingPendingError : public SimplePendingError {
 public:
  SchemaValidatingPendingError(const std::string& policy_name,
                               const std::string& error_path,
                               const std::string& replacement)
      : SimplePendingError(policy_name, -1, replacement),
        error_path_(error_path) {};
  virtual ~SchemaValidatingPendingError() {}

  virtual base::string16 GetMessage() const OVERRIDE {
    return l10n_util::GetStringFUTF16(IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                                      base::ASCIIToUTF16(error_path_),
                                      SimplePendingError::GetMessage());
  }

 private:
  std::string error_path_;

  DISALLOW_COPY_AND_ASSIGN(SchemaValidatingPendingError);
};

}  // namespace

PolicyErrorMap::PolicyErrorMap() {
}

PolicyErrorMap::~PolicyErrorMap() {
}

bool PolicyErrorMap::IsReady() const {
  return ui::ResourceBundle::HasSharedInstance();
}

void PolicyErrorMap::AddError(const std::string& policy, int message_id) {
  AddError(new SimplePendingError(policy, message_id, std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id) {
  AddError(
      new DictSubkeyPendingError(policy, subkey, message_id, std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id) {
  AddError(new ListItemPendingError(policy, index, message_id, std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement) {
  AddError(new SimplePendingError(policy, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id,
                              const std::string& replacement) {
  AddError(new DictSubkeyPendingError(policy, subkey, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id,
                              const std::string& replacement) {
  AddError(new ListItemPendingError(policy, index, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& error_path,
                              const std::string& message) {
  AddError(new SchemaValidatingPendingError(policy, error_path, message));
}

base::string16 PolicyErrorMap::GetErrors(const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<base::string16> list;
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

void PolicyErrorMap::AddError(PendingError* error) {
  if (IsReady()) {
    Convert(error);
    delete error;
  } else {
    pending_.push_back(error);
  }
}

void PolicyErrorMap::Convert(PendingError* error) {
  map_.insert(std::make_pair(error->policy_name(), error->GetMessage()));
}

void PolicyErrorMap::CheckReadyAndConvert() {
  DCHECK(IsReady());
  for (size_t i = 0; i < pending_.size(); ++i) {
    Convert(pending_[i]);
  }
  pending_.clear();
}

}  // namespace policy
