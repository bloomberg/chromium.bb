// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/database.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_monitor {
namespace {
const char kDbDir[] = "Performance Monitor Databases";
const char kRecentDb[] = "Recent Metrics";
const char kEventDb[] = "Events";
const char kStateDb[] = "Configuration";
const char kActiveIntervalDb[] = "Active Interval";
const char kMetricDb[] = "Metrics";
const char kDelimiter = '!';

struct RecentKey {
  RecentKey(const std::string& recent_time,
            MetricType recent_type,
            const std::string& recent_activity)
      : time(recent_time),
        type(recent_type),
        activity(recent_activity) {}
  ~RecentKey() {}

  const std::string time;
  const MetricType type;
  const std::string activity;
};

struct MetricKey {
  MetricKey(const std::string& metric_time,
            MetricType metric_type,
            const std::string& metric_activity)
      : time(metric_time),
        type(metric_type),
        activity(metric_activity) {}
  ~MetricKey() {}

  const std::string time;
  const MetricType type;
  const std::string activity;
};

// The position of different elements in the key for the event db.
enum EventKeyPosition {
  EVENT_TIME,  // The time the event was generated.
  EVENT_TYPE  // The type of event.
};

// The position of different elements in the key for the recent db.
enum RecentKeyPosition {
  RECENT_TIME,  // The time the stat was gathered.
  RECENT_TYPE,  // The unique identifier for the type of metric gathered.
  RECENT_ACTIVITY  // The unique identifier for the activity.
};

// The position of different elements in the key for a metric db.
enum MetricKeyPosition {
  METRIC_TYPE,  // The unique identifier for the metric.
  METRIC_TIME,  // The time the stat was gathered.
  METRIC_ACTIVITY  // The unique identifier for the activity.
};

// If the db is quiet for this number of minutes, then it is considered down.
const base::TimeDelta kActiveIntervalTimeout = base::TimeDelta::FromMinutes(5);

// Create the key used for internal active interval monitoring.
// Key Schema: <Time>
std::string CreateActiveIntervalKey(const base::Time& time) {
  return StringPrintf("%016" PRId64, time.ToInternalValue());
}

// Create the database key for a particular event.
// Key Schema: <Time>-<Event Type>
// Where '-' represents the delimiter.
std::string CreateEventKey(const base::Time& time, EventType event_type) {
  return StringPrintf("%016" PRId64 "%c%04d", time.ToInternalValue(),
                      kDelimiter, static_cast<int>(event_type));
}

// Create the database key for a certain metric to go in the "Recent" database.
// Key Schema: <Time>-<Metric>-<Activity>
std::string CreateRecentKey(const base::Time& time,
                            MetricType metric_type,
                            const std::string& activity) {
  return StringPrintf("%016" PRId64 "%c%04d%c%s", time.ToInternalValue(),
                      kDelimiter, metric_type, kDelimiter, activity.c_str());
}

EventType EventKeyToEventType(const std::string& event_key) {
  std::vector<std::string> split;
  base::SplitString(event_key, kDelimiter, &split);
  int event_type = 0;
  base::StringToInt(split[EVENT_TYPE], &event_type);
  return static_cast<EventType>(event_type);
}

MetricType StringToMetricType(const std::string& metric_type) {
  int metric_type_int = 0;
  base::StringToInt(metric_type, &metric_type_int);
  return static_cast<MetricType>(metric_type_int);
}

RecentKey SplitRecentKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  return RecentKey(split[RECENT_TIME], StringToMetricType(split[RECENT_TYPE]),
                   split[RECENT_ACTIVITY]);
}

// Create the database key for a statistic of a certain metric.
// Key Schema: <Metric>-<Time>-<Activity>
std::string CreateMetricKey(const base::Time& time,
                            MetricType metric_type,
                            const std::string& activity) {
  return StringPrintf("%04d%c%016" PRId64 "%c%s", metric_type, kDelimiter,
                      time.ToInternalValue(), kDelimiter, activity.c_str());
}

MetricKey SplitMetricKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  return MetricKey(split[METRIC_TIME], StringToMetricType(split[METRIC_TYPE]),
                   split[METRIC_ACTIVITY]);
}

// Create the key for recent_map_.
// Key Schema: <Activity>-<Metric>
std::string CreateRecentMapKey(MetricType metric_type,
                               const std::string& activity) {
  return StringPrintf("%s%c%04d", activity.c_str(), kDelimiter, metric_type);
}

