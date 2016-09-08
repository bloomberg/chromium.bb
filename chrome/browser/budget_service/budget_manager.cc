// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_manager.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom.h"

using content::BrowserThread;

namespace {

// Previously, budget information was stored in the prefs. If there is any old
// information still there, clear it.
// TODO(harkness): Remove once Chrome 56 has branched.
void ClearBudgetDataFromPrefs(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kBackgroundBudgetMap);
}

}  // namespace

BudgetManager::BudgetManager(Profile* profile)
    : profile_(profile),
      db_(profile,
          profile->GetPath().Append(FILE_PATH_LITERAL("BudgetDatabase")),
          BrowserThread::GetBlockingPool()
              ->GetSequencedTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::GetSequenceToken(),
                  base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)),
      weak_ptr_factory_(this) {
  ClearBudgetDataFromPrefs(profile);
}

BudgetManager::~BudgetManager() {}

// static
void BudgetManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kBackgroundBudgetMap);
}

// static
double BudgetManager::GetCost(blink::mojom::BudgetOperationType type) {
  switch (type) {
    case blink::mojom::BudgetOperationType::SILENT_PUSH:
      return 2.0;
    case blink::mojom::BudgetOperationType::INVALID_OPERATION:
      return SiteEngagementScore::kMaxPoints + 1;
      // No default case.
  }
  NOTREACHED();
  return SiteEngagementScore::kMaxPoints + 1.0;
}

void BudgetManager::GetBudget(const GURL& origin,
                              const GetBudgetCallback& callback) {
  db_.GetBudgetDetails(origin, callback);
}

void BudgetManager::Reserve(const GURL& origin,
                            blink::mojom::BudgetOperationType type,
                            const ReserveCallback& callback) {
  DCHECK_EQ(origin, origin.GetOrigin());

  db_.SpendBudget(
      origin, GetCost(type),
      base::Bind(&BudgetManager::DidReserve, weak_ptr_factory_.GetWeakPtr(),
                 origin, type, callback));
}

void BudgetManager::Consume(const GURL& origin,
                            blink::mojom::BudgetOperationType type,
                            const ConsumeCallback& callback) {
  DCHECK_EQ(origin, origin.GetOrigin());
  bool found_reservation = false;

  // First, see if there is a reservation already.
  auto count = reservation_map_.find(origin.spec());
  if (count != reservation_map_.end()) {
    if (count->second == 1)
      reservation_map_.erase(origin.spec());
    else
      reservation_map_[origin.spec()]--;
    found_reservation = true;
  }

  if (found_reservation) {
    callback.Run(true);
    return;
  }

  // If there wasn't a reservation already, try to directly consume budget.
  // The callback will return directly to the caller.
  db_.SpendBudget(origin, GetCost(type), callback);
}

void BudgetManager::DidReserve(const GURL& origin,
                               blink::mojom::BudgetOperationType type,
                               const ReserveCallback& callback,
                               bool success) {
  if (!success) {
    callback.Run(false);
    return;
  }

  // Write the new reservation into the map.
  reservation_map_[origin.spec()]++;
  callback.Run(true);
}
