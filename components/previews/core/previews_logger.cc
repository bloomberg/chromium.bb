// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/previews/core/previews_logger_observer.h"

namespace previews {

std::string GetDescriptionForInfoBarDescription(previews::PreviewsType type) {
  return base::StringPrintf("%s InfoBar shown",
                            previews::GetStringNameForType(type).c_str());
}

namespace {

static const char kPreviewDecisionMadeEventType[] = "Decision";
static const char kPreviewNavigationEventType[] = "Navigation";
const size_t kMaximumNavigationLogs = 10;
const size_t kMaximumDecisionLogs = 25;

std::string GetDescriptionForPreviewsNavigation(PreviewsType type,
                                                bool opt_out) {
  return base::StringPrintf("%s preview - user opt-out: %s",
                            GetStringNameForType(type).c_str(),
                            opt_out ? "True" : "False");
}

std::string GetReasonDescription(PreviewsEligibilityReason reason,
                                 bool final_reason) {
  switch (reason) {
    case PreviewsEligibilityReason::ALLOWED:
      DCHECK(final_reason);
      return "Allowed";
    case PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE:
      return final_reason ? "Blacklist failed to be created"
                          : "Blacklist not null";
    case PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED:
      return final_reason ? "Blacklist not loaded from disk yet"
                          : "Blacklist loaded from disk";
    case PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT:
      return final_reason ? "User recently opted out"
                          : "User did not opt out recently";
    case PreviewsEligibilityReason::USER_BLACKLISTED:
      return final_reason ? "All previews are blacklisted"
                          : "Not all previews are blacklisted";
    case PreviewsEligibilityReason::HOST_BLACKLISTED:
      return final_reason ? "All previews on this host are blacklisted"
                          : "Host is not blacklisted on all previews";
    case PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE:
      return final_reason ? "Network quality unavailable"
                          : "Network quality available";
    case PreviewsEligibilityReason::NETWORK_NOT_SLOW:
      return final_reason ? "Network not slow" : "Network is slow";
    case PreviewsEligibilityReason::RELOAD_DISALLOWED:
      return final_reason
                 ? "Page reloads do not show previews for this preview type"
                 : "Page reloads allowed";
    case PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER:
      return final_reason ? "Host blacklisted by server rules"
                          : "Host not blacklisted by server rules";
    case PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER:
      return final_reason ? "Host not whitelisted by server rules"
                          : "Host whitelisted by server rules";
    case PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS:
      return final_reason ? "Allowed (but without server rule check)"
                          : "Not allowed (without server rule check)";
  }
  NOTREACHED();
  return "";
}

std::string GetDescriptionForPreviewsDecision(PreviewsEligibilityReason reason,
                                              PreviewsType type,
                                              bool final_reason) {
  return base::StringPrintf("%s preview - %s",
                            GetStringNameForType(type).c_str(),
                            GetReasonDescription(reason, final_reason).c_str());
}

}  // namespace

PreviewsLogger::MessageLog::MessageLog(const std::string& event_type,
                                       const std::string& event_description,
                                       const GURL& url,
                                       base::Time time)
    : event_type(event_type),
      event_description(event_description),
      url(url),
      time(time) {}

PreviewsLogger::MessageLog::MessageLog(const MessageLog& other)
    : event_type(other.event_type),
      event_description(other.event_description),
      url(other.url),
      time(other.time) {}

PreviewsLogger::PreviewsLogger() : blacklist_ignored_(false) {}

PreviewsLogger::~PreviewsLogger() {}

void PreviewsLogger::AddAndNotifyObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
  // Notify the status of blacklist decisions ingored.
  observer->OnIgnoreBlacklistDecisionStatusChanged(blacklist_ignored_);