TimeRange ActiveIntervalToTimeRange(const std::string& start_time,
                                    const std::string& end_time) {
  int64 start_time_int = 0;
  int64 end_time_int = 0;
  base::StringToInt64(start_time, &start_time_int);
  base::StringToInt64(end_time, &end_time_int);
  return TimeRange(base::Time::FromInternalValue(start_time_int),
                   base::Time::FromInternalValue(end_time_int));
}

}  // namespace

const char Database::kDatabaseSequenceToken[] =
    "_performance_monitor_db_sequence_token_";

TimeRange::TimeRange() {
}

TimeRange::TimeRange(base::Time start_time, base::Time end_time)
    : start(start_time),
      end(end_time) {
}

TimeRange::~TimeRange() {
}

base::Time Database::SystemClock::GetTime() {
  return base::Time::Now();
}

// Static
scoped_ptr<Database> Database::Create(FilePath path) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (path.empty()) {
    CHECK(PathService::Get(chrome::DIR_USER_DATA, &path));
    path = path.AppendASCII(kDbDir);
  }
  if (!file_util::DirectoryExists(path) && !file_util::CreateDirectory(path))
    return scoped_ptr<Database>();
  return scoped_ptr<Database>(new Database(path));
}

bool Database::AddStateValue(const std::string& key, const std::string& value) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  leveldb::Status insert_status = state_db_->Put(write_options_, key, value);
  return insert_status.ok();
}

std::string Database::GetStateValue(const std::string& key) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string result;
  state_db_->Get(read_options_, key, &result);
  return result;
}

bool Database::AddEvent(const Event& event) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  std::string value;
  base::JSONWriter::Write(event.data(), &value);
  std::string key = CreateEventKey(event.time(), event.type());
  leveldb::Status status = event_db_->Put(write_options_, key, value);
  return status.ok();
}

std::vector<TimeRange> Database::GetActiveIntervals(const base::Time& start,
                                                    const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::vector<TimeRange> results;
  std::string start_key = CreateActiveIntervalKey(start);
  std::string end_key = CreateActiveIntervalKey(end);
  scoped_ptr<leveldb::Iterator> it(active_interval_db_->NewIterator(
        read_options_));
  it->Seek(start_key);
  // If the interator is valid, we check the previous value in case we jumped
  // into the middle of an active interval. If the iterator is not valid, then
  // the key may be in the current active interval.
  if (it->Valid())
    it->Prev();
  else
    it->SeekToLast();
  if (it->Valid() && it->value().ToString() > start_key) {
    results.push_back(ActiveIntervalToTimeRange(it->key().ToString(),
                                                it->value().ToString()));
  }

  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() < end_key;
       it->Next()) {
    results.push_back(ActiveIntervalToTimeRange(it->key().ToString(),
                                                it->value().ToString()));
  }
  return results;
}

Database::EventList Database::GetEvents(EventType type, const base::Time& start,
                                        const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventList events;
  std::string start_key = CreateEventKey(start, EVENT_UNDEFINED);
  std::string end_key = CreateEventKey(end, EVENT_NUMBER_OF_EVENTS);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    if (type != EVENT_UNDEFINED) {
      EventType key_type = EventKeyToEventType(it->key().ToString());
      if (key_type != type)
        continue;
    }
    base::DictionaryValue* dict = NULL;
    if (!base::JSONReader::Read(
        it->value().ToString())->GetAsDictionary(&dict)) {
      LOG(ERROR) << "Unable to convert database event to JSON.";
      continue;
    }
    scoped_ptr<Event> event =
        Event::FromValue(scoped_ptr<base::DictionaryValue>(dict));
    if (!event.get())
      continue;
    events.push_back(linked_ptr<Event>(event.release()));
  }
  return events;
}

Database::EventTypeSet Database::GetEventTypes(const base::Time& start,
                                               const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventTypeSet results;
  std::string start_key = CreateEventKey(start, EVENT_UNDEFINED);
  std::string end_key = CreateEventKey(end, EVENT_NUMBER_OF_EVENTS);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    EventType key_type = EventKeyToEventType(it->key().ToString());
    results.insert(key_type);
  }
  return results;
}

