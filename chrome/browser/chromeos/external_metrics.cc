// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
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
#include "base/metrics/histogram.h"
#include "base/perftimer.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

// The interval between external metrics collections, in milliseconds.
static const int kExternalMetricsCollectionIntervalMs = 30 * 1000;

ExternalMetrics::ExternalMetrics()
    : test_recorder_(NULL) {
}

void ExternalMetrics::Start() {
  // Register user actions external to the browser.
  // chrome/tools/extract_actions.py won't understand these lines, so all of
  // these are explicitly added in that script.
  // TODO(derat): We shouldn't need to verify actions before reporting them;
  // remove all of this once http://crosbug.com/11125 is fixed.
  valid_user_actions_.insert("Accel_NextWindow_Tab");
  valid_user_actions_.insert("Accel_PrevWindow_Tab");
  valid_user_actions_.insert("Accel_NextWindow_F5");
  valid_user_actions_.insert("Accel_PrevWindow_F5");
  valid_user_actions_.insert("Accel_BrightnessDown_F6");
  valid_user_actions_.insert("Accel_BrightnessUp_F7");

  ScheduleCollector();
}

void ExternalMetrics::RecordActionUI(std::string action_string) {
  if (valid_user_actions_.count(action_string)) {
    UserMetrics::RecordComputedAction(action_string);
  } else {
    LOG(ERROR) << "undefined UMA action: " << action_string;
  }
}

void ExternalMetrics::RecordAction(const char* action) {
  std::string action_string(action);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &ExternalMetrics::RecordActionUI, action_string));
}

void ExternalMetrics::RecordCrashUI(const std::string& crash_kind) {
  if (g_browser_process && g_browser_process->metrics_service()) {
    g_browser_process->metrics_service()->LogChromeOSCrash(crash_kind);
  }
}

void ExternalMetrics::RecordCrash(const std::string& crash_kind) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &ExternalMetrics::RecordCrashUI, crash_kind));
}

void ExternalMetrics::RecordHistogram(const char* histogram_data) {
  int sample, min, max, nbuckets;
  char name[128];   // length must be consistent with sscanf format below.
  int n = sscanf(histogram_data, "%127s %d %d %d %d",
                 name, &sample, &min, &max, &nbuckets);
  if (n != 5) {
    LOG(ERROR) << "bad histogram request: " << histogram_data;
    return;
  }
  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  scoped_refptr<base::Histogram> counter = base::Histogram::FactoryGet(
      name, min, max, nbuckets, base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ExternalMetrics::RecordLinearHistogram(const char* histogram_data) {
  int sample, max;
  char name[128];   // length must be consistent with sscanf format below.
  int n = sscanf(histogram_data, "%127s %d %d", name, &sample, &max);
  if (n != 3) {
    LOG(ERROR) << "bad linear histogram request: " << histogram_data;
    return;
  }
  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  scoped_refptr<base::Histogram> counter = base::LinearHistogram::FactoryGet(
      name, 1, max, max + 1, base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ExternalMetrics::CollectEvents() {
  const char* event_file_path = "/var/log/metrics/uma-events";
  struct stat stat_buf;
  int result;
  if (!test_path_.empty()) {
    event_file_path = test_path_.value().c_str();
  }
  result = stat(event_file_path, &stat_buf);
  if (result < 0) {
    if (errno != ENOENT) {
      PLOG(ERROR) << event_file_path << ": bad metrics file stat";
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
    close(fd);
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
      if (test_recorder_ != NULL) {
        test_recorder_(name, value);
      } else if (strcmp(name, "crash") == 0) {
        RecordCrash(value);
      } else if (strcmp(name, "histogram") == 0) {
        RecordHistogram(value);
      } else if (strcmp(name, "linearhistogram") == 0) {
        RecordLinearHistogram(value);
      } else if (strcmp(name, "useraction") == 0) {
        RecordAction(value);
      } else {
        LOG(ERROR) << "invalid event type: " << name;
      }
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
  PerfTimer timer;
  CollectEvents();
  UMA_HISTOGRAM_TIMES("UMA.CollectExternalEventsTime", timer.Elapsed());
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  bool result;
  result = BrowserThread::PostDelayedTask(
    BrowserThread::FILE, FROM_HERE, NewRunnableMethod(
        this, &chromeos::ExternalMetrics::CollectEventsAndReschedule),
    kExternalMetricsCollectionIntervalMs);
  DCHECK(result);
}

}  // namespace chromeos
