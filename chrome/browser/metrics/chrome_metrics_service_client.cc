// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/service_process/service_process_control.h"
#endif

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

metrics::SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel) {
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return metrics::SystemProfileProto::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return metrics::SystemProfileProto::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return metrics::SystemProfileProto::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return metrics::SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

// Handles asynchronous fetching of memory details.
// Will run the provided task after finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  explicit MetricsMemoryDetails(const base::Closure& callback)
      : callback_(callback) {}

  virtual void OnDetailsAvailable() OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE, callback_);
  }

 private:
  virtual ~MetricsMemoryDetails() {}

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(MetricsMemoryDetails);
};

}  // namespace

ChromeMetricsServiceClient::ChromeMetricsServiceClient()
    : service_(NULL),
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordCommandLineMetrics();
  RegisterForNotifications();
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ChromeMetricsServiceClient::SetClientID(const std::string& client_id) {
  crash_keys::SetClientID(client_id);
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return !chrome::IsOffTheRecordSessionActive();
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_util::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return AsProtobufChannel(chrome::VersionInfo::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED();
    return std::string();
  }

  std::string version = version_info.Version();
#if defined(ARCH_CPU_64_BITS)
  version += "-64";
#endif  // defined(ARCH_CPU_64_BITS)
  if (!version_info.IsOfficialBuild())
    version.append("-devel");
  return version;
}

void ChromeMetricsServiceClient::OnLogUploadComplete() {
  // Collect network stats after each UMA upload.
  network_stats_uploader_.CollectAndReportNetworkStats();
}

void ChromeMetricsServiceClient::CollectFinalMetrics(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_collect_final_metrics_step_);
  waiting_for_collect_final_metrics_step_ = true;

  base::Closure callback =
      base::Bind(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                 weak_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback));
  details->StartFetch(MemoryDetails::UPDATE_USER_METRICS);

  // Collect WebCore cache information to put into a histogram.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new ChromeViewMsg_GetCacheResourceStats());
  }
}

void ChromeMetricsServiceClient::OnMemoryDetailCollectionDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::Closure callback = base::Bind(
      &ChromeMetricsServiceClient::OnHistogramSynchronizationDone,
      weak_ptr_factory_.GetWeakPtr());

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kMaxHistogramGatheringWaitDuration);

  DCHECK_EQ(num_async_histogram_fetches_in_progress_, 0);

#if defined(OS_ANDROID)
  // Android has no service process.
  num_async_histogram_fetches_in_progress_ = 1;
#else  // OS_ANDROID
  num_async_histogram_fetches_in_progress_ = 2;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 2);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#endif  // OS_ANDROID

  // Set up the callback to task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(base::MessageLoop::current(), callback,
                                         timeout);
}

void ChromeMetricsServiceClient::OnHistogramSynchronizationDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);
  DCHECK_GT(num_async_histogram_fetches_in_progress_, 0);

  // Check if all expected requests finished.
  if (--num_async_histogram_fetches_in_progress_ > 0)
    return;

  waiting_for_collect_final_metrics_step_ = false;
  collect_final_metrics_done_callback_.Run();
}

void ChromeMetricsServiceClient::RecordCommandLineMetrics() {
  // Get stats on use of command line.
  const CommandLine* command_line(CommandLine::ForCurrentProcess());
  size_t common_commands = 0;
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineDatDirCount", 1);
  }

  if (command_line->HasSwitch(switches::kApp)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineAppModeCount", 1);
  }

  // TODO(rohitrao): Should these be logged on iOS as well?
  // http://crbug.com/375794
  size_t switch_count = command_line->GetSwitches().size();
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineFlagCount", switch_count);
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineUncommonFlagCount",
                           switch_count - common_commands);
}

void ChromeMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL:
    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_TAB_CLOSING:
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      // TODO(isherman): Remove this NULL check: http://crbug.com/375248
      if (service_)
        service_->OnApplicationNotIdle();
      break;

    default:
      NOTREACHED();
  }
}