bool Database::AddMetric(const std::string& activity, MetricType metric,
                         const std::string& value) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  base::Time timestamp = clock_->GetTime();
  std::string recent_key = CreateRecentKey(timestamp, metric, activity);
  std::string metric_key = CreateMetricKey(timestamp, metric, activity);
  std::string recent_map_key = CreateRecentMapKey(metric, activity);
  // Use recent_map_ to quickly find the key that must be removed.
  RecentMap::iterator old_it = recent_map_.find(recent_map_key);
  if (old_it != recent_map_.end())
    recent_db_->Delete(write_options_, old_it->second);
  recent_map_[recent_map_key] = recent_key;
  leveldb::Status recent_status =
      recent_db_->Put(write_options_, recent_key, value);
  leveldb::Status metric_status =
      metric_db_->Put(write_options_, metric_key, value);
  return recent_status.ok() && metric_status.ok();
}

std::vector<const MetricDetails*> Database::GetActiveMetrics(
    const base::Time& start, const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::vector<const MetricDetails*> results;
  std::string recent_start_key = CreateRecentKey(
      start, static_cast<MetricType>(0), std::string());
  std::string recent_end_key = CreateRecentKey(
      end, METRIC_NUMBER_OF_METRICS, std::string());
  std::string recent_end_of_time_key = CreateRecentKey(
      clock_->GetTime(), METRIC_NUMBER_OF_METRICS, std::string());

  std::set<MetricType> active_metrics;
  // Get all the guaranteed metrics.
  scoped_ptr<leveldb::Iterator> recent_it(
      recent_db_->NewIterator(read_options_));
  for (recent_it->Seek(recent_start_key);
       recent_it->Valid() && recent_it->key().ToString() <= recent_end_key;
       recent_it->Next()) {
    RecentKey split_key = SplitRecentKey(recent_it->key().ToString());
    active_metrics.insert(split_key.type);
  }
  // Get all the possible metrics (metrics that may have been updated after
  // |end|).
  std::set<MetricType> possible_metrics;
  for (recent_it->Seek(recent_end_key);
       recent_it->Valid() &&
       recent_it->key().ToString() <= recent_end_of_time_key;
       recent_it->Next()) {
    RecentKey split_key = SplitRecentKey(recent_it->key().ToString());
    possible_metrics.insert(split_key.type);
  }
  std::set<MetricType>::iterator possible_it;
  scoped_ptr<leveldb::Iterator> metric_it(
      metric_db_->NewIterator(read_options_));
  for (possible_it = possible_metrics.begin();
       possible_it != possible_metrics.end();
       ++possible_it) {
    std::string metric_start_key = CreateMetricKey(start, *possible_it,
                                                   std::string());
    std::string metric_end_key = CreateMetricKey(end, *possible_it,
                                                 std::string());
    metric_it->Seek(metric_start_key);
    // Stats in the timerange from any activity makes the metric active.
    if (metric_it->Valid() && metric_it->key().ToString() <= metric_end_key) {
      active_metrics.insert(*possible_it);
    }
  }
  std::set<MetricType>::iterator it;
  for (it = active_metrics.begin(); it != active_metrics.end(); ++it)
    results.push_back(GetMetricDetails(*it));

  return results;
}

std::vector<std::string> Database::GetActiveActivities(
    MetricType metric_type, const base::Time& start) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::vector<std::string> results;
  std::string start_key = CreateRecentKey(
      start, static_cast<MetricType>(0), std::string());
  scoped_ptr<leveldb::Iterator> it(recent_db_->NewIterator(read_options_));
  for (it->Seek(start_key); it->Valid(); it->Next()) {
    RecentKey split_key = SplitRecentKey(it->key().ToString());
    if (split_key.type == metric_type)
      results.push_back(split_key.activity);
  }
  return results;
}

bool Database::GetRecentStatsForActivityAndMetric(
    const std::string& activity,
    MetricType metric,
    MetricInfo* info) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string recent_map_key = CreateRecentMapKey(metric, activity);
  if (!ContainsKey(recent_map_, recent_map_key))
    return false;
  std::string recent_key = recent_map_[recent_map_key];

  std::string result;
  leveldb::Status status = recent_db_->Get(read_options_, recent_key, &result);
  if (status.ok())
    *info = MetricInfo(SplitRecentKey(recent_key).time, result);
  return status.ok();
}

