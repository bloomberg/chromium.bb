// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/database.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/performance_monitor/key_builder.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_monitor {
namespace {
const char kDbDir[] = "Performance Monitor Databases";
const char kRecentDb[] = "Recent Metrics";
const char kMaxValueDb[] = "Max Value Metrics";
const char kEventDb[] = "Events";
const char kStateDb[] = "Configuration";
const char kActiveIntervalDb[] = "Active Interval";
const char kMetricDb[] = "Metrics";
const double kDefaultMaxValue = 0.0;

// If the db is quiet for this number of minutes, then it is considered down.
const base::TimeDelta kActiveIntervalTimeout = base::TimeDelta::FromMinutes(5);

TimeRange ActiveIntervalToTimeRange(const std::string& start_time,
                                    const std::string& end_time) {
  int64 start_time_int = 0;
  int64 end_time_int = 0;
  base::StringToInt64(start_time, &start_time_int);
  base::StringToInt64(end_time, &end_time_int);
  return TimeRange(base::Time::FromInternalValue(start_time_int),
                   base::Time::FromInternalValue(end_time_int));
}

double StringToDouble(const std::string& s) {
  double value = 0.0;
  if (!base::StringToDouble(s, &value))
    LOG(ERROR) << "Failed to convert " << s << " to double.";
  return value;
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
  std::string key = key_builder_->CreateEventKey(event.time(), event.type());
  leveldb::Status status = event_db_->Put(write_options_, key, value);
  return status.ok();
}

std::vector<TimeRange> Database::GetActiveIntervals(const base::Time& start,
                                                    const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::vector<TimeRange> results;
  std::string start_key = key_builder_->CreateActiveIntervalKey(start);
  std::string end_key = key_builder_->CreateActiveIntervalKey(end);
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

Database::EventVector Database::GetEvents(EventType type,
                                          const base::Time& start,
                                          const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventVector events;
  std::string start_key =
      key_builder_->CreateEventKey(start, EVENT_UNDEFINED);
  std::string end_key =
      key_builder_->CreateEventKey(end, EVENT_NUMBER_OF_EVENTS);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    if (type != EVENT_UNDEFINED) {
      EventType key_type =
          key_builder_->EventKeyToEventType(it->key().ToString());
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
  std::string start_key =
      key_builder_->CreateEventKey(start, EVENT_UNDEFINED);
  std::string end_key =
      key_builder_->CreateEventKey(end, EVENT_NUMBER_OF_EVENTS);
  scoped_ptr<leveldb::Iterator> it(event_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    EventType key_type =
        key_builder_->EventKeyToEventType(it->key().ToString());
    results.insert(key_type);
  }
  return results;
}

bool Database::AddMetric(const std::string& activity,
                         MetricType metric,
                         const std::string& value) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateActiveInterval();
  base::Time timestamp = clock_->GetTime();
  std::string recent_key =
      key_builder_->CreateRecentKey(timestamp, metric, activity);
  std::string metric_key =
      key_builder_->CreateMetricKey(timestamp, metric, activity);
  std::string recent_map_key =
      key_builder_->CreateRecentMapKey(metric, activity);
  // Use recent_map_ to quickly find the key that must be removed.
  RecentMap::iterator old_it = recent_map_.find(recent_map_key);
  if (old_it != recent_map_.end())
    recent_db_->Delete(write_options_, old_it->second);
  recent_map_[recent_map_key] = recent_key;
  leveldb::Status recent_status =
      recent_db_->Put(write_options_, recent_key, value);
  leveldb::Status metric_status =
      metric_db_->Put(write_options_, metric_key, value);

  bool max_value_success = UpdateMaxValue(activity, metric, value);
  return recent_status.ok() && metric_status.ok() && max_value_success;
}

bool Database::UpdateMaxValue(const std::string& activity,
                              MetricType metric,
                              const std::string& value) {
  std::string max_value_key(
      key_builder_->CreateMaxValueKey(metric, activity));
  bool has_key = ContainsKey(max_value_map_, max_value_key);
  if ((has_key && StringToDouble(value) > max_value_map_[max_value_key]) ||
      !has_key) {
    max_value_map_[max_value_key] = StringToDouble(value);
    return max_value_db_->Put(write_options_, max_value_key, value).ok();
  }

  return true;
}

Database::MetricTypeSet Database::GetActiveMetrics(const base::Time& start,
                                                   const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string recent_start_key = key_builder_->CreateRecentKey(
      start, static_cast<MetricType>(0), std::string());
  std::string recent_end_key = key_builder_->CreateRecentKey(
      end, METRIC_NUMBER_OF_METRICS, std::string());
  std::string recent_end_of_time_key = key_builder_->CreateRecentKey(
      clock_->GetTime(), METRIC_NUMBER_OF_METRICS, std::string());

  MetricTypeSet active_metrics;
  // Get all the guaranteed metrics.
  scoped_ptr<leveldb::Iterator> recent_it(
      recent_db_->NewIterator(read_options_));
  for (recent_it->Seek(recent_start_key);
       recent_it->Valid() && recent_it->key().ToString() <= recent_end_key;
       recent_it->Next()) {
    RecentKey split_key =
        key_builder_->SplitRecentKey(recent_it->key().ToString());
    active_metrics.insert(split_key.type);
  }
  // Get all the possible metrics (metrics that may have been updated after
  // |end|).
  MetricTypeSet possible_metrics;
  for (recent_it->Seek(recent_end_key);
       recent_it->Valid() &&
       recent_it->key().ToString() <= recent_end_of_time_key;
       recent_it->Next()) {
    RecentKey split_key =
        key_builder_->SplitRecentKey(recent_it->key().ToString());
    possible_metrics.insert(split_key.type);
  }
  MetricTypeSet::iterator possible_it;
  scoped_ptr<leveldb::Iterator> metric_it(
      metric_db_->NewIterator(read_options_));
  for (possible_it = possible_metrics.begin();
       possible_it != possible_metrics.end();
       ++possible_it) {
    std::string metric_start_key =
        key_builder_->CreateMetricKey(start, *possible_it,std::string());
    std::string metric_end_key =
        key_builder_->CreateMetricKey(end, *possible_it, std::string());
    metric_it->Seek(metric_start_key);
    // Stats in the timerange from any activity makes the metric active.
    if (metric_it->Valid() && metric_it->key().ToString() <= metric_end_key) {
      active_metrics.insert(*possible_it);
    }
  }

  return active_metrics;
}

std::set<std::string> Database::GetActiveActivities(MetricType metric_type,
                                                    const base::Time& start) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::set<std::string> results;
  std::string start_key = key_builder_->CreateRecentKey(
      start, static_cast<MetricType>(0), std::string());
  scoped_ptr<leveldb::Iterator> it(recent_db_->NewIterator(read_options_));
  for (it->Seek(start_key); it->Valid(); it->Next()) {
    RecentKey split_key =
        key_builder_->SplitRecentKey(it->key().ToString());
    if (split_key.type == metric_type)
      results.insert(split_key.activity);
  }
  return results;
}

double Database::GetMaxStatsForActivityAndMetric(const std::string& activity,
                                                 MetricType metric) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string max_value_key(
      key_builder_->CreateMaxValueKey(metric, activity));
  if (ContainsKey(max_value_map_, max_value_key))
    return max_value_map_[max_value_key];
  return kDefaultMaxValue;
}