  // Merge navigation logs and decision logs in chronological order, and push
  // them to |observer|.
  auto navigation_ptr = navigations_logs_.begin();
  auto decision_ptr = decisions_logs_.begin();
  while (navigation_ptr != navigations_logs_.end() ||
         decision_ptr != decisions_logs_.end()) {
    if (navigation_ptr == navigations_logs_.end()) {
      observer->OnNewMessageLogAdded(*decision_ptr);
      ++decision_ptr;
      continue;
    }
    if (decision_ptr == decisions_logs_.end()) {
      observer->OnNewMessageLogAdded(*navigation_ptr);
      ++navigation_ptr;
      continue;
    }
    if (navigation_ptr->time < decision_ptr->time) {
      observer->OnNewMessageLogAdded(*navigation_ptr);
      ++navigation_ptr;
    } else {
      observer->OnNewMessageLogAdded(*decision_ptr);
      ++decision_ptr;
    }
  }

  // Push the current state of blacklist (user blacklisted state and all
  // blacklisted hosts).
  observer->OnUserBlacklistedStatusChange(user_blacklisted_status_);
  for (auto entry : blacklisted_hosts_) {
    observer->OnNewBlacklistedHost(entry.first, entry.second);
  }
}

void PreviewsLogger::RemoveObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
  if (observer_list_.begin() == observer_list_.end()) {
    // |observer_list_| is empty.
    observer->OnLastObserverRemove();
  }
}

void PreviewsLogger::LogMessage(const std::string& event_type,
                                const std::string& event_description,
                                const GURL& url,
                                base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Notify observers about the new MessageLog.
  for (auto& observer : observer_list_) {
    observer.OnNewMessageLogAdded(
        MessageLog(event_type, event_description, url, time));
  }
}

void PreviewsLogger::LogPreviewNavigation(const GURL& url,
                                          PreviewsType type,
                                          bool opt_out,
                                          base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumNavigationLogs, navigations_logs_.size());

  std::string description = GetDescriptionForPreviewsNavigation(type, opt_out);
  LogMessage(kPreviewNavigationEventType, description, url, time);

  // Pop out the oldest message when the list is full.
  if (navigations_logs_.size() >= kMaximumNavigationLogs) {
    navigations_logs_.pop_front();
  }

  navigations_logs_.emplace_back(kPreviewNavigationEventType, description, url,
                                 time);
}

void PreviewsLogger::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumDecisionLogs, decisions_logs_.size());

  // Logs all passed decisions messages.
  for (auto decision : passed_reasons) {
    std::string decision_description = GetDescriptionForPreviewsDecision(
        decision, type, false /* final_reason */);
    LogMessage(kPreviewDecisionMadeEventType, decision_description, url, time);
    decisions_logs_.emplace_back(kPreviewDecisionMadeEventType,
                                 decision_description, url, time);
  }

  std::string description =
      GetDescriptionForPreviewsDecision(reason, type, true /* final_reason */);
  LogMessage(kPreviewDecisionMadeEventType, description, url, time);

  // Pop out the older messages when the list is full.
  while (decisions_logs_.size() >= kMaximumDecisionLogs) {
    decisions_logs_.pop_front();
  }

  decisions_logs_.emplace_back(kPreviewDecisionMadeEventType, description, url,
                               time);
}

void PreviewsLogger::OnNewBlacklistedHost(const std::string& host,
                                          base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklisted_hosts_[host] = time;
  for (auto& observer : observer_list_) {
    observer.OnNewBlacklistedHost(host, time);
  }
}

void PreviewsLogger::OnUserBlacklistedStatusChange(bool blacklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnUserBlacklistedStatusChange(blacklisted);
  }
  user_blacklisted_status_ = blacklisted;
}

void PreviewsLogger::OnBlacklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnBlacklistCleared(time);
  }
  blacklisted_hosts_.clear();
}

void PreviewsLogger::OnIgnoreBlacklistDecisionStatusChanged(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blacklist_ignored_ = ignored;
  for (auto& observer : observer_list_) {
    observer.OnIgnoreBlacklistDecisionStatusChanged(ignored);
  }
}

}  // namespace previews
