// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_
#pragma once

#include <vector>
#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_monitor {

struct TimeRange {
  TimeRange();
  TimeRange(base::Time start_time, base::Time end_time);
  ~TimeRange();

  base::Time start;
  base::Time end;
};

// The class supporting all performance monitor storage. This class wraps
// multiple leveldb::DB objects. All methods must be called from a background
// thread. Callers should use BrowserThread::PostBlockingPoolSequencedTask using
// performance_monitor::kDBSequenceToken as the sequence token.
//
// Different schemas are used for the different leveldb::DB's based off of the
// structure of the data and the common ways that it will need to be accessed.
// The following specifies the schema of each type of leveldb::DB. Delimiters
// are denoted with a '-'.
//
// State DB:
// Stores information about the configuration or 'state' of the browser. Things
// like browser version go in here.
// Key: Unique Identifier
// Value: State Value
//
// Active Interval DB:
// Stores information about when there is data in the database. When the
// database is constructed, the time is noted as the start of the active
// interval. Then, every write operation the current time is marked as the end
// of the current active interval. If the database has no write operations for
// a certian amount of time, then the database is considered inactive for that
// time period and a new start time is noted. Having the key be the beginning
// of the active interval allows for efficient upserts to the current active
// interval. If the end of the active interval was in the key, then every update
// to the active interval would have to remove a key and insert a new one.
// Key: Beginning of ActiveInterval
// Value: End of ActiveInterval
//
// Event DB:
// Stores all events. A time and type is enough to uniquely identify an event.
// Using the time that the event took place as the beginning of the key allows
// us to efficiently answer the question: "What are all the events that took
// place in this time range?".
// Key: Time - Type
// Value: Event in JSON
//
// Recent DB:
// Stores the most recent metric statistics to go into the database. This
// database becomes useful when it is necessary to find all the active metrics
// within a timerange. Without it, all the metric databases would need to be
// searched to see if that metric is active.
// Key: Time - Metric - Activity
// Value: Statistic
//
// Metric DB:
// Stores the statistics for different metrics. Having the activity before the
// time in the key esures that all statistics for a specific acticity are
// spatially local.
// Key: Metric - Activity - Time
// Value: Statistic
class Database : public base::RefCountedThreadSafe<Database> {
 public:
  // The class that the database will use to infer time. Abstracting out the
  // time mechanism allows for easy testing and mock data insetion.
  class Clock {
   public:
    Clock() {}
    virtual ~Clock() {}
    virtual base::Time GetTime() = 0;
  };

  static scoped_refptr<Database> Create(FilePath path);

  // A "state" value is anything that can only have one value at a time, and
  // usually describes the state of the browser eg. version.
  bool AddStateValue(const std::string& key, const std::string& value);

  std::string GetStateValue(const std::string& key);

  // Erase everything in the database. Warning - No undo!
  void Clear();

  // Returns the times for which there is data in the database.
  std::vector<TimeRange> GetActiveIntervals(const base::Time& start,
                                           const base::Time& end);

  FilePath path() const { return path_; }

  void set_clock(scoped_ptr<Clock> clock) {
    clock_ = clock.Pass();
  }

 private:
  friend class base::RefCountedThreadSafe<Database>;
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorDatabaseSetupTest, OpenCloseTest);
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorDatabaseSetupTest,
                           ActiveIntervalTest);

  // By default, the database uses a clock that simply returns the current time.
  class SystemClock : public Clock {
   public:
    SystemClock() {}
    virtual ~SystemClock() {}
    virtual base::Time GetTime() OVERRIDE;
  };

  explicit Database(const FilePath& path);
  virtual ~Database();

  bool Close();

  // Mark the database as being active for the current time.
  void UpdateActiveInterval();

  // The directory where all the databases will reside.
  FilePath path_;

  // The key for the beginning of the active interval.
  std::string start_time_key_;

  // The last time the database had a transaction.
  base::Time last_update_time_;

  scoped_ptr<Clock> clock_;

  scoped_ptr<leveldb::DB> recent_db_;

  scoped_ptr<leveldb::DB> state_db_;

  scoped_ptr<leveldb::DB> active_interval_db_;

  scoped_ptr<leveldb::DB> metric_db_;

  leveldb::ReadOptions read_options_;
  leveldb::WriteOptions write_options_;

  DISALLOW_COPY_AND_ASSIGN(Database);
};
}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_
