// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_metrics.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_info.h"
#include "base/test/perftimer.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace chromeos {

namespace {

bool CheckValues(const std::string& name,
                 int minimum,
                 int maximum,
                 size_t bucket_count) {
  if (!base::Histogram::InspectConstructionArguments(
      name, &minimum, &maximum, &bucket_count))
    return false;
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return true;
  return histogram->HasConstructionArguments(minimum, maximum, bucket_count);
}

bool CheckLinearValues(const std::string& name, int maximum) {
  return CheckValues(name, 1, maximum, maximum + 1);
}

// Establishes field trial for wifi scanning in chromeos.  crbug.com/242733.
void SetupProgressiveScanFieldTrial() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const char name_of_experiment[] = "ProgressiveScan";
  const char path_to_group_file[] = "/home/chronos/.progressive_scan_variation";
  const base::FieldTrial::Probability kDivisor = 1000;
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          name_of_experiment, kDivisor, "Default", 2013, 12, 31,
          base::FieldTrial::SESSION_RANDOMIZED, NULL);

  // Announce the groups with 0 percentage; the actual percentages come from
  // the server configuration.
  std::map<int, std::string> group_to_char;
  group_to_char[trial->AppendGroup("FullScan", 0)] = "c";
  group_to_char[trial->AppendGroup("33Percent_4MinMax", 0)] = "1";
  group_to_char[trial->AppendGroup("50Percent_4MinMax", 0)] = "2";
  group_to_char[trial->AppendGroup("50Percent_8MinMax", 0)] = "3";
  group_to_char[trial->AppendGroup("100Percent_8MinMax", 0)] = "4";

  // Announce the experiment to any listeners (especially important is the UMA
  // software, which will append the group names to UMA statistics).
  const int group_num = trial->group();
  std::string group_char = "x";
  if (ContainsKey(group_to_char, group_num))
    group_char = group_to_char[group_num];

  // Write the group to the file to be read by ChromeOS.
  const base::FilePath kPathToGroupFile(path_to_group_file);

  if (file_util::WriteFile(kPathToGroupFile, group_char.c_str(),
                           group_char.length())) {
    LOG(INFO) << "Configured in group '" << trial->group_name()
              << "' ('" << group_char << "') for "
              << name_of_experiment << " field trial";
  } else {
    LOG(ERROR) << "Couldn't write to " << path_to_group_file;
  }
}

// Finds out if we're on a 2GB Parrot.
//
// This code reads and parses /etc/lsb-release. There are at least four other
// places that open and parse /etc/lsb-release, and I wish I could fix the
// mess.  At least this code is temporary.

bool Is2GBParrot() {
  base::FilePath path("/etc/lsb-release");
  std::string contents;
  if (!file_util::ReadFileToString(path, &contents))
    return false;
  if (contents.find("CHROMEOS_RELEASE_BOARD=parrot") == std::string::npos)
    return false;
  // There are 2GB and 4GB models.
  return base::SysInfo::AmountOfPhysicalMemory() <= 2LL * 1024 * 1024 * 1024;
}

// Sets up field trial for measuring swap and CPU metrics after tab switch
// and scroll events. crbug.com/253994
void SetupSwapJankFieldTrial() {
  const char name_of_experiment[] = "SwapJank64vs32Parrot";

  // Determine if this is a 32 or 64 bit build of Chrome.
  bool is_chrome_64 = sizeof(void*) == 8;

  // Determine if this is a 32 or 64 bit kernel.
  bool is_kernel_64 = base::SysInfo::OperatingSystemArchitecture() == "x86_64";

  // A 32 bit kernel requires 32 bit Chrome.
  DCHECK(is_kernel_64 || !is_chrome_64);

  // Find out if we're on a 2GB Parrot.
  bool is_parrot = Is2GBParrot();

  // All groups are either on or off.
  const base::FieldTrial::Probability kTotalProbability = 1;
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          name_of_experiment, kTotalProbability, "default", 2013, 12, 31,
          base::FieldTrial::SESSION_RANDOMIZED, NULL);

  // Assign probability of 1 to this Chrome's group.  Assign 0 to all other
  // choices.
  trial->AppendGroup("kernel_64_chrome_64",
                     is_parrot && is_kernel_64 && is_chrome_64 ?
                     kTotalProbability : 0);
  trial->AppendGroup("kernel_64_chrome_32",
                     is_parrot && is_kernel_64 && !is_chrome_64 ?
                     kTotalProbability : 0);
  trial->AppendGroup("kernel_32_chrome_32",
                     is_parrot && !is_kernel_64 && !is_chrome_64 ?
                     kTotalProbability : 0);
  trial->AppendGroup("not_parrot",
                     !is_parrot ? kTotalProbability : 0);

  // Announce the experiment to any listeners (especially important is the UMA
  // software, which will append the group names to UMA statistics).
  trial->group();
  DVLOG(1) << "Configured in group '" << trial->group_name() << "' for "
           << name_of_experiment << " field trial";
}

}  // namespace

// The interval between external metrics collections in seconds
static const int kExternalMetricsCollectionIntervalSeconds = 30;

ExternalMetrics::ExternalMetrics() : test_recorder_(NULL) {}

ExternalMetrics::~ExternalMetrics() {}

