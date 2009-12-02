// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use
// of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_metrics.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/histogram.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"

namespace chromeos {

// The interval between external metrics collections, in milliseconds.
static const int kExternalMetricsCollectionIntervalMs = 30 * 1000;

// External_metrics_profile could be a member of ExternalMetrics, but then all
// the RecordSomething functions would have to be member functions too, and
// would have to be declared in the class, and there is really no point.
static Profile* external_metrics_profile = NULL;

// There is one of the following functions for every user action as we have to
// call RecordAction in a way that gets picked up by the processing scripts.

static void RecordTabOverviewKeystroke(const char* ignore) {
  UserMetrics::RecordAction("TabOverview_Keystroke", external_metrics_profile);
}

static void RecordTabOverviewExitMouse(const char* ignore) {
  UserMetrics::RecordAction("TabOverview_ExitMouse", external_metrics_profile);
}

static void RecordBootTime(const char* info) {
  // Time string is a whole number of milliseconds.
  int64 time = atol(info);
  UMA_HISTOGRAM_CUSTOM_TIMES("ChromeOS.Boot Time",
                             base::TimeDelta::FromMilliseconds(time),
                             base::TimeDelta::FromSeconds(0),
                             base::TimeDelta::FromSeconds(60),
                             100);
}

static void RecordUpTime(const char* info) {
  int64 time = atol(info);
  UMA_HISTOGRAM_LONG_TIMES("ChromeOS.Uptime",
                           base::TimeDelta::FromSeconds(time));
}

void ExternalMetrics::Start(Profile* profile) {
  DCHECK(external_metrics_profile == NULL);
  external_metrics_profile = profile;
  SetRecorder(&RecordEvent);
  InitializeFunctionTable();
  ScheduleCollector();
}

ExternalMetrics::~ExternalMetrics() {
  LOG_IF(WARNING, external_metrics_profile == NULL) <<
    "external metrics class was never started";
  external_metrics_profile = NULL;
}

void ExternalMetrics::RecordActionWrapper(RecordFunctionType f) {
  if (external_metrics_profile != NULL) {
    f(NULL);
  }
}

// Record Function Entry
#define RF_ENTRY(s, type) { #s, Record ## s, type }

ExternalMetrics::RecordFunctionTableEntry ExternalMetrics::function_table_[] = {
  // These entries MUST be in alphabetical order.
  RF_ENTRY(BootTime, EVENT_TYPE_HISTOGRAM),
  RF_ENTRY(TabOverviewExitMouse, EVENT_TYPE_ACTION),
  RF_ENTRY(TabOverviewKeystroke, EVENT_TYPE_ACTION),
  RF_ENTRY(UpTime, EVENT_TYPE_HISTOGRAM),
};

// Finds the table entry for |name|.
const ExternalMetrics::RecordFunctionTableEntry*
  ExternalMetrics::FindRecordEntry(const char* name) {
  // Use binary search. (TODO(semenzato): map, hash map?)
  int low = 0;
  int high = ARRAYSIZE_UNSAFE(function_table_);

  while (low < high) {
    int middle = (high + low) / 2;
    int comparison = strcmp(name, function_table_[middle].name);
    if (comparison == 0) {
      return &function_table_[middle];
    } else if (comparison < 0) {
      high = middle;
    } else {
      low = middle + 1;
    }
  }
  return NULL;
}

void ExternalMetrics::InitializeFunctionTable() {
  int n = ARRAYSIZE_UNSAFE(function_table_);
  // Check that table is in alphabetical order.  This should be a compile-time
  // check, but this ain't Lisp so we settle for checking the debug builds.
  for (int i = 0; i < n - 1; i++) {
    DCHECK(strcmp(function_table_[i].name, function_table_[i+1].name) < 0);
  }
}

void ExternalMetrics::RecordEvent(const char* name, const char* value) {
  const RecordFunctionTableEntry* entry = FindRecordEntry(name);
  if (entry == NULL) {
    // TODO(semenzato): should do this only once for each name.
    LOG(WARNING) << "no UMA recording function for external event " << name;
  } else {
    switch (entry->type) {
    case EVENT_TYPE_ACTION:
      ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&RecordActionWrapper, entry->function));
      break;
    case EVENT_TYPE_HISTOGRAM:
      entry->function(value);
      break;
    default:
      NOTREACHED();
      break;
    }
  }
}

