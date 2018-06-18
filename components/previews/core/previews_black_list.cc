// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

namespace {

PreviewsEligibilityReason BlacklistReasonToPreviewsReason(
    BlacklistReason reason) {
  // TODO(ryansturm): Blacklist split -- In derived class.
  switch (reason) {
    case BlacklistReason::kBlacklistNotLoaded:
      return PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED;
    case BlacklistReason::kUserOptedOutInSession:
      return PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT;
    case BlacklistReason::kUserOptedOutInGeneral:
      return PreviewsEligibilityReason::USER_BLACKLISTED;
    case BlacklistReason::kUserOptedOutOfHost:
      return PreviewsEligibilityReason::HOST_BLACKLISTED;
    case BlacklistReason::kUserOptedOutOfType:
      NOTREACHED() << "Previews does not support type-base blacklisting";
      return PreviewsEligibilityReason::ALLOWED;
    case BlacklistReason::kAllowed:
      return PreviewsEligibilityReason::ALLOWED;
  }
}

}

PreviewsBlackList::PreviewsBlackList(
    std::unique_ptr<PreviewsOptOutStore> opt_out_store,
    base::Clock* clock,
    PreviewsBlacklistDelegate* blacklist_delegate,
    BlacklistData::AllowedTypesAndVersions allowed_types)
    : loaded_(false),
      opt_out_store_(std::move(opt_out_store)),
      clock_(clock),
      blacklist_delegate_(blacklist_delegate),
      allowed_types_(std::move(allowed_types)),
      weak_factory_(this) {
  // TODO(ryansturm): Blacklist split -- In derived class.
  DCHECK(blacklist_delegate_);
  Init();
}

void PreviewsBlackList::Init() {
  // TODO(ryansturm): Blacklist split -- In base class.

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  std::unique_ptr<BlacklistData::Policy> session_policy;
  if (ShouldUseSessionPolicy(&duration, &history, &threshold)) {
    session_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlacklistData::Policy> persistent_policy;
  if (ShouldUsePersistentPolicy(&duration, &history, &threshold)) {
    persistent_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  size_t max_hosts = 0;
  std::unique_ptr<BlacklistData::Policy> host_policy;
  if (ShouldUseHostPolicy(&duration, &history, &threshold, &max_hosts)) {
    host_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlacklistData::Policy> type_policy;
  if (ShouldUseTypePolicy(&duration, &history, &threshold)) {
    type_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  auto blacklist_data = std::make_unique<BlacklistData>(
      std::move(session_policy), std::move(persistent_policy),
      std::move(host_policy), std::move(type_policy), max_hosts,
      GetAllowedTypes());

  if (opt_out_store_) {
    opt_out_store_->LoadBlackList(
        std::move(blacklist_data),
        base::BindOnce(&PreviewsBlackList::LoadBlackListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(std::move(blacklist_data));
  }
}

bool PreviewsBlackList::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                               size_t* history,
                                               int* threshold) const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  *duration = params::SingleOptOutDuration();
  *history = 1;
  *threshold = 1;
  return true;
}
bool PreviewsBlackList::ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                                  size_t* history,
                                                  int* threshold) const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  *history = params::MaxStoredHistoryLengthForHostIndifferentBlackList();
  *threshold = params::HostIndifferentBlackListOptOutThreshold();
  *duration = params::HostIndifferentBlackListPerHostDuration();
  return true;
}
bool PreviewsBlackList::ShouldUseHostPolicy(base::TimeDelta* duration,
                                            size_t* history,
                                            int* threshold,
                                            size_t* max_hosts) const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  *max_hosts = params::MaxInMemoryHostsInBlackList();
  *history = params::MaxStoredHistoryLengthForPerHostBlackList();
  *threshold = params::PerHostBlackListOptOutThreshold();
  *duration = params::PerHostBlackListDuration();
  return true;
}
bool PreviewsBlackList::ShouldUseTypePolicy(base::TimeDelta* duration,
                                            size_t* history,
                                            int* threshold) const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  return false;
}

const BlacklistData::AllowedTypesAndVersions&
PreviewsBlackList::GetAllowedTypes() const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  return allowed_types_;
}

PreviewsBlackList::~PreviewsBlackList() {}

base::Time PreviewsBlackList::AddPreviewNavigation(const GURL& url,
                                                   bool opt_out,
                                                   PreviewsType type) {
  // TODO(ryansturm): Blacklist split -- In derived class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());

  base::BooleanHistogram::FactoryGet(
      base::StringPrintf("Previews.OptOut.UserOptedOut.%s",
                         GetStringNameForType(type).c_str()),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(opt_out);

  return AddEntry(url.host(), opt_out, static_cast<int>(type));
}

base::Time PreviewsBlackList::AddEntry(const std::string& host_name,
                                       bool opt_out,
                                       int type) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Time now = clock_->Now();

  // If the |blacklist_data| has been loaded from |opt_out_store_|,  synchronous
  // operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    AddEntrySync(host_name, opt_out, type, now);
  } else {
    QueuePendingTask(base::BindOnce(&PreviewsBlackList::AddEntrySync,
                                    base::Unretained(this), host_name, opt_out,
                                    type, now));
  }

  return now;
}

