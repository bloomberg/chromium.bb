// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_merger.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"

namespace chromeos {
namespace onc {
namespace {

typedef scoped_ptr<base::DictionaryValue> DictionaryPtr;

// Inserts |true| at every field name in |result| that is recommended in policy.
void MarkRecommendedFieldnames(const base::DictionaryValue& policy,
                              base::DictionaryValue* result) {
  const ListValue* recommended_value = NULL;
  if (!policy.GetListWithoutPathExpansion(kRecommended, &recommended_value))
    return;
  for (ListValue::const_iterator it = recommended_value->begin();
       it != recommended_value->end(); ++it) {
    std::string entry;
    if ((*it)->GetAsString(&entry))
      result->SetBooleanWithoutPathExpansion(entry, true);
  }
}

// Split the given |policy| into its recommended and mandatory settings. The
// dictionary |result_editable| contains a |true| value at each path that is
// editable by the user.
void SplitPolicy(const base::DictionaryValue& policy,
                 DictionaryPtr* result_recommended,
                 DictionaryPtr* result_editable,
                 DictionaryPtr* result_mandatory) {
  result_recommended->reset(new base::DictionaryValue);
  result_editable->reset(new base::DictionaryValue);
  result_mandatory->reset(new base::DictionaryValue);

  MarkRecommendedFieldnames(policy, result_editable->get());

  // Distribute policy's entries into |result_recommended| and |result_editable|
  // according to |result_editable| and recurse into nested dictionaries.
  for (base::DictionaryValue::Iterator it(policy); it.HasNext(); it.Advance()) {
    if (it.key() == kRecommended)
      continue;

    const base::DictionaryValue* child_policy = NULL;
    if (it.value().GetAsDictionary(&child_policy)) {
      DictionaryPtr child_recommended, child_editable, child_mandatory;
      SplitPolicy(*child_policy, &child_recommended, &child_editable,
                  &child_mandatory);

      (*result_recommended)->SetWithoutPathExpansion(
          it.key(), child_recommended.release());
      (*result_editable)->SetWithoutPathExpansion(
          it.key(), child_editable.release());
      (*result_mandatory)->SetWithoutPathExpansion(
          it.key(), child_mandatory.release());
    } else {
      bool is_recommended = false;
      (*result_editable)->GetBooleanWithoutPathExpansion(it.key(),
                                                         &is_recommended);
      if (is_recommended) {
        (*result_recommended)->SetWithoutPathExpansion(
            it.key(), it.value().DeepCopy());
      } else {
        (*result_mandatory)->SetWithoutPathExpansion(
            it.key(), it.value().DeepCopy());
      }
    }
  }
}

// Copy values from |user| at paths that are assigned true in |editable|. The
// values are copied to a new dictionary which is returned.
DictionaryPtr DeepCopyIf(
    const base::DictionaryValue& user,
    const base::DictionaryValue& editable) {
  DictionaryPtr result(new base::DictionaryValue);

  for (base::DictionaryValue::Iterator it(user); it.HasNext(); it.Advance()) {
    const base::DictionaryValue* user_child = NULL;
    if (it.value().GetAsDictionary(&user_child)) {
      const base::DictionaryValue* editable_child = NULL;
      if (editable.GetDictionaryWithoutPathExpansion(it.key(),
                                                     &editable_child)) {
        result->SetWithoutPathExpansion(
            it.key(), DeepCopyIf(*user_child, *editable_child).release());
      }
    } else {
      bool editable_flag;
      if (editable.GetBooleanWithoutPathExpansion(it.key(), &editable_flag) &&
          editable_flag) {
        result->SetWithoutPathExpansion(it.key(), it.value().DeepCopy());
      }
    }
  }

  return result.Pass();
}

void MergeDictionaryIfNotNULL(const base::DictionaryValue* update,
                              base::DictionaryValue* result) {
  if (update != NULL)
    result->MergeDictionary(update);
}

}  // namespace

DictionaryPtr MergeSettingsWithPolicies(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_onc,
    const base::DictionaryValue* shared_onc) {
  DictionaryPtr user_mandatory;
  DictionaryPtr user_editable;
  DictionaryPtr user_recommended;
  if (user_policy != NULL) {
    SplitPolicy(*user_policy, &user_recommended, &user_editable,
                &user_mandatory);
  } else {
    user_mandatory.reset(new base::DictionaryValue);
    user_recommended.reset(new base::DictionaryValue);
    // 'user_editable == NULL' means everything is editable
  }

  DictionaryPtr device_mandatory;
  DictionaryPtr device_editable;
  DictionaryPtr device_recommended;
  if (device_policy != NULL) {
    SplitPolicy(*device_policy, &device_recommended, &device_editable,
                &device_mandatory);
  } else {
    device_mandatory.reset(new base::DictionaryValue);
    device_recommended.reset(new base::DictionaryValue);
    // 'device_editable == NULL' means everything is editable
  }

  DictionaryPtr reduced_user_onc(
      user_onc != NULL ? user_onc->DeepCopy() : new base::DictionaryValue);
  if (user_editable.get() != NULL)
    reduced_user_onc = DeepCopyIf(*reduced_user_onc, *user_editable);
  if (device_editable.get() != NULL)
    reduced_user_onc = DeepCopyIf(*reduced_user_onc, *device_editable);

  DictionaryPtr reduced_shared_onc(
      shared_onc != NULL ? shared_onc->DeepCopy() : new base::DictionaryValue);
  if (user_editable.get() != NULL)
    reduced_shared_onc = DeepCopyIf(*reduced_shared_onc, *user_editable);
  if (device_editable.get() != NULL)
    reduced_shared_onc = DeepCopyIf(*reduced_shared_onc, *device_editable);

  // Merge the settings layers according to their priority: From lowest to
  // highest priority.
  DictionaryPtr result(new base::DictionaryValue);
  MergeDictionaryIfNotNULL(device_recommended.get(), result.get());
  MergeDictionaryIfNotNULL(user_recommended.get(), result.get());
  MergeDictionaryIfNotNULL(reduced_shared_onc.get(), result.get());
  MergeDictionaryIfNotNULL(reduced_user_onc.get(), result.get());
  MergeDictionaryIfNotNULL(device_mandatory.get(), result.get());
  MergeDictionaryIfNotNULL(user_mandatory.get(), result.get());

  return result.Pass();
}

}  // namespace onc
}  // namespace chromeos
