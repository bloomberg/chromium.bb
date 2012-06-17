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

// The position of different elements in the key for the event db.
enum EventKeyPosition {
  EVENT_TIME,  // The time the event was generated.
  EVENT_TYPE  // The type of event.
};

// If the db is quiet for this number of microseconds, then it is considered
// down.
const base::TimeDelta kActiveIntervalTimeout = base::TimeDelta::FromSeconds(5);

// Create the key used for internal active interval monitoring.
std::string CreateActiveIntervalKey(const base::Time& time) {
  return StringPrintf("%016" PRId64, time.ToInternalValue());
}

// Create the database key for a particular event.
std::string CreateEventKey(const base::Time& time,
                           performance_monitor::EventType type) {
  return StringPrintf("%016" PRId64 "%c%04d", time.ToInternalValue(),
                      kDelimiter, static_cast<int>(type));
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
scoped_refptr<Database> Database::Create(FilePath path) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (path.empty()) {
    CHECK(PathService::Get(chrome::DIR_USER_DATA, &path));
    path = path.AppendASCII(kDbDir);
  }
  if (!file_util::DirectoryExists(path) && !file_util::CreateDirectory(path))
    return scoped_refptr<Database>();
  return scoped_refptr<Database>(new Database(path));
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

Database::Database(const FilePath& path)
    : path_(path),
      read_options_(leveldb::ReadOptions()),
      write_options_(leveldb::WriteOptions()) {
  InitDBs();
  clock_ = scoped_ptr<Clock>(new SystemClock());
}

Database::~Database() {
  Close();
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

// TODO(eriq): Only update the active interval under certian circumstances eg.
// every 10 times or when forced.
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
