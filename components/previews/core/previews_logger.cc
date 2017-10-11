// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/previews/core/previews_logger_observer.h"

namespace previews {

namespace {

const char kPreviewNavigationEventType[] = "Navigation";
const size_t kMaximumMessageLogs = 10;

std::string GetPreviewNavigationDescription(
    const PreviewsLogger::PreviewNavigation& navigation) {
  return base::StringPrintf("%s preview navigation - user opt_out: %s",
                            GetStringNameForType(navigation.type).c_str(),
                            navigation.opt_out ? "True" : "False");
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
  for (auto message : log_messages_) {
    observer->OnNewMessageLogAdded(message);
  }
}

void PreviewsLogger::RemoveObserver(PreviewsLoggerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

std::vector<PreviewsLogger::MessageLog> PreviewsLogger::log_messages() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::vector<MessageLog>(log_messages_.begin(), log_messages_.end());
}

void PreviewsLogger::LogMessage(const std::string& event_type,
                                const std::string& event_description,
                                const GURL& url,
                                base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaximumMessageLogs, log_messages_.size());
  MessageLog message(event_type, event_description, url, time);

  // Notify observers about the new MessageLog.
  for (auto& observer : observer_list_) {
    observer.OnNewMessageLogAdded(message);
  }

  if (log_messages_.size() >= kMaximumMessageLogs) {
    log_messages_.pop_front();
  }
  log_messages_.push_back(message);
}

void PreviewsLogger::LogPreviewNavigation(const PreviewNavigation& navigation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogMessage(kPreviewNavigationEventType,
             GetPreviewNavigationDescription(navigation), navigation.url,
             navigation.time);
}

}  // namespace previews
