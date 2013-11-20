// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <map>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/version.h"
#include "components/variations/processed_study.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

Study_Platform GetCurrentPlatform() {
#if defined(OS_WIN)
  return Study_Platform_PLATFORM_WINDOWS;
#elif defined(OS_IOS)
  return Study_Platform_PLATFORM_IOS;
#elif defined(OS_MACOSX)
  return Study_Platform_PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return Study_Platform_PLATFORM_CHROMEOS;
#elif defined(OS_ANDROID)
  return Study_Platform_PLATFORM_ANDROID;
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study_Platform_PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64 date_time) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(date_time);
}

// Associates the variations params of |experiment|, if present.
void RegisterExperimentParams(const Study& study,
                              const Study_Experiment& experiment) {
  std::map<std::string, std::string> params;
  for (int i = 0; i < experiment.param_size(); ++i) {
    if (experiment.param(i).has_name() && experiment.param(i).has_value())
      params[experiment.param(i).name()] = experiment.param(i).value();
  }
  if (!params.empty())
    AssociateVariationParams(study.name(), experiment.name(), params);
}

// If there are variation ids associated with |experiment|, register the
// variation ids.
void RegisterVariationIds(const Study_Experiment& experiment,
                          const std::string& trial_name) {
  if (experiment.has_google_web_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_web_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
  if (experiment.has_google_update_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_update_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_UPDATE_SERVICE,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
}

}  // namespace

VariationsSeedProcessor::VariationsSeedProcessor() {
}

VariationsSeedProcessor::~VariationsSeedProcessor() {
}

bool VariationsSeedProcessor::AllowVariationIdWithForcingFlag(
    const Study& study) {
  if (!study.has_filter())
    return false;
  const Study_Filter& filter = study.filter();
  if (filter.platform_size() == 0 || filter.channel_size() == 0)
    return false;
  for (int i = 0; i < filter.platform_size(); ++i) {
    if (filter.platform(i) != Study_Platform_PLATFORM_ANDROID &&
        filter.platform(i) != Study_Platform_PLATFORM_IOS) {
      return false;
    }
  }
  for (int i = 0; i < filter.channel_size(); ++i) {
    if (filter.channel(i) != Study_Channel_CANARY &&
        filter.channel(i) != Study_Channel_DEV) {
      return false;
    }
  }
  return true;
}

void VariationsSeedProcessor::CreateTrialsFromSeed(
    const VariationsSeed& seed,
    const std::string& locale,
    const base::Time& reference_date,
    const base::Version& version,
    Study_Channel channel,
    Study_FormFactor form_factor) {
  std::vector<ProcessedStudy> filtered_studies;
  FilterAndValidateStudies(seed, locale, reference_date, version, channel,
                           form_factor, &filtered_studies);

  for (size_t i = 0; i < filtered_studies.size(); ++i)
    CreateTrialFromStudy(filtered_studies[i]);
}

void VariationsSeedProcessor::FilterAndValidateStudies(
    const VariationsSeed& seed,
    const std::string& locale,
    const base::Time& reference_date,
    const base::Version& version,
    Study_Channel channel,
    Study_FormFactor form_factor,
    std::vector<ProcessedStudy>* filtered_studies) {
  DCHECK(version.IsValid());

  // Add expired studies (in a disabled state) only after all the non-expired
  // studies have been added (and do not add an expired study if a corresponding
  // non-expired study got added). This way, if there's both an expired and a
  // non-expired study that applies, the non-expired study takes priority.
  std::set<std::string> created_studies;
  std::vector<const Study*> expired_studies;

  for (int i = 0; i < seed.study_size(); ++i) {
    const Study& study = seed.study(i);
    if (!ShouldAddStudy(study, locale, reference_date,
                        version, channel, form_factor))
      continue;

    if (IsStudyExpired(study, reference_date)) {
      expired_studies.push_back(&study);
    } else if (!ContainsKey(created_studies, study.name())) {
      ProcessedStudy::ValidateAndAppendStudy(&study, false, filtered_studies);
      created_studies.insert(study.name());
    }
  }

  for (size_t i = 0; i < expired_studies.size(); ++i) {
    if (!ContainsKey(created_studies, expired_studies[i]->name())) {
      ProcessedStudy::ValidateAndAppendStudy(expired_studies[i], true,
                                             filtered_studies);
    }
  }
}

bool VariationsSeedProcessor::CheckStudyChannel(const Study_Filter& filter,
                                                Study_Channel channel) {
  // An empty channel list matches all channels.
  if (filter.channel_size() == 0)
    return true;

  for (int i = 0; i < filter.channel_size(); ++i) {
    if (filter.channel(i) == channel)
      return true;
  }
  return false;
}

bool VariationsSeedProcessor::CheckStudyFormFactor(
    const Study_Filter& filter,
    Study_FormFactor form_factor) {
  // An empty form factor list matches all form factors.
  if (filter.form_factor_size() == 0)
    return true;

  for (int i = 0; i < filter.form_factor_size(); ++i) {
    if (filter.form_factor(i) == form_factor)
      return true;
  }
  return false;
}

bool VariationsSeedProcessor::CheckStudyLocale(
    const Study_Filter& filter,
    const std::string& locale) {
  // An empty locale list matches all locales.
  if (filter.locale_size() == 0)
    return true;

  for (int i = 0; i < filter.locale_size(); ++i) {
    if (filter.locale(i) == locale)
      return true;
  }
  return false;
}

bool VariationsSeedProcessor::CheckStudyPlatform(
    const Study_Filter& filter,
    Study_Platform platform) {
  // An empty platform list matches all platforms.
  if (filter.platform_size() == 0)
    return true;

  for (int i = 0; i < filter.platform_size(); ++i) {
    if (filter.platform(i) == platform)
      return true;
  }
  return false;
}

bool VariationsSeedProcessor::CheckStudyStartDate(
    const Study_Filter& filter,
    const base::Time& date_time) {
  if (filter.has_start_date()) {
    const base::Time start_date =
        ConvertStudyDateToBaseTime(filter.start_date());
    return date_time >= start_date;
  }

  return true;
}

bool VariationsSeedProcessor::CheckStudyVersion(
    const Study_Filter& filter,
    const base::Version& version) {
  if (filter.has_min_version()) {
    if (version.CompareToWildcardString(filter.min_version()) < 0)
      return false;
  }

  if (filter.has_max_version()) {
    if (version.CompareToWildcardString(filter.max_version()) > 0)
      return false;
  }

  return true;
}

void VariationsSeedProcessor::CreateTrialFromStudy(
    const ProcessedStudy& processed_study) {
  const Study& study = *processed_study.study();

  // Check if any experiments need to be forced due to a command line
  // flag. Force the first experiment with an existing flag.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.has_forcing_flag() &&
        command_line->HasSwitch(experiment.forcing_flag())) {
      base::FieldTrialList::CreateFieldTrial(study.name(), experiment.name());
      RegisterExperimentParams(study, experiment);
      DVLOG(1) << "Trial " << study.name() << " forced by flag: "
               << experiment.forcing_flag();
      if (AllowVariationIdWithForcingFlag(study))
        RegisterVariationIds(experiment, study.name());
      return;
    }
  }

  uint32 randomization_seed = 0;
  base::FieldTrial::RandomizationType randomization_type =
      base::FieldTrial::SESSION_RANDOMIZED;
  if (study.has_consistency() &&
      study.consistency() == Study_Consistency_PERMANENT) {
    randomization_type = base::FieldTrial::ONE_TIME_RANDOMIZED;
    if (study.has_randomization_seed())
      randomization_seed = study.randomization_seed();
  }

  // The trial is created without specifying an expiration date because the
  // expiration check in field_trial.cc is based on the build date. Instead,
  // the expiration check using |reference_date| is done explicitly below.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrialWithRandomizationSeed(
          study.name(), processed_study.total_probability(),
          study.default_experiment_name(),
          base::FieldTrialList::kNoExpirationYear, 1, 1, randomization_type,
          randomization_seed, NULL));

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    RegisterExperimentParams(study, experiment);

    // Groups with forcing flags have probability 0 and will never be selected.
    // Therefore, there's no need to add them to the field trial.
    if (experiment.has_forcing_flag())
      continue;

    if (experiment.name() != study.default_experiment_name())
      trial->AppendGroup(experiment.name(), experiment.probability_weight());

    RegisterVariationIds(experiment, study.name());
  }

  trial->SetForced();
  if (processed_study.is_expired())
    trial->Disable();
  else if (study.activation_type() == Study_ActivationType_ACTIVATION_AUTO)
    trial->group();
}

