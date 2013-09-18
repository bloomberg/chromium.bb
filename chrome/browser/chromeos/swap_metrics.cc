// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/swap_metrics.h"

#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/event.h"

using base::FilePath;

namespace chromeos {
namespace {

// Time delays for metrics collections, starting after an interesting UI event.
// Times are relative to the UI event. Start with zero to record the initial
// state of the metrics immediately.
const int kMetricsDelayMs[] = { 0, 100, 300, 1000, 3000 };

}  // namespace

///////////////////////////////////////////////////////////////////////////////

// Runs in the blocking thread pool to load metrics and record them.
// Reads data about CPU utilization and swap activity from the /proc and /sys
// file systems. Owned by SwapMetrics on the UI thread.
class SwapMetrics::Backend : public base::RefCountedThreadSafe<Backend> {
 public:
  explicit Backend(const std::string& reason);

  // Records one set of statistics for |time_index| after the interesting
  // event. May trigger another delayed task to record more statistics.
  void RecordMetricsOnBlockingPool(size_t time_index);

  // Sets the thread-safe cancellation flag.
  void CancelOnUIThread() { cancelled_.Set(); }

 private:
  friend class base::RefCountedThreadSafe<Backend>;

  virtual ~Backend();

  // Extracts a field value from a list of name-value pairs
  // in a file (typically a /proc or /sys file). Returns false
  // if the field is not found, or for other errors.
  bool GetFieldFromKernelOutput(const std::string& path,
                                const std::string& field,
                                int64* value);

  // Reads a file whose content is a single line, and returns its content as a
  // list of tokens. |expected_tokens_count| is the expected number of tokens.
  // |delimiters| is a string containing characters that may appear between
  // tokens. Returns true on success, false otherwise.
  bool TokenizeOneLineFile(const std::string& path,
                           size_t expected_tokens_count,
                           const std::string& delimiters,
                           std::vector<std::string>* tokens);

  // Retrieve various system metrics. Return true on success.
  bool GetMeminfoField(const std::string& field, int64* value);
  bool GetUptime(double* uptime_secs, double* idle_time_secs);
  bool GetPageFaults(int64* page_faults);

  // Record histogram samples.
  void RecordFaultsHistogramSample(int faults,
                                   const std::string& reason,
                                   int swap_group,
                                   int time_index);
  void RecordCpuHistogramSample(int cpu,
                                const std::string& reason,
                                int swap_group,
                                int time_index);

  // Cancellation flag that can be written from the UI thread (which owns this
  // object) and read from any thread.
  base::CancellationFlag cancelled_;

  // Data initialized once and shared by all instances of this class.
  static bool first_time_;
  static int64 swap_total_kb_;
  static int number_of_cpus_;

  // Values at the beginning of each sampling interval.
  double last_uptime_secs_;
  double last_idle_time_secs_;
  int64 last_page_faults_;

  // Swap group for a set of samples, chosen at the beginning of the set.
  int swap_group_;

  // Reason for sampling.
  const std::string reason_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// static
bool SwapMetrics::Backend::first_time_ = true;
// static
int64 SwapMetrics::Backend::swap_total_kb_ = 0;
// static
int SwapMetrics::Backend::number_of_cpus_ = 0;

SwapMetrics::Backend::Backend(const std::string& reason)
    : last_uptime_secs_(0.0),
      last_idle_time_secs_(0.0),
      last_page_faults_(0),
      swap_group_(0),
      reason_(reason) {
}

SwapMetrics::Backend::~Backend() {
}

void SwapMetrics::Backend::RecordMetricsOnBlockingPool(size_t time_index) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  // Another UI event might have occurred. Don't run this metrics collection
  // and don't post another task. This object's refcount will drop and it will
  // be deleted when the next backend is created on the UI thread.
  if (cancelled_.IsSet())
    return;
  DVLOG(1) << "RecordMetricsInBlockingPool " << time_index;

  // At init time, get the number of cpus and the total swap. The number of cpus
  // is necessary because the idle time reported is the sum of the idle
  // times of all cpus.
  //
  // When an event occurs:
  // - At time t_0, save the initial values for uptime, idle_time, page_faults,
  //   and swap used.
  //
  // - At time t_i, compute cpu utilization as a fraction (1 = fully utilized):
  //  utilization =
  //      1 - (idle_time_i - idle_time_0) / (uptime_i - uptime_0) / ncpus
  //
  // then UMA-report it in the right swap group. Do the same for page faults.

