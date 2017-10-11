// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

class PreviewsLoggerObserver;

// Records information about previews and interventions events. The class only
// keeps the recent event logs.
class PreviewsLogger {
 public:
  // Information needed for a log message. This information will be used to
  // display log messages on chrome://interventions-internals.
  struct MessageLog {
    MessageLog(const std::string& event_type,
               const std::string& event_description,
               const GURL& url,
               base::Time time);

    MessageLog(const MessageLog& other);

    // The type of event associated with the log.
    const std::string event_type;

    // Human readable description of the event.
    const std::string event_description;

    // The url associated with the log.
    const GURL url;

    // The time of when the event happened.
    const base::Time time;
  };

  // Information about a preview navigation.
  struct PreviewNavigation {
    PreviewNavigation(const GURL& url,
                      PreviewsType type,
                      bool opt_out,
                      base::Time time)
        : url(url), type(type), opt_out(opt_out), time(time) {}

    // The url associated with the log.
    const GURL url;

    const PreviewsType type;

    // Opt out status of the navigation.
    const bool opt_out;

    // Time stamp of when the navigation was determined to be an opt out non-opt
    // out.
    const base::Time time;
  };

  PreviewsLogger();
  virtual ~PreviewsLogger();

  // Add a observer to the list. This observer will be notified when new a log
  // message is added to the logger. Observers must remove themselves with
  // RemoveObserver.
  void AddAndNotifyObserver(PreviewsLoggerObserver* observer);

  // Removes a observer from the observers list. Virtualized in testing.
  virtual void RemoveObserver(PreviewsLoggerObserver* observer);

  std::vector<MessageLog> log_messages() const;

  // Add MessageLog using the given information. Pop out the oldest log if the
  // size of |log_messages_| grows larger than a threshold.
  void LogMessage(const std::string& event_type,
                  const std::string& event_description,
                  const GURL& url,
                  base::Time time);

  // Convert |navigation| to a MessageLog, and add that message to
  // |log_messages_|. Virtual for testing.
  virtual void LogPreviewNavigation(const PreviewNavigation& navigation);

 private:
  // Collection of recorded log messages.
  std::list<MessageLog> log_messages_;

  // A list of observers listening to the logger.
  base::ObserverList<PreviewsLoggerObserver> observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLogger);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_H_
