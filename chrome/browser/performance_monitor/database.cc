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
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace {
const char kDbDir[] = "Performance Monitor Databases";
const char kRecentDb[] = "Recent Metrics";
const char kEventDb[] = "Events";
const char kStateDb[] = "Configuration";
const char kActiveIntervalDb[] = "Active Interval";
const char kMetricDb[] = "Metrics";
const char kDelimiter = '!';

struct RecentKey {
  RecentKey(const std::string& recent_time, const std::string& recent_metric,
           const std::string& recent_activity)
      : time(recent_time),
        metric(recent_metric),
        activity(recent_activity) {}
  ~RecentKey() {}

  const std::string time;
  const std::string metric;
  const std::string activity;
};

struct MetricKey {
  MetricKey(const std::string& metric_time, const std::string& metric_metric,
           const std::string& metric_activity)
      : time(metric_time),
        metric(metric_metric),
        activity(metric_activity) {}
  ~MetricKey() {}

  const std::string time;
  const std::string metric;
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
  RECENT_METRIC,  // The unique identifier for the type of metric gathered.
  RECENT_ACTIVITY  // The unique identifier for the activity.
};

// The position of different elements in the key for a metric db.
enum MetricKeyPosition {
  METRIC_METRIC,  // The unique identifier for the metric.
  METRIC_TIME,  // The time the stat was gathered.
  METRIC_ACTIVITY  // The unique identifier for the activity.
};

// If the db is quiet for this number of microseconds, then it is considered
// down.
const base::TimeDelta kActiveIntervalTimeout = base::TimeDelta::FromSeconds(5);

// Create the key used for internal active interval monitoring.
// Key Schema: <Time>
std::string CreateActiveIntervalKey(const base::Time& time) {
  return StringPrintf("%016" PRId64, time.ToInternalValue());
}

// Create the database key for a particular event.
// Key Schema: <Time>-<Event Type>
// Where '-' represents the delimiter.
std::string CreateEventKey(const base::Time& time,
                           performance_monitor::EventType type) {
  return StringPrintf("%016" PRId64 "%c%04d", time.ToInternalValue(),
                      kDelimiter, static_cast<int>(type));
}

// Create the database key for a certain metric to go in the "Recent" database.
// Key Schema: <Time>-<Metric>-<Activity>
std::string CreateRecentKey(const base::Time& time,
                            const std::string& metric,
                            const std::string& activity) {
  return StringPrintf("%016" PRId64 "%c%s%c%s", time.ToInternalValue(),
                      kDelimiter, metric.c_str(), kDelimiter, activity.c_str());
}

RecentKey SplitRecentKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  return RecentKey(split[RECENT_TIME], split[RECENT_METRIC],
                   split[RECENT_ACTIVITY]);
}

// Create the database key for a statistic of a certain metric.
// Key Schema: <Metric>-<Time>-<Activity>
std::string CreateMetricKey(const base::Time& time,
                            const std::string& metric,
                            const std::string& activity) {
  return StringPrintf("%s%c%016" PRId64 "%c%s", metric.c_str(), kDelimiter,
                      time.ToInternalValue(), kDelimiter, activity.c_str());
}

MetricKey SplitMetricKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  return MetricKey(split[METRIC_TIME], split[METRIC_METRIC],
                   split[METRIC_ACTIVITY]);
}

// Create the key for recent_map_.
// Key Schema: <Activity>-<Metric>
std::string CreateRecentMapKey(const std::string& metric,
                               const std::string& activity) {
  return activity + kDelimiter + metric;
}

int EventKeyToType(const std::string& event_key) {
  std::vector<std::string> split;
  base::SplitString(event_key, kDelimiter, &split);
  int event_type;
  base::StringToInt(split[EVENT_TYPE], &event_type);
  return event_type;
}
}  // namespace

namespace performance_monitor {

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
  int64 start_time;
  int64 end_time;
  // Check the previous value in case we jumped in in the middle of an active
  // interval.
  if (it->Valid()) {
    it->Prev();
    if (it->Valid() && it->value().ToString() > start_key) {
      base::StringToInt64(it->key().ToString(), &start_time);
      base::StringToInt64(it->value().ToString(), &end_time);
      results.push_back(TimeRange(base::Time::FromInternalValue(start_time),
                                  base::Time::FromInternalValue(end_time)));
    }
  }
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() < end_key;
       it->Next()) {
    base::StringToInt64(it->key().ToString(), &start_time);
    base::StringToInt64(it->value().ToString(), &end_time);
    results.push_back(TimeRange(base::Time::FromInternalValue(start_time),
                                base::Time::FromInternalValue(end_time)));
  }
  return results;
}