void PreviewsBlackList::AddEntrySync(const std::string& host_name,
                                     bool opt_out,
                                     int type,
                                     base::Time time) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);

  bool host_was_blacklisted =
      blacklist_data_->IsHostBlacklisted(host_name, time);
  bool user_was_blacklisted = blacklist_data_->IsUserOptedOutInGeneral(time);
  blacklist_data_->AddEntry(host_name, opt_out, type, time, false);

  if (!host_was_blacklisted &&
      blacklist_data_->IsHostBlacklisted(host_name, time)) {
    // Notify |blacklist_delegate_| about a new blacklisted host.
    blacklist_delegate_->OnNewBlacklistedHost(host_name, time);
  }

  if (user_was_blacklisted != blacklist_data_->IsUserOptedOutInGeneral(time)) {
    // Notify |blacklist_delegate_| about a new blacklisted host.
    blacklist_delegate_->OnUserBlacklistedStatusChange(
        blacklist_data_->IsUserOptedOutInGeneral(time));
  }

  if (!opt_out_store_)
    return;
  opt_out_store_->AddEntry(opt_out, host_name, type, time);
}

PreviewsEligibilityReason PreviewsBlackList::IsLoadedAndAllowed(
    const GURL& url,
    PreviewsType type,
    bool ignore_long_term_black_list_rules,
    std::vector<PreviewsEligibilityReason>* passed_reasons) const {
  // TODO(ryansturm): Blacklist split -- In derived class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());

  std::vector<BlacklistReason> passed_blacklist_reasons;
  BlacklistReason reason = IsLoadedAndAllowed(
      url.host(), static_cast<int>(type), ignore_long_term_black_list_rules,
      &passed_blacklist_reasons);
  for (auto passed_reason : passed_blacklist_reasons) {
    passed_reasons->push_back(BlacklistReasonToPreviewsReason(passed_reason));
  }

  return BlacklistReasonToPreviewsReason(reason);
}

BlacklistReason PreviewsBlackList::IsLoadedAndAllowed(
    const std::string& host_name,
    int type,
    bool ignore_long_term_black_list_rules,
    std::vector<BlacklistReason>* passed_reasons) const {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!loaded_)
    return BlacklistReason::kBlacklistNotLoaded;
  passed_reasons->push_back(BlacklistReason::kBlacklistNotLoaded);

  return blacklist_data_->IsAllowed(host_name, type,
                                    ignore_long_term_black_list_rules,
                                    clock_->Now(), passed_reasons);
}

void PreviewsBlackList::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(begin_time, end_time);
  // If the |blacklist_data| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    ClearBlackListSync(begin_time, end_time);
  } else {
    QueuePendingTask(base::BindOnce(&PreviewsBlackList::ClearBlackListSync,
                                    base::Unretained(this), begin_time,
                                    end_time));
  }
}

void PreviewsBlackList::ClearBlackListSync(base::Time begin_time,
                                           base::Time end_time) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  DCHECK_LE(begin_time, end_time);

  // Clear the in-memory rules entirely.
  blacklist_data_->ClearData();
  loaded_ = false;

  // Notify |blacklist_delegate_| that the blacklist is cleared.
  blacklist_delegate_->OnBlacklistCleared(clock_->Now());

  // Delete relevant entries and reload the blacklist into memory.
  if (opt_out_store_) {
    opt_out_store_->ClearBlackList(begin_time, end_time);
    opt_out_store_->LoadBlackList(
        std::move(blacklist_data_),
        base::BindOnce(&PreviewsBlackList::LoadBlackListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(std::move(blacklist_data_));
  }
}

void PreviewsBlackList::QueuePendingTask(base::OnceClosure callback) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!loaded_);
  DCHECK(!callback.is_null());
  pending_callbacks_.push(std::move(callback));
}

void PreviewsBlackList::LoadBlackListDone(
    std::unique_ptr<BlacklistData> blacklist_data) {
  // TODO(ryansturm): Blacklist split -- In base class.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(blacklist_data);
  loaded_ = true;
  blacklist_data_ = std::move(blacklist_data);

  // Notify |blacklist_delegate_| on current user blacklisted status.
  blacklist_delegate_->OnUserBlacklistedStatusChange(
      blacklist_data_->IsUserOptedOutInGeneral(clock_->Now()));

  // Notify the |blacklist_delegate_| on historical blacklisted hosts.
  for (const auto& entry : blacklist_data_->black_list_item_host_map()) {
    if (blacklist_data_->IsHostBlacklisted(entry.first, clock_->Now())) {
      blacklist_delegate_->OnNewBlacklistedHost(
          entry.first, entry.second.most_recent_opt_out_time().value());
    }
  }

  // Run all pending tasks. |loaded_| may change if ClearBlackList is queued.
  while (pending_callbacks_.size() > 0 && loaded_) {
    std::move(pending_callbacks_.front()).Run();
    pending_callbacks_.pop();
  }
}

}  // namespace previews