  if (first_time_) {
    first_time_ = false;
    number_of_cpus_ = base::SysInfo::NumberOfProcessors();
    // Avoid divide by zero in case of errors.
    if (number_of_cpus_ == 0)
      number_of_cpus_ = 1;
    GetMeminfoField("SwapTotal:", &swap_total_kb_);
  }

  if (time_index == 0) {
    // Record baseline data.
    GetUptime(&last_uptime_secs_, &last_idle_time_secs_);
    GetPageFaults(&last_page_faults_);
    int64 swap_free_kb = 0;
    GetMeminfoField("SwapFree:", &swap_free_kb);
    int swap_percent = swap_total_kb_ > 0
        ? (swap_total_kb_ - swap_free_kb) * 100 / swap_total_kb_
        : 0;
    if (swap_percent < 10)
      swap_group_ = 0;
    else if (swap_percent < 30)
      swap_group_ = 1;
    else if (swap_percent < 60)
      swap_group_ = 2;
    else
      swap_group_ = 3;
  } else {
    int64 page_faults = 0;
    double idle_time_secs = 0.0;
    double uptime_secs = 0.0;
    GetUptime(&uptime_secs, &idle_time_secs);
    GetPageFaults(&page_faults);
    double delta_time_secs = uptime_secs - last_uptime_secs_;
    // Unexpected, but not worth agonizing over it.
    if (delta_time_secs == 0)
      return;

    int cpu = (1.0 - (idle_time_secs - last_idle_time_secs_) /
               delta_time_secs / number_of_cpus_) * 100;
    int faults_per_sec = (page_faults - last_page_faults_) / delta_time_secs;

    RecordCpuHistogramSample(cpu, reason_, swap_group_, time_index);
    RecordFaultsHistogramSample(faults_per_sec, reason_,
                                swap_group_, time_index);

    last_uptime_secs_ = uptime_secs;
    last_page_faults_ = page_faults;
    last_idle_time_secs_ = idle_time_secs;
  }

  // Check if another metrics recording is needed.
  if (++time_index >= arraysize(kMetricsDelayMs))
    return;
  PostTaskRecordMetrics(
      scoped_refptr<Backend>(this),
      time_index,
      kMetricsDelayMs[time_index] - kMetricsDelayMs[time_index - 1]);
}

void SwapMetrics::Backend::RecordFaultsHistogramSample(
    int faults,
    const std::string& reason,
    int swap_group,
    int time_index) {
  std::string name =
      base::StringPrintf("Platform.SwapJank.%s.Faults.Swap%d.Time%d",
                         reason.c_str(),
                         swap_group,
                         time_index);
  const int kMinimumBucket = 10;
  const int kMaximumBucket = 200000;
  const size_t kBucketCount = 50;
  base::HistogramBase* counter =
      base::Histogram::FactoryGet(name,
                                  kMinimumBucket,
                                  kMaximumBucket,
                                  kBucketCount,
                                  base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(faults);
}

void SwapMetrics::Backend::RecordCpuHistogramSample(int cpu,
                                                    const std::string& reason,
                                                    int swap_group,
                                                    int time_index) {
  std::string name =
      base::StringPrintf("Platform.SwapJank.%s.Cpu.Swap%d.Time%d",
                         reason.c_str(),
                         swap_group,
                         time_index);
  const int kMinimumBucket = 0;
  const int kMaximumBucket = 101;
  const size_t kBucketCount = 102;
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      name,
      kMinimumBucket,
      kMaximumBucket,
      kBucketCount,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(cpu);
}

// Extracts a field value from a list of name-value pairs
// in a file (typically a /proc or /sys file). Returns false
// if the field is not found, or for other errors.
bool SwapMetrics::Backend::GetFieldFromKernelOutput(const std::string& path,
                                                    const std::string& field,
                                                    int64* value) {
  std::string file_content;
  if (!base::ReadFileToString(FilePath(path), &file_content)) {
    LOG(WARNING) << "Cannot read " << path;
    return false;
  }
  std::vector<std::string> lines;
  size_t line_count = Tokenize(file_content, "\n", &lines);
  if (line_count < 2) {
    LOG(WARNING) << "Error breaking " << path << " into lines";
    return false;
  }
  for (size_t i = 0; i < line_count; ++i) {
    std::vector<std::string> tokens;
    size_t token_count = Tokenize(lines[i], " ", &tokens);
    if (token_count < 2) {
      LOG(WARNING) << "Unexpected line: " << lines[i];
      return false;
    }
    if (tokens[0].compare(field) != 0)
      continue;
    if (!base::StringToInt64(tokens[1], value)) {
      LOG(WARNING) << "Cannot convert " << tokens[1] << " to int";
      return false;
    }
    return true;
  }
  LOG(WARNING) << "could not find field " << field;
  return false;
}

bool SwapMetrics::Backend::TokenizeOneLineFile(const std::string& path,
                                               size_t expected_tokens_count,
                                               const std::string& delimiters,
                                               std::vector<std::string>*
                                               tokens) {
  std::string file_content;
  if (!base::ReadFileToString(FilePath(path), &file_content)) {
    LOG(WARNING) << "cannot read " << path;
    return false;
  }
  size_t tokens_count = Tokenize(file_content, delimiters, tokens);
  if (tokens_count != expected_tokens_count) {
    LOG(WARNING) << "unexpected content of " << path << ": " << file_content;
    return false;
  }
  return true;
}

bool SwapMetrics::Backend::GetMeminfoField(const std::string& name,
                                           int64* value) {
  return GetFieldFromKernelOutput("/proc/meminfo", name, value);
}

bool SwapMetrics::Backend::GetUptime(double* uptime_secs,
                                     double* idle_time_secs) {
  // Get the time since boot.
  const char kUptimePath[] = "/proc/uptime";
  std::vector<std::string> tokens;
  if (!TokenizeOneLineFile(kUptimePath, 2, " \n", &tokens))
    return false;

  if (!base::StringToDouble(tokens[0], uptime_secs)) {
    LOG(WARNING) << "cannot convert " << tokens[0] << " to double";
    return false;
  }

  // Get the idle time since boot. The number available in /proc/stat is more
  // precise, but this one should be good enough.
  if (!base::StringToDouble(tokens[1], idle_time_secs)) {
    LOG(WARNING) << "cannot convert " << tokens[0] << " to double";
    return false;
  }
  return true;
}

bool SwapMetrics::Backend::GetPageFaults(int64* page_faults) {
  // Get number of page faults.
  return GetFieldFromKernelOutput("/proc/vmstat", "pgmajfault", page_faults);
}

///////////////////////////////////////////////////////////////////////////////

SwapMetrics::SwapMetrics() : browser_(NULL) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
  BrowserList::AddObserver(this);
}

SwapMetrics::~SwapMetrics() {
  if (backend_.get())
    backend_->CancelOnUIThread();
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
  BrowserList::RemoveObserver(this);
  SetBrowser(NULL);
}

void SwapMetrics::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    SetBrowser(NULL);
}

