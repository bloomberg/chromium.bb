// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/previews/core/previews_logger_observer.h"

namespace previews {

namespace {

const char kPreviewDecisionMade[] = "Decision";
const char kPreviewNavigationEventType[] = "Navigation";
const size_t kMaximumNavigationLogs = 10;
const size_t kMaximumDecisionLogs = 25;

std::string GetDescriptionForPreviewsNavigation(PreviewsType type,
                                                bool opt_out) {
  return base::StringPrintf("%s preview navigation - user opt_out: %s",
                            GetStringNameForType(type).c_str(),
                            opt_out ? "True" : "False");
}

std::string GetDescriptionForPreviewsDecision(PreviewsEligibilityReason reason,
                                              PreviewsType type) {
  std::string reason_str;
  switch (reason) {
    case PreviewsEligibilityReason::ALLOWED:
      reason_str = "Allowed";
    case PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE:
      reason_str = "Blacklist failed to be created";
      break;
    case PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED:
      reason_str = "Blacklist not loaded from disk yet";
      break;
    case PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT:
      reason_str = "User recently opted out";
      break;
    case PreviewsEligibilityReason::USER_BLACKLISTED:
      reason_str = "All previews are blacklisted";
      break;
    case PreviewsEligibilityReason::HOST_BLACKLISTED:
      reason_str = "All previews on this host are blacklisted";
      break;
    case PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE:
      reason_str = "Network quality unavailable";
      break;
    case PreviewsEligibilityReason::NETWORK_NOT_SLOW:
      reason_str = "Network not slow";
      break;
    case PreviewsEligibilityReason::RELOAD_DISALLOWED:
      reason_str = "Page reloads do not show previews for this preview type";
      break;
    case PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER:
      reason_str = "Host blacklisted by server rules";
      break;
  }
  return base::StringPrintf("%s preview decision made - %s",
                            GetStringNameForType(type).c_str(),
                            reason_str.c_str());
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

PreviewsLogger::PreviewsLogger() {}

PreviewsLogger::~PreviewsLogger() {}

void PreviewsLogger::AddAndNotifyObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);

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
}

void PreviewsLogger::RemoveObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
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

void PreviewsLogger::LogPreviewDecisionMade(PreviewsEligibilityReason reason,
                                            const GURL& url,
                                            base::Time time,
                                            PreviewsType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumDecisionLogs, decisions_logs_.size());

  std::string description = GetDescriptionForPreviewsDecision(reason, type);
  LogMessage(kPreviewDecisionMade, description, url, time);

  // Pop out the oldest message when the list is full.
  if (decisions_logs_.size() >= kMaximumDecisionLogs) {
    decisions_logs_.pop_front();
  }

  decisions_logs_.emplace_back(kPreviewDecisionMade, description, url, time);
}

}  // namespace previews
