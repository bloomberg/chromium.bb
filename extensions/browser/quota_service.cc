// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/quota_service.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/error_utils.h"

namespace {

// If the browser stays open long enough, we reset state once a day.
// Whatever this value is, it should be an order of magnitude longer than
// the longest interval in any of the QuotaLimitHeuristics in use.
const int kPurgeIntervalInDays = 1;

const char kOverQuotaError[] = "This request exceeds the * quota.";

}  // namespace

namespace extensions {

QuotaService::QuotaService() {
  if (base::MessageLoop::current() != NULL) {  // Null in unit tests.
    purge_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromDays(kPurgeIntervalInDays),
                       this,
                       &QuotaService::Purge);
  }
}

QuotaService::~QuotaService() {
  DCHECK(CalledOnValidThread());
  purge_timer_.Stop();
  Purge();
}

std::string QuotaService::Assess(const std::string& extension_id,
                                 ExtensionFunction* function,
                                 const base::ListValue* args,
                                 const base::TimeTicks& event_time) {
  DCHECK(CalledOnValidThread());

  if (function->ShouldSkipQuotaLimiting())
    return std::string();

  // Lookup function list for extension.
  FunctionHeuristicsMap& functions = function_heuristics_[extension_id];

  // Lookup heuristics for function, create if necessary.
  QuotaLimitHeuristics& heuristics = functions[function->name()];
  if (heuristics.empty())
    function->GetQuotaLimitHeuristics(&heuristics);

  if (heuristics.empty())
    return std::string();  // No heuristic implies no limit.

  ViolationErrorMap::iterator violation_error =
      violation_errors_.find(extension_id);
  if (violation_error != violation_errors_.end())
    return violation_error->second;  // Repeat offender.

  QuotaLimitHeuristic* failed_heuristic = NULL;
  for (QuotaLimitHeuristics::iterator heuristic = heuristics.begin();
       heuristic != heuristics.end();
       ++heuristic) {
    // Apply heuristic to each item (bucket).
    if (!(*heuristic)->ApplyToArgs(args, event_time)) {
      failed_heuristic = *heuristic;
      break;
    }
  }

  if (!failed_heuristic)
    return std::string();

  std::string error = failed_heuristic->GetError();
  DCHECK_GT(error.length(), 0u);

  PurgeFunctionHeuristicsMap(&functions);
  function_heuristics_.erase(extension_id);
  violation_errors_[extension_id] = error;
  return error;
}

void QuotaService::PurgeFunctionHeuristicsMap(FunctionHeuristicsMap* map) {
  FunctionHeuristicsMap::iterator heuristics = map->begin();
  while (heuristics != map->end()) {
    STLDeleteElements(&heuristics->second);
    map->erase(heuristics++);
  }
}

void QuotaService::Purge() {
  DCHECK(CalledOnValidThread());
  std::map<std::string, FunctionHeuristicsMap>::iterator it =
      function_heuristics_.begin();
  for (; it != function_heuristics_.end(); function_heuristics_.erase(it++))
    PurgeFunctionHeuristicsMap(&it->second);
}

void QuotaLimitHeuristic::Bucket::Reset(const Config& config,
                                        const base::TimeTicks& start) {
  num_tokens_ = config.refill_token_count;
  expiration_ = start + config.refill_interval;
}

void QuotaLimitHeuristic::SingletonBucketMapper::GetBucketsForArgs(
    const base::ListValue* args,
    BucketList* buckets) {
  buckets->push_back(&bucket_);
}

QuotaLimitHeuristic::QuotaLimitHeuristic(const Config& config,
                                         BucketMapper* map,
                                         const std::string& name)
    : config_(config), bucket_mapper_(map), name_(name) {}

QuotaLimitHeuristic::~QuotaLimitHeuristic() {}

bool QuotaLimitHeuristic::ApplyToArgs(const base::ListValue* args,
                                      const base::TimeTicks& event_time) {
  BucketList buckets;
  bucket_mapper_->GetBucketsForArgs(args, &buckets);
  for (BucketList::iterator i = buckets.begin(); i != buckets.end(); ++i) {
    if ((*i)->expiration().is_null())  // A brand new bucket.
      (*i)->Reset(config_, event_time);
    if (!Apply(*i, event_time))
      return false;  // It only takes one to spoil it for everyone.
  }
  return true;
}

std::string QuotaLimitHeuristic::GetError() const {
  return extensions::ErrorUtils::FormatErrorMessage(kOverQuotaError, name_);
}

QuotaService::SustainedLimit::SustainedLimit(const base::TimeDelta& sustain,
                                             const Config& config,
                                             BucketMapper* map,
                                             const std::string& name)
    : QuotaLimitHeuristic(config, map, name),
      repeat_exhaustion_allowance_(sustain.InSeconds() /
                                   config.refill_interval.InSeconds()),
      num_available_repeat_exhaustions_(repeat_exhaustion_allowance_) {}

bool QuotaService::TimedLimit::Apply(Bucket* bucket,
                                     const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration())
    bucket->Reset(config(), event_time);

  return bucket->DeductToken();
}

bool QuotaService::SustainedLimit::Apply(Bucket* bucket,
                                         const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration()) {
    // We reset state for this item and start over again if this request breaks
    // the bad cycle that was previously being tracked.  This occurs if the
    // state in the bucket expired recently (it has been long enough since the
    // event that we don't care about the last event), but the bucket still has
    // tokens (so pressure was not sustained over that time), OR we are more
    // than 1 full refill interval away from the last event (so even if we used
    // up all the tokens in the last bucket, nothing happened in the entire
    // next refill interval, so it doesn't matter).
    if (bucket->has_tokens() ||
        event_time > bucket->expiration() + config().refill_interval) {
      bucket->Reset(config(), event_time);
      num_available_repeat_exhaustions_ = repeat_exhaustion_allowance_;
    } else if (--num_available_repeat_exhaustions_ > 0) {
      // The last interval was saturated with requests, and this is the first
      // event in the next interval. If this happens
      // repeat_exhaustion_allowance_ times, it's a violation. Reset the bucket
      // state to start timing from the end of the last interval (and we'll
      // deduct the token below) so we can detect this each time it happens.
      bucket->Reset(config(), bucket->expiration());
    } else {
      // No allowances left; this request is a violation.
      return false;
    }
  }

  // We can go negative since we check has_tokens when we get to *next* bucket,
  // and for the small interval all that matters is whether we used up all the
  // tokens (which is true if num_tokens_ <= 0).
  bucket->DeductToken();
  return true;
}

}  // namespace extensions