Database::MetricInfoVector Database::GetStatsForActivityAndMetric(
    const std::string& activity, MetricType metric_type,
    const base::Time& start, const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  MetricInfoVector results;
  std::string start_key = CreateMetricKey(start, metric_type, activity);
  std::string end_key = CreateMetricKey(end, metric_type, activity);
  scoped_ptr<leveldb::Iterator> it(metric_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    MetricKey split_key = SplitMetricKey(it->key().ToString());
    if (split_key.activity == activity)
      results.push_back(MetricInfo(split_key.time, it->value().ToString()));
  }
  return results;
}

Database::MetricVectorMap Database::GetStatsForMetricByActivity(
    MetricType metric_type, const base::Time& start, const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  MetricVectorMap results;
  std::string start_key = CreateMetricKey(start, metric_type, std::string());
  std::string end_key = CreateMetricKey(end, metric_type, std::string());
  scoped_ptr<leveldb::Iterator> it(metric_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    MetricKey split_key = SplitMetricKey(it->key().ToString());
    if (!results[split_key.activity].get()) {
      results[split_key.activity] =
          linked_ptr<MetricInfoVector>(new MetricInfoVector());
    }
    results[split_key.activity]->push_back(
        MetricInfo(split_key.time, it->value().ToString()));
  }
  return results;
}

Database::Database(const FilePath& path)
    : path_(path),
      read_options_(leveldb::ReadOptions()),
      write_options_(leveldb::WriteOptions()) {
  InitDBs();
  LoadRecents();
  clock_ = scoped_ptr<Clock>(new SystemClock());
}

Database::~Database() {
}

void Database::InitDBs() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  leveldb::DB* new_db = NULL;
  leveldb::Options open_options;
  open_options.create_if_missing = true;

#if defined(OS_POSIX)
  leveldb::DB::Open(open_options, path_.AppendASCII(kRecentDb).value(),
                    &new_db);
  recent_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options, path_.AppendASCII(kStateDb).value(),
                    &new_db);
  state_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options, path_.AppendASCII(kActiveIntervalDb).value(),
                    &new_db);
  active_interval_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options, path_.AppendASCII(kMetricDb).value(),
                    &new_db);
  metric_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options, path_.AppendASCII(kEventDb).value(),
                    &new_db);
  event_db_ = scoped_ptr<leveldb::DB>(new_db);
#elif defined(OS_WIN)
  leveldb::DB::Open(open_options,
                    WideToUTF8(path_.AppendASCII(kRecentDb).value()), &new_db);
  recent_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options,
                    WideToUTF8(path_.AppendASCII(kStateDb).value()), &new_db);
  state_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options,
                    WideToUTF8(path_.AppendASCII(kActiveIntervalDb).value()),
                    &new_db);
  active_interval_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options,
                    WideToUTF8(path_.AppendASCII(kMetricDb).value()), &new_db);
  metric_db_ = scoped_ptr<leveldb::DB>(new_db);
  leveldb::DB::Open(open_options,
                    WideToUTF8(path_.AppendASCII(kEventDb).value()), &new_db);
  event_db_ = scoped_ptr<leveldb::DB>(new_db);
#endif
}

bool Database::Close() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  metric_db_.reset();
  recent_db_.reset();
  state_db_.reset();
  active_interval_db_.reset();
  start_time_key_.clear();
  return true;
}

void Database::LoadRecents() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  recent_map_.clear();
  scoped_ptr<leveldb::Iterator> it(recent_db_->NewIterator(read_options_));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    RecentKey split_key = SplitRecentKey(it->key().ToString());
    recent_map_[CreateRecentMapKey(split_key.type, split_key.activity)] =
        it->key().ToString();
  }
}

// TODO(chebert): Only update the active interval under certian circumstances
// eg. every 10 times or when forced.
void Database::UpdateActiveInterval() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::Time current_time = clock_->GetTime();
  std::string end_time;
  // If the last update was too long ago.
  if (start_time_key_.empty() ||
      current_time - last_update_time_ > kActiveIntervalTimeout) {
    start_time_key_ = CreateActiveIntervalKey(current_time);
    end_time = start_time_key_;
  } else {
    end_time = CreateActiveIntervalKey(clock_->GetTime());
  }
  last_update_time_ = current_time;
  active_interval_db_->Put(write_options_, start_time_key_, end_time);
}

}  // namespace performance_monitor
