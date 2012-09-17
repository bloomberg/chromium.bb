// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_

#include <vector>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_monitor {

struct TimeRange {
  TimeRange();
  TimeRange(base::Time start_time, base::Time end_time);
  ~TimeRange();

  base::Time start;
  base::Time end;
};

class KeyBuilder;

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
// a certain amount of time, then the database is considered inactive for that
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
// Stores the most recent metric statistics to go into the database. There is
// only ever one entry per (metric, activity) pair. |recent_map_| keeps an
// in-memory version of this database with a mapping from a concatenation of
// metric and activity to the key used in the recent db. |recent_map_| allows us
// to quickly find the key that must be replaced in the recent db. This
// database becomes useful when it is necessary to find all the active metrics
// within a timerange. Without it, all the metric databases would need to be
// searched to see if that metric is active.
// Key: Time - Metric - Activity
// Value: Statistic
//
// Max Value DB:
// Stores the max metric statistics that have been inserted into the database.
// There is only ever one entry per (metric, activity) pair. |max_value_map_|
// keeps an in-memory version of this database with a mapping from a
// concatenation of metric and activity to the max metric.
// Key: Metric - Activity
// Value: Statistic
//
// Metric DB:
// Stores the statistics for different metrics. Having the time before the
// activity ensures that the search space can only be as large as the time
// interval.
// Key: Metric - Time - Activity
// Value: Statistic
class Database {
 public:
  typedef std::set<EventType> EventTypeSet;
  typedef std::vector<linked_ptr<Event> > EventVector;
  typedef std::set<MetricType> MetricTypeSet;
  typedef std::vector<Metric> MetricVector;
  typedef std::map<std::string, linked_ptr<MetricVector> > MetricVectorMap;

  static const char kDatabaseSequenceToken[];

  // The class that the database will use to infer time. Abstracting out the
  // time mechanism allows for easy testing and mock data insetion.
  class Clock {
   public:
    Clock() {}
    virtual ~Clock() {}
    virtual base::Time GetTime() = 0;
  };

  virtual ~Database();

  static scoped_ptr<Database> Create(FilePath path);

  // A "state" value is anything that can only have one value at a time, and
  // usually describes the state of the browser eg. version.
  bool AddStateValue(const std::string& key, const std::string& value);

  std::string GetStateValue(const std::string& key);

  // Add an event to the database.
  bool AddEvent(const Event& event);

  // Retrieve the events from the database. These methods populate the provided
  // vector, and will search on the given criteria.
  EventVector GetEvents(EventType type,
                        const base::Time& start,
                        const base::Time& end);

  EventVector GetEvents(const base::Time& start, const base::Time& end) {
    return GetEvents(EVENT_UNDEFINED, start, end);
  }

  EventVector GetEvents(EventType type) {
    return GetEvents(type, base::Time(), clock_->GetTime());
  }

  EventVector GetEvents() {
    return GetEvents(EVENT_UNDEFINED, base::Time(), clock_->GetTime());
  }

  EventTypeSet GetEventTypes(const base::Time& start, const base::Time& end);

  EventTypeSet GetEventTypes() {
    return GetEventTypes(base::Time(), clock_->GetTime());
  }

  // Add a metric instance to the database.
  bool AddMetric(const std::string& activity,
                 MetricType metric_type,
                 const std::string& value);

  bool AddMetric(MetricType metric_type, const std::string& value) {
    return AddMetric(kProcessChromeAggregate, metric_type, value);
  }

  // Get the metrics that are active for the given process between |start|
  // (inclusive) and |end| (exclusive).
  MetricTypeSet GetActiveMetrics(const base::Time& start,
                                 const base::Time& end);

  // Get the activities that are active for the given metric after |start|.
  std::set<std::string> GetActiveActivities(MetricType metric_type,
                                            const base::Time& start);

  // Get the max value for the given metric in the db.
  double GetMaxStatsForActivityAndMetric(const std::string& activity,
                                         MetricType metric_type);
  double GetMaxStatsForActivityAndMetric(MetricType metric_type) {
    return GetMaxStatsForActivityAndMetric(kProcessChromeAggregate,
                                           metric_type);
  }