bool VariationsSeedProcessor::IsStudyExpired(const Study& study,
                                       const base::Time& date_time) {
  if (study.has_expiry_date()) {
    const base::Time expiry_date =
        ConvertStudyDateToBaseTime(study.expiry_date());
    return date_time >= expiry_date;
  }

  return false;
}

bool VariationsSeedProcessor::ShouldAddStudy(
    const Study& study,
    const std::string& locale,
    const base::Time& reference_date,
    const base::Version& version,
    Study_Channel channel,
    Study_FormFactor form_factor) {
  if (study.has_filter()) {
    if (!CheckStudyChannel(study.filter(), channel)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to channel.";
      return false;
    }

    if (!CheckStudyFormFactor(study.filter(), form_factor)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to form factor.";
      return false;
    }

    if (!CheckStudyLocale(study.filter(), locale)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to locale.";
      return false;
    }

    if (!CheckStudyPlatform(study.filter(), GetCurrentPlatform())) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to platform.";
      return false;
    }

    if (!CheckStudyVersion(study.filter(), version)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to version.";
      return false;
    }

    if (!CheckStudyStartDate(study.filter(), reference_date)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to start date.";
      return false;
    }
  }

  DVLOG(1) << "Kept study " << study.name() << ".";
  return true;
}

}  // namespace chrome_variations
