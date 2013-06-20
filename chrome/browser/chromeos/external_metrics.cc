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
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/perftimer.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time.h"
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


// Helper function for ChromeOS field trials whose group choice is left in a
// file by an external entity.  The file needs to contain a single character
// (a trailing newline character is acceptable, as well) indicating the group.
char GetFieldTrialGroupFromFile(const std::string& name_of_experiment,
                                const std::string& path_to_group_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // The dice for this experiment have been thrown at boot.  The selected group
  // number is stored in a file.
  const base::FilePath kPathToGroupFile(
      FILE_PATH_LITERAL(path_to_group_file.c_str()));
  std::string file_content;

  // If the file does not exist, the experiment has not started.
  if (!file_util::ReadFileToString(kPathToGroupFile, &file_content)) {
    LOG(INFO) << name_of_experiment << " field trial file "
              << path_to_group_file << " does not exist.";
    return '\0';
  }

  // The file contains a single significant character followed by a newline.
  if (file_content.empty()) {
    LOG(WARNING) << name_of_experiment << " field trial: "
                 << path_to_group_file << " is empty";
    return '\0';
  }
  if (file_content.size() > 2) {
    // File size includes newline character (since this is only useful under
    // ChromeOS, we only need to deal with single-character newlines).
    LOG(WARNING) << name_of_experiment << " field trial: "
                 << path_to_group_file
                 << " contains an unexpected number of characters"
                 << "(" << file_content.size() << ") "
                 << "'" << file_content << "'";
    return '\0';
  }

  return file_content[0];
}

// Checks to see if the character, potentially describing the field trial,
// group actually corresponds to a group participating in the field trial.
// |name_of_experiment| and |path_to_group_file| (the file that contained the
// character in question) are only for logging. |group_char| is the character
// in question and |legal_group_chars| is the list of characters describing
// groups in the field trial.  The character 'x', which is an implied legal
// character, describes the default/disabled group (i.e., it will not be
// taking part in the field trial).
bool IsGroupInFieldTrial(const std::string& name_of_experiment,
                         const std::string& path_to_group_file,
                         char group_char,
                         const std::string& legal_group_chars) {
  if (group_char == 'x') {
    LOG(INFO) << name_of_experiment << " in default/disabled group";
    return false;
  }
  if (legal_group_chars.find_first_of(group_char) == std::string::npos) {
    LOG(WARNING) << name_of_experiment << " field trial: "
                 << path_to_group_file
                 << " contains an illegal group (" << group_char << ").";
    return false;
  }

  LOG(INFO) << name_of_experiment << " field trial: group " << group_char;
  return true;
}

// Establishes field trial for zram (compressed swap) in chromeos.
// crbug.com/169925
void SetupZramFieldTrial() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const char name_of_experiment[] = "ZRAM";
  const char path_to_group_file[] = "/home/chronos/.swap_exp_enrolled";
  char group_char = GetFieldTrialGroupFromFile(name_of_experiment,
                                               path_to_group_file);
  if (!IsGroupInFieldTrial(name_of_experiment, path_to_group_file, group_char,
                           "012345678")) {
    return;
  }

  const base::FieldTrial::Probability kDivisor = 1;  // on/off only.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(name_of_experiment,
                                                 kDivisor,
                                                 "default",
                                                 2013, 12, 31, NULL);
  // Assign probability of 1 to the group Chrome OS has picked.  Assign 0 to
  // all other choices.
  trial->AppendGroup("2GB_RAM_no_swap", group_char == '0' ? kDivisor : 0);
  trial->AppendGroup("2GB_RAM_2GB_swap", group_char == '1' ? kDivisor : 0);
  trial->AppendGroup("2GB_RAM_3GB_swap", group_char == '2' ? kDivisor : 0);
  trial->AppendGroup("4GB_RAM_no_swap", group_char == '3' ? kDivisor : 0);
  trial->AppendGroup("4GB_RAM_4GB_swap", group_char == '4' ? kDivisor : 0);
  trial->AppendGroup("4GB_RAM_6GB_swap", group_char == '5' ? kDivisor : 0);
  trial->AppendGroup("snow_no_swap", group_char == '6' ? kDivisor : 0);
  trial->AppendGroup("snow_1GB_swap", group_char == '7' ? kDivisor : 0);
  trial->AppendGroup("snow_2GB_swap", group_char == '8' ? kDivisor : 0);

  // Announce the experiment to any listeners (especially important is the UMA
  // software, which will append the group names to UMA statistics).
  trial->group();
  LOG(INFO) << "Configured in group '" << trial->group_name() << "' for "
            << name_of_experiment << " field trial";
}

// Establishes field trial for wifi scanning in chromeos.  crbug.com/242733.
void SetupProgressiveScanFieldTrial() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const char name_of_experiment[] = "ProgressiveScan";
  const char path_to_group_file[] = "/home/chronos/.progressive_scan_variation";
  char group_char = GetFieldTrialGroupFromFile(name_of_experiment,
                                               path_to_group_file);
  if (!IsGroupInFieldTrial(name_of_experiment, path_to_group_file, group_char,
                           "c1234")) {
    return;
  }

  const base::FieldTrial::Probability kDivisor = 1;  // on/off only.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(name_of_experiment,
                                                 kDivisor,
                                                 "default",
                                                 2013, 12, 31, NULL);
  // Assign probability of 1 to the group Chrome OS has picked.  Assign 0 to
  // all other choices.
  trial->AppendGroup("FullScan", group_char == 'c' ? kDivisor : 0);
  trial->AppendGroup("33Percent_4MinMax", group_char == '1' ? kDivisor : 0);
  trial->AppendGroup("50Percent_4MinMax", group_char == '2' ? kDivisor : 0);
  trial->AppendGroup("50Percent_8MinMax", group_char == '3' ? kDivisor : 0);
  trial->AppendGroup("100Percent_8MinMax", group_char == '4' ? kDivisor : 0);

  // Announce the experiment to any listeners (especially important is the UMA
  // software, which will append the group names to UMA statistics).
  trial->group();
  LOG(INFO) << "Configured in group '" << trial->group_name() << "' for "
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

  // Initialize any chromeos field trials that need to read from a file (e.g.,
  // those that have an upstart script determine their experimental group for
  // them) then schedule the data collection.  All of this is done on the file
  // thread.
  bool task_posted = BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&chromeos::ExternalMetrics::SetupAllFieldTrials, this));
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

void ExternalMetrics::SetupAllFieldTrials() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SetupZramFieldTrial();
  SetupProgressiveScanFieldTrial();
  ScheduleCollector();
}

}  // namespace chromeos
