// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_merger.h"

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"

namespace chromeos {
namespace onc {
namespace {

typedef scoped_ptr<base::DictionaryValue> DictionaryPtr;

// Inserts |true| at every field name in |result| that is recommended in
// |policy|.
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

// Returns a dictionary which contains |true| at each path that is editable by
// the user. No other fields are set.
DictionaryPtr GetEditableFlags(const base::DictionaryValue& policy) {
  DictionaryPtr result_editable(new base::DictionaryValue);
  MarkRecommendedFieldnames(policy, result_editable.get());

  // Recurse into nested dictionaries.
  for (base::DictionaryValue::Iterator it(policy); it.HasNext(); it.Advance()) {
    const base::DictionaryValue* child_policy = NULL;
    if (it.key() == kRecommended ||
        !it.value().GetAsDictionary(&child_policy)) {
      continue;
    }

    result_editable->SetWithoutPathExpansion(
        it.key(), GetEditableFlags(*child_policy).release());
  }
  return result_editable.Pass();
}

// This is the base class for merging a list of DictionaryValues in
// parallel. See MergeDictionaries function.
class MergeListOfDictionaries {
 public:
  typedef std::vector<const base::DictionaryValue*> DictPtrs;

  MergeListOfDictionaries() {
  }

  virtual ~MergeListOfDictionaries() {
  }

  // For each path in any of the dictionaries |dicts|, the function
  // MergeListOfValues is called with the list of values that are located at
  // that path in each of the dictionaries. This function returns a new
  // dictionary containing all results of MergeListOfValues at the respective
  // paths. The resulting dictionary doesn't contain empty dictionaries.
  DictionaryPtr MergeDictionaries(const DictPtrs &dicts) {
    DictionaryPtr result(new base::DictionaryValue);
    std::set<std::string> visited;
    for (DictPtrs::const_iterator it_outer = dicts.begin();
         it_outer != dicts.end(); ++it_outer) {
      if (!*it_outer)
        continue;

      for (base::DictionaryValue::Iterator field(**it_outer); !field.IsAtEnd();
           field.Advance()) {
        const std::string& key = field.key();
        if (key == kRecommended || !visited.insert(key).second)
          continue;

        scoped_ptr<base::Value> merged_value;
        if (field.value().IsType(base::Value::TYPE_DICTIONARY)) {
          DictPtrs nested_dicts;
          for (DictPtrs::const_iterator it_inner = dicts.begin();
               it_inner != dicts.end(); ++it_inner) {
            const base::DictionaryValue* nested_dict = NULL;
            if (*it_inner)
              (*it_inner)->GetDictionaryWithoutPathExpansion(key, &nested_dict);
            nested_dicts.push_back(nested_dict);
          }
          DictionaryPtr merged_dict(MergeDictionaries(nested_dicts));
          if (!merged_dict->empty())
            merged_value = merged_dict.Pass();
        } else {
          std::vector<const base::Value*> values;
          for (DictPtrs::const_iterator it_inner = dicts.begin();
               it_inner != dicts.end(); ++it_inner) {
            const base::Value* value = NULL;
            if (*it_inner)
              (*it_inner)->GetWithoutPathExpansion(key, &value);
            values.push_back(value);
          }
          merged_value = MergeListOfValues(values);
        }

        if (merged_value)
          result->SetWithoutPathExpansion(key, merged_value.release());
      }
    }
    return result.Pass();
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. The order of the
  // values is the same as of the given dictionaries |dicts|. If a dictionary
  // doesn't contain a path then it's value is NULL.
  virtual scoped_ptr<base::Value> MergeListOfValues(
      const std::vector<const base::Value*>& values) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MergeListOfDictionaries);
};

// This is the base class for merging policies and user settings.
class MergeSettingsAndPolicies : public MergeListOfDictionaries {
 public:
  struct ValueParams {
    const base::Value* user_policy;
    const base::Value* device_policy;
    const base::Value* user_settings;
    const base::Value* shared_settings;
    bool user_editable;
    bool device_editable;
  };

  // Merge the provided dictionaries. For each path in any of the dictionaries,
  // MergeValues is called. Its results are collected in a new dictionary which
  // is then returned. The resulting dictionary never contains empty
  // dictionaries.
  DictionaryPtr MergeDictionaries(
      const base::DictionaryValue* user_policy,
      const base::DictionaryValue* device_policy,
      const base::DictionaryValue* user_settings,
      const base::DictionaryValue* shared_settings) {
    hasUserPolicy_ = (user_policy != NULL);
    hasDevicePolicy_ = (device_policy != NULL);

    DictionaryPtr user_editable;
    if (user_policy != NULL)
      user_editable = GetEditableFlags(*user_policy);

    DictionaryPtr device_editable;
    if (device_policy != NULL)
      device_editable = GetEditableFlags(*device_policy);

    std::vector<const base::DictionaryValue*> dicts(kLastIndex, NULL);
    dicts[kUserPolicyIndex] = user_policy;
    dicts[kDevicePolicyIndex] = device_policy;
    dicts[kUserSettingsIndex] = user_settings;
    dicts[kSharedSettingsIndex] = shared_settings;
    dicts[kUserEditableIndex] = user_editable.get();
    dicts[kDeviceEditableIndex] = device_editable.get();
    return MergeListOfDictionaries::MergeDictionaries(dicts);
  }

 protected:
  // This function is called by MergeDictionaries for each list of values that
  // are located at the same path in each of the dictionaries. Implementations
  // can use the Has*Policy functions.
  virtual scoped_ptr<base::Value> MergeValues(ValueParams values) = 0;