Database::EventList Database::GetEvents(EventType type, const base::Time& start,
                                        const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventList events;
  std::string start_key = CreateEventKey(start, EVENT_UNDEFINED);
  std::string end_key = CreateEventKey(end, EVENT_UNDEFINED);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    if (type != EVENT_UNDEFINED) {
      int key_type = EventKeyToType(it->key().ToString());
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
  std::string end_key = CreateEventKey(end, EVENT_UNDEFINED);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    int key_type = EventKeyToType(it->key().ToString());
    results.insert(static_cast<EventType>(key_type));
  }
  return results;
}

bool Database::AddMetric(const std::string& activity, const std::string& metric,
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

void Database::AddMetricDetails(const MetricDetails& details) {
  UpdateActiveInterval();
  metric_details_map_[details.name] = details;
}

std::vector<MetricDetails> Database::GetActiveMetrics(const base::Time& start,
                                                      const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  std::vector<MetricDetails> results;
  std::string recent_start_key = CreateRecentKey(start, std::string(),
                                                 std::string());
  std::string recent_end_key = CreateRecentKey(end, std::string(),
                                               std::string());
  std::string recent_end_of_time_key = CreateRecentKey(clock_->GetTime(),
                                                       std::string(),
                                                       std::string());
  std::set<std::string> active_metrics;
  // Get all the guaranteed metrics.
  scoped_ptr<leveldb::Iterator> recent_it(
      recent_db_->NewIterator(read_options_));
  for (recent_it->Seek(recent_start_key);
       recent_it->Valid() && recent_it->key().ToString() <= recent_end_key;
       recent_it->Next()) {
    RecentKey split_key = SplitRecentKey(recent_it->key().ToString());
    active_metrics.insert(split_key.metric);
  }
  // Get all the possible metrics (metrics that may have been updated after
  // |end|).
  std::set<std::string> possible_metrics;
  for (recent_it->Seek(recent_end_key);
       recent_it->Valid() &&
       recent_it->key().ToString() <= recent_end_of_time_key;
       recent_it->Next()) {
    RecentKey split_key = SplitRecentKey(recent_it->key().ToString());
    possible_metrics.insert(split_key.metric);
  }
  std::set<std::string>::iterator possible_it;
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
  std::set<std::string>::iterator it;
  for (it = active_metrics.begin(); it != active_metrics.end(); ++it) {
    MetricDetailsMap::iterator metric_it;
    metric_it = metric_details_map_.find(*it);
    if (metric_it == metric_details_map_.end())
      results.push_back(MetricDetails(*it, kMetricNotFoundError));
    else
      results.push_back(metric_it->second);
  }
  return results;
}

std::vector<std::string> Database::GetActiveActivities(
    const std::string& metric, const base::Time& start) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  std::vector<std::string> results;
  std::string start_key = CreateRecentKey(start, std::string(), std::string());
  scoped_ptr<leveldb::Iterator> it(recent_db_->NewIterator(read_options_));
  for (it->Seek(start_key); it->Valid(); it->Next()) {
    RecentKey split_key = SplitRecentKey(it->key().ToString());
    if (split_key.metric == metric)
      results.push_back(split_key.activity);
  }
  return results;
}

Database::MetricInfoVector Database::GetStatsForActivityAndMetric(
    const std::string& activity, const std::string& metric,
    const base::Time& start, const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  MetricInfoVector results;
  std::string start_key = CreateMetricKey(start, metric, activity);
  std::string end_key = CreateMetricKey(end, metric, activity);
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
    const std::string& metric, const base::Time& start, const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  MetricVectorMap results;
  std::string start_key = CreateMetricKey(start, metric, std::string());
  std::string end_key = CreateMetricKey(end, metric, std::string());
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

void Database::InitMetricDetails() {
  // TODO(mtytel): Delete this sample metric when a real one is added.
  metric_details_map_[kSampleMetricName] =
      MetricDetails(kSampleMetricName, kSampleMetricDescription);
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
    recent_map_[CreateRecentMapKey(split_key.metric, split_key.activity)] =
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
  active_interval_db_->Put(write_options_, start_time_key_, end_time);
}

}  // namespace performance_monitor
