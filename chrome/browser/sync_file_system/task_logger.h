// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_TASK_LOGGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_TASK_LOGGER_H_

#include <deque>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace sync_file_system {

class TaskLogger : public base::SupportsWeakPtr<TaskLogger> {
 public:
  struct TaskLog {
    base::TimeDelta duration;
    std::string type;
    std::string summary;
    std::vector<std::string> details;

    TaskLog();
    ~TaskLog();
  };

  class Observer {
   public:
    virtual void OnLogRecorded(const TaskLog& task_log) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  TaskLogger();
  ~TaskLogger();

  void RecordLog(scoped_ptr<TaskLog> log);
  void ClearLog();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::deque<TaskLog*>& GetLog() const;

 private:
  std::deque<TaskLog*> log_history_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(TaskLogger);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_TASK_LOGGER_H_