  // Populate info with the most recent activity. Return false if populate
  // was unsuccessful.
  bool GetRecentStatsForActivityAndMetric(const std::string& activity,
                                          MetricType metric_type,
                                          Metric* metric);

  bool GetRecentStatsForActivityAndMetric(MetricType metric_type,
                                           Metric* metric) {
    return GetRecentStatsForActivityAndMetric(kProcessChromeAggregate,
                                              metric_type,
                                              metric);
  }

  // Query given |metric_type| and |activity|.
  MetricVector GetStatsForActivityAndMetric(const std::string& activity,
                                            MetricType metric_type,
                                            const base::Time& start,
                                            const base::Time& end);

  MetricVector GetStatsForActivityAndMetric(MetricType metric_type,
                                            const base::Time& start,
                                            const base::Time& end) {
    return GetStatsForActivityAndMetric(kProcessChromeAggregate, metric_type,
                                        start, end);
  }

  MetricVector GetStatsForActivityAndMetric(const std::string& activity,
                                            MetricType metric_type) {
    return GetStatsForActivityAndMetric(activity, metric_type, base::Time(),
                                        clock_->GetTime());
  }

  MetricVector GetStatsForActivityAndMetric(MetricType metric_type) {
    return GetStatsForActivityAndMetric(kProcessChromeAggregate, metric_type,
                                        base::Time(), clock_->GetTime());
  }

  // Query given |metric_type|. The returned map is keyed by activity.
  MetricVectorMap GetStatsForMetricByActivity(MetricType metric_type,
                                              const base::Time& start,
                                              const base::Time& end);

  MetricVectorMap GetStatsForMetricByActivity(MetricType metric_type) {
    return GetStatsForMetricByActivity(
        metric_type, base::Time(), clock_->GetTime());
  }

  // Returns the active time intervals that overlap with the time interval
  // defined by |start| and |end|.
  std::vector<TimeRange> GetActiveIntervals(const base::Time& start,
                                            const base::Time& end);

  FilePath path() const { return path_; }

  void set_clock(scoped_ptr<Clock> clock) {
    clock_ = clock.Pass();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorDatabaseSetupTest, OpenClose);
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorDatabaseSetupTest, ActiveInterval);

  typedef std::map<std::string, std::string> RecentMap;
  typedef std::map<std::string, double> MaxValueMap;

  // By default, the database uses a clock that simply returns the current time.
  class SystemClock : public Clock {
   public:
    SystemClock() {}
    virtual ~SystemClock() {}
    virtual base::Time GetTime() OVERRIDE;
  };

  explicit Database(const FilePath& path);

  void InitDBs();

  bool Close();

  // Load recent info from the db into recent_map_.
  void LoadRecents();
  // Load max values from the db into the max_value_map_.
  void LoadMaxValues();

  // Mark the database as being active for the current time.
  void UpdateActiveInterval();
  // Updates the max_value_map_ and max_value_db_ if the value is greater than
  // the current max value for the given activity and metric.
  bool UpdateMaxValue(const std::string& activity,
                      MetricType metric,
                      const std::string& value);

  scoped_ptr<KeyBuilder> key_builder_;

  // A mapping of id,metric to the last inserted key for those parameters
  // is maintained to prevent having to search through the recent db every
  // insert.
  RecentMap recent_map_;

  MaxValueMap max_value_map_;

  // The directory where all the databases will reside.
  FilePath path_;

  // The key for the beginning of the active interval.
  std::string start_time_key_;

  // The last time the database had a transaction.
  base::Time last_update_time_;

  scoped_ptr<Clock> clock_;

  scoped_ptr<leveldb::DB> recent_db_;

  scoped_ptr<leveldb::DB> max_value_db_;

  scoped_ptr<leveldb::DB> state_db_;

  scoped_ptr<leveldb::DB> active_interval_db_;

  scoped_ptr<leveldb::DB> metric_db_;

  scoped_ptr<leveldb::DB> event_db_;

  leveldb::ReadOptions read_options_;
  leveldb::WriteOptions write_options_;

  DISALLOW_COPY_AND_ASSIGN(Database);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_DATABASE_H_
