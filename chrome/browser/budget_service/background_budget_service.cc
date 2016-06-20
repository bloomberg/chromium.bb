// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/background_budget_service.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

constexpr char kSeparator = '#';

// Calculate the ratio of the different components of a budget with respect
// to a maximum time period of 10 days = 864000.0 seconds.
constexpr double kSecondsToAccumulate = 864000.0;

bool GetBudgetDataFromPrefs(Profile* profile,
                            const GURL& origin,
                            double* old_budget,
                            double* old_ses,
                            double* last_updated) {
  const base::DictionaryValue* map =
      profile->GetPrefs()->GetDictionary(prefs::kBackgroundBudgetMap);

  std::string map_string;
  map->GetStringWithoutPathExpansion(origin.spec(), &map_string);

  // There is no data for the preference, return false and let the caller
  // deal with that.
  if (map_string.empty())
    return false;

  std::vector<base::StringPiece> parts =
      base::SplitStringPiece(map_string, base::StringPiece(&kSeparator, 1),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if ((parts.size() != 3) ||
      (!base::StringToDouble(parts[0].as_string(), last_updated)) ||
      (!base::StringToDouble(parts[1].as_string(), old_budget)) ||
      (!base::StringToDouble(parts[2].as_string(), old_ses))) {
    // Somehow the data stored in the preferences has become corrupted, log an
    // error and remove the invalid data.
    LOG(ERROR) << "Preferences data for background budget service is "
               << "invalid for origin " << origin.possibly_invalid_spec();
    DictionaryPrefUpdate update(profile->GetPrefs(),
                                prefs::kBackgroundBudgetMap);
    base::DictionaryValue* update_map = update.Get();
    update_map->RemoveWithoutPathExpansion(origin.spec(), nullptr);
    return false;
  }

  return true;
}

// The value stored in prefs is a concatenated string of last updated time, old
// budget, and old ses.
void SetBudgetDataInPrefs(Profile* profile,
                          const GURL& origin,
                          double last_updated,
                          double budget,
                          double ses) {
  std::string s = base::StringPrintf("%f%c%f%c%f", last_updated, kSeparator,
                                     budget, kSeparator, ses);
  DictionaryPrefUpdate update(profile->GetPrefs(), prefs::kBackgroundBudgetMap);
  base::DictionaryValue* map = update.Get();

  map->SetStringWithoutPathExpansion(origin.spec(), s);
}

}  // namespace

BackgroundBudgetService::BackgroundBudgetService(Profile* profile)
    : clock_(base::WrapUnique(new base::DefaultClock)), profile_(profile) {
  DCHECK(profile);
}

BackgroundBudgetService::~BackgroundBudgetService() {}

// static
void BackgroundBudgetService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kBackgroundBudgetMap);
}

// static
double BackgroundBudgetService::GetCost(CostType type) {
  switch (type) {
    case CostType::SILENT_PUSH:
      return 2.0;
      // No default case.
  }
  NOTREACHED();
  return SiteEngagementScore::kMaxPoints + 1.0;
}

void BackgroundBudgetService::GetBudget(const GURL& origin,
                                        const GetBudgetCallback& callback) {
  DCHECK_EQ(origin, origin.GetOrigin());

  // Get the current SES score, which we'll use to set a new budget.
  SiteEngagementService* service = SiteEngagementService::Get(profile_);
  double ses_score = service->GetScore(origin);

  // Get the last used budget data. This is a triple of last calculated time,
  // budget at that time, and Site Engagement Score (ses) at that time.
  double old_budget = 0.0, old_ses = 0.0, last_updated_msec = 0.0;
  if (!GetBudgetDataFromPrefs(profile_, origin, &old_budget, &old_ses,
                              &last_updated_msec)) {
    // If there is no stored data or the data can't be parsed, just return the
    // SES.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, ses_score));
    return;
  }

  base::Time now = clock_->Now();
  base::TimeDelta elapsed = now - base::Time::FromDoubleT(last_updated_msec);

  // The user can set their clock backwards, so if the last updated time is in
  // the future, don't update the budget based on elapsed time. Eventually the
  // clock will reach the future, and the budget calculations will catch up.
  // TODO(harkness): Consider what to do if the clock jumps forward by a
  // significant amount.
  if (elapsed.InMicroseconds() < 0) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, old_budget));
    return;
  }

  // For each time period that elapses, calculate the carryover ratio as the
  // ratio of time remaining in our max period to the total period.
  // The carryover component is then the old budget multiplied by the ratio.
  double carryover_ratio = std::max(
      0.0,
      ((kSecondsToAccumulate - elapsed.InSeconds()) / kSecondsToAccumulate));
  double budget_carryover = old_budget * carryover_ratio;

  // The ses component is an average of the last ses score used for budget
  // calculation and the current ses score.
  // The ses average is them multiplied by the ratio of time elapsed to the
  // total period.
  double ses_ratio =
      std::min(1.0, (elapsed.InSeconds() / kSecondsToAccumulate));
  double ses_component = (old_ses + ses_score) / 2 * ses_ratio;

  // Budget recalculation consists of a budget carryover component, which
  // rewards sites that don't use all their budgets every day, and a ses
  // component, which gives extra budget to sites that have a high ses score.
  double budget = budget_carryover + ses_component;
  DCHECK_GE(budget, 0.0);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, budget));
}

void BackgroundBudgetService::StoreBudget(const GURL& origin,
                                          double budget,
                                          const base::Closure& closure) {
  DCHECK_EQ(origin, origin.GetOrigin());
  DCHECK_GE(budget, 0.0);
  DCHECK_LE(budget, SiteEngagementService::GetMaxPoints());

  // Get the current SES score to write into the prefs with the new budget.
  SiteEngagementService* service = SiteEngagementService::Get(profile_);
  double ses_score = service->GetScore(origin);

  base::Time time = clock_->Now();
  SetBudgetDataInPrefs(profile_, origin, time.ToDoubleT(), budget, ses_score);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(closure));
}

// Override the default clock with the specified clock. Only used for testing.
void BackgroundBudgetService::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}