void ExternalMetrics::Start() {
  // Register user actions external to the browser.
  // chrome/tools/extract_actions.py won't understand these lines, so all of
  // these are explicitly added in that script.
  // TODO(derat): We shouldn't need to verify actions before reporting them;
  // remove all of this once http://crosbug.com/11125 is fixed.
  valid_user_actions_.insert("Cryptohome.PKCS11InitFail");
  valid_user_actions_.insert("Updater.ServerCertificateChanged");
  valid_user_actions_.insert("Updater.ServerCertificateFailed");

  // Initialize here field trials that don't need to read from files.
  // (None for the moment.)

  // Initialize any chromeos field trials that need to read from a file (e.g.,
  // those that have an upstart script determine their experimental group for
  // them) then schedule the data collection.  All of this is done on the file
  // thread.
  bool task_posted = BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&chromeos::ExternalMetrics::SetupFieldTrialsOnFileThread,
                 this));
  DCHECK(task_posted);
}

void ExternalMetrics::RecordActionUI(std::string action_string) {
  if (valid_user_actions_.count(action_string)) {
    content::RecordComputedAction(action_string);
  } else {
    DLOG(ERROR) << "undefined UMA action: " << action_string;
  }
}

void ExternalMetrics::RecordAction(const char* action) {
  std::string action_string(action);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalMetrics::RecordActionUI, this, action_string));
}

void ExternalMetrics::RecordCrashUI(const std::string& crash_kind) {
  if (g_browser_process && g_browser_process->metrics_service()) {
    g_browser_process->metrics_service()->LogChromeOSCrash(crash_kind);
  }
}

void ExternalMetrics::RecordCrash(const std::string& crash_kind) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalMetrics::RecordCrashUI, this, crash_kind));
}

void ExternalMetrics::RecordHistogram(const char* histogram_data) {
  int sample, min, max, nbuckets;
  char name[128];   // length must be consistent with sscanf format below.
  int n = sscanf(histogram_data, "%127s %d %d %d %d",
                 name, &sample, &min, &max, &nbuckets);
  if (n != 5) {
    DLOG(ERROR) << "bad histogram request: " << histogram_data;
    return;
  }

  if (!CheckValues(name, min, max, nbuckets)) {
    DLOG(ERROR) << "Invalid histogram " << name
                << ", min=" << min
                << ", max=" << max
                << ", nbuckets=" << nbuckets;
    return;
  }
  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      name, min, max, nbuckets, base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ExternalMetrics::RecordLinearHistogram(const char* histogram_data) {
  int sample, max;
  char name[128];   // length must be consistent with sscanf format below.
  int n = sscanf(histogram_data, "%127s %d %d", name, &sample, &max);
  if (n != 3) {
    DLOG(ERROR) << "bad linear histogram request: " << histogram_data;
    return;
  }

  if (!CheckLinearValues(name, max)) {
    DLOG(ERROR) << "Invalid linear histogram " << name
                << ", max=" << max;
    return;
  }
  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      name, 1, max, max + 1, base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ExternalMetrics::RecordSparseHistogram(const char* histogram_data) {
  int sample;
  char name[128];   // length must be consistent with sscanf format below.
  int n = sscanf(histogram_data, "%127s %d", name, &sample);
  if (n != 2) {
    DLOG(ERROR) << "bad sparse histogram request: " << histogram_data;
    return;
  }

  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  base::HistogramBase* counter = base::SparseHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
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
      DPLOG(ERROR) << event_file_path << ": bad metrics file stat";
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
    DPLOG(ERROR) << event_file_path << ": cannot open";
    return;
  }
  result = flock(fd, LOCK_EX);
  if (result < 0) {
    DPLOG(ERROR) << event_file_path << ": cannot lock";
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
      DPLOG(ERROR) << "reading metrics message header";
      break;
    }
    if (result == 0) {  // This indicates a normal EOF.
      break;
    }
    if (result < static_cast<int>(sizeof(message_size))) {
      DLOG(ERROR) << "bad read size " << result <<
                     ", expecting " << sizeof(message_size);
      break;
    }
    // kMetricsMessageMaxLength applies to the entire message: the 4-byte
    // length field and the two null-terminated strings.
    if (message_size < 2 + static_cast<int>(sizeof(message_size)) ||
        message_size > static_cast<int>(kMetricsMessageMaxLength)) {
      DLOG(ERROR) << "bad message size " << message_size;
      break;
    }
    message_size -= sizeof(message_size);  // The message size includes itself.
    uint8 buffer[kMetricsMessageMaxLength];
    result = HANDLE_EINTR(read(fd, buffer, message_size));
    if (result < 0) {
      DPLOG(ERROR) << "reading metrics message body";
      break;
    }
    if (result < message_size) {
      DLOG(ERROR) << "message too short: length " << result <<
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
      DLOG(ERROR) << "bad name-value pair for metrics";
      break;
    }
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
    } else if (strcmp(name, "sparsehistogram") == 0) {
      RecordSparseHistogram(value);
    } else if (strcmp(name, "useraction") == 0) {
      RecordAction(value);
    } else {
      DLOG(ERROR) << "invalid event type: " << name;
    }
  }

  result = ftruncate(fd, 0);
  if (result < 0) {
    DPLOG(ERROR) << "truncate metrics log";
  }
  result = flock(fd, LOCK_UN);
  if (result < 0) {
    DPLOG(ERROR) << "unlock metrics log";
  }
  result = close(fd);
  if (result < 0) {
    DPLOG(ERROR) << "close metrics log";
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
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&chromeos::ExternalMetrics::CollectEventsAndReschedule, this),
      base::TimeDelta::FromSeconds(kExternalMetricsCollectionIntervalSeconds));
  DCHECK(result);
}

void ExternalMetrics::SetupFieldTrialsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Field trials that do not read from files can be initialized in
  // ExternalMetrics::Start() above.
  SetupProgressiveScanFieldTrial();
  SetupSwapJankFieldTrial();

  ScheduleCollector();
}

}  // namespace chromeos
