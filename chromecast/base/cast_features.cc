// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromecast {
namespace {

// A constant used to always activate a FieldTrial.
const base::FieldTrial::Probability k100PercentProbability = 100;

// The name of the default group to use for Cast DCS features.
const char kDefaultDCSFeaturesGroup[] = "default_dcs_features_group";

base::LazyInstance<std::unordered_set<int32_t>>::Leaky g_experiment_ids =
    LAZY_INSTANCE_INITIALIZER;
bool g_experiment_ids_initialized = false;

void SetExperimentIds(const base::ListValue& list) {
  DCHECK(!g_experiment_ids_initialized);
  std::unordered_set<int32_t> ids;
  for (size_t i = 0; i < list.GetSize(); ++i) {
    int32_t id;
    if (list.GetInteger(i, &id)) {
      ids.insert(id);
    } else {
      LOG(ERROR) << "Non-integer value found in experiment id list!";
    }
  }
  g_experiment_ids.Get().swap(ids);
  g_experiment_ids_initialized = true;
}
}  // namespace

// An iterator for a base::DictionaryValue. Use an alias for brevity in loops.
using Iterator = base::DictionaryValue::Iterator;

void InitializeFeatureList(const base::DictionaryValue& dcs_features,
                           const base::ListValue& dcs_experiment_ids,
                           const std::string& cmd_line_enable_features,
                           const std::string& cmd_line_disable_features) {
  DCHECK(!base::FeatureList::GetInstance());

  // Set the experiments.
  SetExperimentIds(dcs_experiment_ids);

  // Initialize the FeatureList from the command line.
  auto feature_list = base::MakeUnique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(cmd_line_enable_features,
                                          cmd_line_disable_features);

  // Override defaults from the DCS config.
  for (Iterator it(dcs_features); !it.IsAtEnd(); it.Advance()) {
    // Each feature must have its own FieldTrial object. Since experiments are
    // controlled server-side for Chromecast, and this class is designed with a
    // client-side experimentation framework in mind, these parameters are
    // carefully chosen:
    //   - The field trial name is unused for our purposes. However, we need to
    //     maintain a 1:1 mapping with Features in order to properly store and
    //     access parameters associated with each Feature. Therefore, use the
    //     Feature's name as the FieldTrial name to ensure uniqueness.
    //   - The probability is hard-coded to 100% so that the FeatureList always
    //     respects the value from DCS.
    //   - The default group is unused; it will be the same for every feature.
    //   - Expiration year, month, and day use a special value such that the
    //     feature will never expire.
    //   - SESSION_RANDOMIZED is used to prevent the need for an
    //     entropy_provider. However, this value doesn't matter.
    //   - We don't care about the group_id.
    //
    const std::string& feature_name = it.key();
    auto* field_trial = base::FieldTrialList::FactoryGetFieldTrial(
        feature_name, k100PercentProbability, kDefaultDCSFeaturesGroup,
        base::FieldTrialList::kNoExpirationYear, 1 /* month */, 1 /* day */,
        base::FieldTrial::SESSION_RANDOMIZED, nullptr);

    bool enabled;
    if (it.value().GetAsBoolean(&enabled)) {
      // A boolean entry simply either enables or disables a feature.
      feature_list->RegisterFieldTrialOverride(
          feature_name,
          enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
          field_trial);
      continue;
    }

    const base::DictionaryValue* params_dict;
    if (it.value().GetAsDictionary(&params_dict)) {
      // A dictionary entry implies that the feature is enabled.
      feature_list->RegisterFieldTrialOverride(
          feature_name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
          field_trial);

      // If the feature has not been overriden from the command line, set its
      // parameters accordingly.
      if (!feature_list->IsFeatureOverriddenFromCommandLine(
              feature_name, base::FeatureList::OVERRIDE_DISABLE_FEATURE)) {
        // Build a map of the FieldTrial parameters and associate it to the
        // FieldTrial.
        base::FieldTrialParamAssociator::FieldTrialParams params;
        for (Iterator p(*params_dict); !p.IsAtEnd(); p.Advance()) {
          std::string val;
          if (p.value().GetAsString(&val)) {
            params[p.key()] = val;
          } else {
            LOG(ERROR) << "Entry in params dict for \"" << feature_name << "\""
                       << " feature is not a string. Skipping.";
          }
        }

        // Register the params, so that they can be queried by client code.
        bool success = base::AssociateFieldTrialParams(
            feature_name, kDefaultDCSFeaturesGroup, params);
        DCHECK(success);
      }
      continue;
    }

    // Other base::Value types are not supported.
    LOG(ERROR) << "A DCS feature mapped to an unsupported value. key: "
               << feature_name << " type: " << it.value().type();
  }

  base::FeatureList::SetInstance(std::move(feature_list));
}

base::DictionaryValue GetOverriddenFeaturesForStorage(
    const base::DictionaryValue& features) {
  base::DictionaryValue persistent_dict;

  // |features| maps feature names to either a boolean or a dict of params.
  for (Iterator it(features); !it.IsAtEnd(); it.Advance()) {
    bool enabled;
    if (it.value().GetAsBoolean(&enabled)) {
      persistent_dict.SetBoolean(it.key(), enabled);
      continue;
    }

    const base::DictionaryValue* params_dict;
    if (it.value().GetAsDictionary(&params_dict)) {
      auto params = base::MakeUnique<base::DictionaryValue>();

      bool bval;
      int ival;
      double dval;
      std::string sval;
      for (Iterator p(*params_dict); !p.IsAtEnd(); p.Advance()) {
        const auto& param_key = p.key();
        const auto& param_val = p.value();
        if (param_val.GetAsBoolean(&bval)) {
          params->SetString(param_key, bval ? "true" : "false");
        } else if (param_val.GetAsInteger(&ival)) {
          params->SetString(param_key, base::IntToString(ival));
        } else if (param_val.GetAsDouble(&dval)) {
          params->SetString(param_key, base::DoubleToString(dval));
        } else if (param_val.GetAsString(&sval)) {
          params->SetString(param_key, sval);
        } else {
          LOG(ERROR) << "Entry in params dict for \"" << it.key() << "\""
                     << " is not of a supported type (key: " << p.key()
                     << ", type: " << param_val.type();
        }
      }
      persistent_dict.Set(it.key(), std::move(params));
      continue;
    }

    // Other base::Value types are not supported.
    LOG(ERROR) << "A DCS feature mapped to an unsupported value. key: "
               << it.key() << " type: " << it.value().type();
  }

  return persistent_dict;
}

const std::unordered_set<int32_t>& GetDCSExperimentIds() {
  DCHECK(g_experiment_ids_initialized);
  return g_experiment_ids.Get();
}

void ResetCastFeaturesForTesting() {
  g_experiment_ids_initialized = false;
  base::FeatureList::ClearInstanceForTesting();
}

}  // namespace chromecast