void SwapMetrics::OnBrowserSetLastActive(Browser* browser) {
  if (browser && browser->type() == Browser::TYPE_TABBED)
    SetBrowser(browser);
  else
    SetBrowser(NULL);
}

void SwapMetrics::ActiveTabChanged(content::WebContents* old_contents,
                                   content::WebContents* new_contents,
                                   int index,
                                   int reason) {
  // Only measure tab switches, not tabs being replaced with new contents.
  if (reason != TabStripModelObserver::CHANGE_REASON_USER_GESTURE)
    return;
  DVLOG(1) << "ActiveTabChanged";
  StartMetricsCollection("TabSwitch");
}

// This exists primarily for debugging on desktop builds.
void SwapMetrics::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSEWHEEL)
    return;
  DVLOG(1) << "OnMouseEvent";
  StartMetricsCollection("Scroll");
}

void SwapMetrics::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() != ui::ET_SCROLL &&
      event->type() != ui::ET_SCROLL_FLING_START)
    return;
  DVLOG(1) << "OnScrollEvent";
  StartMetricsCollection("Scroll");
}

// static
void SwapMetrics::PostTaskRecordMetrics(scoped_refptr<Backend> backend,
                                        size_t time_index,
                                        int delay_ms) {
  // Don't block shutdown on these tasks, as UMA will be disappearing.
  scoped_refptr<base::TaskRunner> runner =
      content::BrowserThread::GetBlockingPool()->
          GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  runner->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SwapMetrics::Backend::RecordMetricsOnBlockingPool,
                 backend,
                 time_index),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void SwapMetrics::StartMetricsCollection(const std::string& reason) {
  // Cancel any existing metrics run.
  if (backend_.get())
    backend_->CancelOnUIThread();
  backend_ = new Backend(reason);
  PostTaskRecordMetrics(backend_, 0, kMetricsDelayMs[0]);
}

void SwapMetrics::SetBrowser(Browser* browser) {
  if (browser_ == browser)
    return;
  if (browser_)
    browser_->tab_strip_model()->RemoveObserver(this);
  browser_ = browser;
  if (browser_)
    browser_->tab_strip_model()->AddObserver(this);
}

}  // namespace chromeos