  // Whether a user policy was provided.
  bool HasUserPolicy() {
    return hasUserPolicy_;
  }

  // Whether a device policy was provided.
  bool HasDevicePolicy() {
    return hasDevicePolicy_;
  }

  // MergeListOfDictionaries override.
  virtual scoped_ptr<base::Value> MergeListOfValues(
      const std::vector<const base::Value*>& values) OVERRIDE {
    bool user_editable = !HasUserPolicy();
    if (values[kUserEditableIndex])
      values[kUserEditableIndex]->GetAsBoolean(&user_editable);

    bool device_editable = !HasDevicePolicy();
    if (values[kDeviceEditableIndex])
      values[kDeviceEditableIndex]->GetAsBoolean(&device_editable);

    ValueParams params;
    params.user_policy = values[kUserPolicyIndex];
    params.device_policy = values[kDevicePolicyIndex];
    params.user_settings = values[kUserSettingsIndex];
    params.shared_settings = values[kSharedSettingsIndex];
    params.user_editable = user_editable;
    params.device_editable = device_editable;
    return MergeValues(params);
  }

 private:
  enum {
    kUserPolicyIndex,
    kDevicePolicyIndex,
    kUserSettingsIndex,
    kSharedSettingsIndex,
    kUserEditableIndex,
    kDeviceEditableIndex,
    kLastIndex
  };

  bool hasUserPolicy_, hasDevicePolicy_;
};

// Call MergeDictionaries to merge policies and settings to the effective
// values. See the description of MergeSettingsAndPoliciesToEffective.
class MergeToEffective : public MergeSettingsAndPolicies {
 protected:
  // Merges |values| to the effective value (Mandatory policy overwrites user
  // settings overwrites shared settings overwrites recommended policy). |which|
  // is set to the respective onc::kAugmentation* constant that indicates which
  // source of settings is effective. Note that this function may return a NULL
  // pointer and set |which| to kAugmentationUserPolicy, which means that the
  // user policy didn't set a value but also didn't recommend it, thus enforcing
  // the empty value.
  scoped_ptr<base::Value> MergeValues(ValueParams values, std::string* which) {
    const base::Value* result = NULL;
    which->clear();
    if (!values.user_editable) {
      result = values.user_policy;
      *which = kAugmentationUserPolicy;
    } else if (!values.device_editable) {
      result = values.device_policy;
      *which = kAugmentationDevicePolicy;
    } else if (values.user_settings) {
      result = values.user_settings;
      *which = kAugmentationUserSetting;
    } else if (values.shared_settings) {
      result = values.shared_settings;
      *which = kAugmentationSharedSetting;
    } else if (values.user_policy) {
      result = values.user_policy;
      *which = kAugmentationUserPolicy;
    } else if (values.device_policy) {
      result = values.device_policy;
      *which = kAugmentationDevicePolicy;
    } else {
      // Can be reached if the current field is recommended, but none of the
      // dictionaries contained a value for it.
    }
    if (result)
      return make_scoped_ptr(result->DeepCopy());
    return scoped_ptr<base::Value>();
  }

  // MergeSettingsAndPolicies override.
  virtual scoped_ptr<base::Value> MergeValues(ValueParams values) {
    std::string which;
    return MergeValues(values, &which);
  }
};

// Call MergeDictionaries to merge policies and settings to an augmented
// dictionary which contains a dictionary for each value in the original
// dictionaries. See the description of MergeSettingsAndPoliciesToAugmented.
class MergeToAugmented : public MergeToEffective {
 protected:
  // MergeSettingsAndPolicies override.
  virtual scoped_ptr<base::Value> MergeValues(ValueParams values) {
    scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
    std::string which_effective;
    MergeToEffective::MergeValues(values, &which_effective).reset();
    if (!which_effective.empty()) {
      result->SetStringWithoutPathExpansion(kAugmentationEffectiveSetting,
                                            which_effective);
    }
    if (values.user_policy) {
      result->SetWithoutPathExpansion(kAugmentationUserPolicy,
                                      values.user_policy->DeepCopy());
    }
    if (values.device_policy) {
      result->SetWithoutPathExpansion(kAugmentationDevicePolicy,
                                      values.device_policy->DeepCopy());
    }
    if (values.user_settings) {
      result->SetWithoutPathExpansion(kAugmentationUserSetting,
                                      values.user_settings->DeepCopy());
    }
    if (values.shared_settings) {
      result->SetWithoutPathExpansion(kAugmentationSharedSetting,
                                      values.shared_settings->DeepCopy());
    }
    if (HasUserPolicy() && values.user_editable) {
      result->SetBooleanWithoutPathExpansion(kAugmentationUserEditable,
                                             values.user_editable);
    }
    if (HasDevicePolicy() && values.device_editable) {
      result->SetBooleanWithoutPathExpansion(kAugmentationDeviceEditable,
                                             values.device_editable);
    }
    return result.PassAs<base::Value>();
  }
};

}  // namespace

DictionaryPtr MergeSettingsAndPoliciesToEffective(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings) {
  MergeToEffective merger;
  return merger.MergeDictionaries(
      user_policy, device_policy, user_settings, shared_settings);
}

DictionaryPtr MergeSettingsAndPoliciesToAugmented(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* shared_settings) {
  MergeToAugmented merger;
  return merger.MergeDictionaries(
      user_policy, device_policy, user_settings, shared_settings);
}

}  // namespace onc
}  // namespace chromeos