bool Database::GetRecentStatsForActivityAndMetric(const std::string& activity,
                                                  MetricType metric_type,
                                                  Metric* metric) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string recent_map_key =
      key_builder_->CreateRecentMapKey(metric_type, activity);
  if (!ContainsKey(recent_map_, recent_map_key))
    return false;
  std::string recent_key = recent_map_[recent_map_key];

  std::string result;
  leveldb::Status status = recent_db_->Get(read_options_, recent_key, &result);
  if (status.ok())
    *metric = Metric(key_builder_->SplitRecentKey(recent_key).time, result);
  return status.ok();
}

Database::MetricVector Database::GetStatsForActivityAndMetric(
    const std::string& activity,
    MetricType metric_type,
    const base::Time& start,
    const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  MetricVector results;
  std::string start_key =
      key_builder_->CreateMetricKey(start, metric_type, activity);
  std::string end_key =
      key_builder_->CreateMetricKey(end, metric_type, activity);
  scoped_ptr<leveldb::Iterator> it(metric_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    MetricKey split_key =
        key_builder_->SplitMetricKey(it->key().ToString());
    if (split_key.activity == activity)
      results.push_back(Metric(split_key.time, it->value().ToString()));
  }
  return results;
}

Database::MetricVectorMap Database::GetStatsForMetricByActivity(
    MetricType metric_type,
    const base::Time& start,
    const base::Time& end) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  MetricVectorMap results;
  std::string start_key =
      key_builder_->CreateMetricKey(start, metric_type, std::string());
  std::string end_key =
      key_builder_->CreateMetricKey(end, metric_type, std::string());
  scoped_ptr<leveldb::Iterator> it(metric_db_->NewIterator(read_options_));
  for (it->Seek(start_key);
       it->Valid() && it->key().ToString() <= end_key;
       it->Next()) {
    MetricKey split_key = key_builder_->SplitMetricKey(it->key().ToString());
    if (!results[split_key.activity].get()) {
      results[split_key.activity] =
          linked_ptr<MetricVector >(new MetricVector());
    }
    results[split_key.activity]->push_back(
        Metric(split_key.time, it->value().ToString()));
  }
  return results;
}

Database::Database(const FilePath& path)
    : key_builder_(new KeyBuilder()),
      path_(path),
      read_options_(leveldb::ReadOptions()),
      write_options_(leveldb::WriteOptions()) {
  InitDBs();
  LoadRecents();
  LoadMaxValues();
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
  leveldb::DB::Open(open_options,
                    path_.AppendASCII(kMaxValueDb).value(),
                    &new_db);
  max_value_db_ = scoped_ptr<leveldb::DB>(new_db);
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
                    WideToUTF8(path_.AppendASCII(kMaxValueDb).value()),
                    &new_db);
  max_value_db_ = scoped_ptr<leveldb::DB>(new_db);
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
  max_value_db_.reset();
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
    RecentKey split_key = key_builder_->SplitRecentKey(it->key().ToString());
    recent_map_[key_builder_->
        CreateRecentMapKey(split_key.type, split_key.activity)] =
        it->key().ToString();
  }
}

void Database::LoadMaxValues() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  max_value_map_.clear();
  scoped_ptr<leveldb::Iterator> it(max_value_db_->NewIterator(read_options_));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    max_value_map_[it->key().ToString()] =
        StringToDouble(it->value().ToString());
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
    start_time_key_ = key_builder_->CreateActiveIntervalKey(current_time);
    end_time = start_time_key_;
  } else {
    end_time = key_builder_->CreateActiveIntervalKey(clock_->GetTime());
  }
  last_update_time_ = current_time;
  active_interval_db_->Put(write_options_, start_time_key_, end_time);
}

}  // namespace performance_monitor