void ExternalMetrics::CollectEvents() {
  const char* event_file_path = "/tmp/.chromeos-metrics";
  struct stat stat_buf;
  int result;
  result = stat(event_file_path, &stat_buf);
  if (result < 0) {
    if (errno != ENOENT) {
      PLOG(ERROR) << event_file_path << ": cannot collect metrics";
    }
    // Nothing to collect---try later.
    return;
  }
  if (stat_buf.st_size == 0) {
    // Also nothing to collect.
    return;
  }
  int fd = open(event_file_path, O_RDWR);
  if (fd < 0) {
    PLOG(ERROR) << event_file_path << ": cannot open";
    return;
  }
  result = flock(fd, LOCK_EX);
  if (result < 0) {
    PLOG(ERROR) << event_file_path << ": cannot lock";
    return;
  }
  // This processes all messages in the log.  Each message starts with a 4-byte
  // field containing the length of the entire message.  The length is followed
  // by a name-value pair of null-terminated strings.  When all messages are
  // read and processed, or an error occurs, truncate the file to zero size.
  for (;;) {
    int32 message_size;
    result = HANDLE_EINTR(read(fd, &message_size, sizeof(message_size)));
    if (result < 0) {
      PLOG(ERROR) << "reading metrics message header";
      break;
    }
    if (result == 0) {  // normal EOF
      break;
    }
    if (result < static_cast<int>(sizeof(message_size))) {
      LOG(ERROR) << "bad read size " << result <<
                    ", expecting " << sizeof(message_size);
      break;
    }
    // kMetricsMessageMaxLength applies to the entire message: the 4-byte
    // length field and the two null-terminated strings.
    if (message_size < 2 + static_cast<int>(sizeof(message_size)) ||
        message_size > static_cast<int>(kMetricsMessageMaxLength)) {
      LOG(ERROR) << "bad message size " << message_size;
      break;
    }
    message_size -= sizeof(message_size);  // already read this much
    uint8 buffer[kMetricsMessageMaxLength];
    result = HANDLE_EINTR(read(fd, buffer, message_size));
    if (result < 0) {
      PLOG(ERROR) << "reading metrics message body";
      break;
    }
    if (result < message_size) {
      LOG(ERROR) << "message too short: length " << result <<
                    ", expected " << message_size;
      break;
    }
    // The buffer should now contain a pair of null-terminated strings.
    uint8* p = reinterpret_cast<uint8*>(memchr(buffer, '\0', message_size));
    uint8* q = NULL;
    if (p != NULL) {
      q = reinterpret_cast<uint8*>(
        memchr(p + 1, '\0', message_size - (p + 1 - buffer)));
    }
    if (q == NULL) {
      LOG(ERROR) << "bad name-value pair for metrics";
      break;
    } else {
      char* name = reinterpret_cast<char*>(buffer);
      char* value = reinterpret_cast<char*>(p + 1);
      recorder_(name, value);
    }
  }

  result = ftruncate(fd, 0);
  if (result < 0) {
    PLOG(ERROR) << "truncate metrics log";
  }
  result = flock(fd, LOCK_UN);
  if (result < 0) {
    PLOG(ERROR) << "unlock metrics log";
  }
  result = close(fd);
  if (result < 0) {
    PLOG(ERROR) << "close metrics log";
  }
}

void ExternalMetrics::CollectEventsAndReschedule() {
  CollectEvents();
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  bool result;
  result = ChromeThread::PostDelayedTask(
    ChromeThread::FILE, FROM_HERE, NewRunnableMethod(
      this, &chromeos::ExternalMetrics::CollectEventsAndReschedule),
    kExternalMetricsCollectionIntervalMs);
  DCHECK(result);
}

}  // namespace chromeos
