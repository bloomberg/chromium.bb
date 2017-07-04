// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"

#if defined(OS_ANDROID)
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#else
#include "chrome/browser/chrome_browser_field_trials_desktop.h"
#endif

namespace {

// Creating a "spare" file for persistent metrics involves a lot of I/O and
// isn't important so delay the operation for a while after startup.
#if defined(OS_ANDROID)
// Android needs the spare file and also launches faster.
constexpr bool kSpareFileRequired = true;
constexpr int kSpareFileCreateDelaySeconds = 10;
#else
// Desktop may have to restore a lot of tabs so give it more time before doing
// non-essential work. The spare file is still a performance boost but not as
// significant of one so it's not required.
constexpr bool kSpareFileRequired = false;
constexpr int kSpareFileCreateDelaySeconds = 90;
#endif

// Check for feature enabling the use of persistent histogram storage and
// enable the global allocator if so.
// TODO(bcwhite): Move this and CreateInstallerFileMetricsProvider into a new
// file and make kBrowserMetricsName local to that file.
void InstantiatePersistentHistograms() {
  base::FilePath metrics_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &metrics_dir))
    return;

  // Remove any existing file from its legacy location.
  // TODO(bcwhite): Remove this block of code in M62 or later.
  base::FilePath legacy_file;
  base::GlobalHistogramAllocator::ConstructFilePaths(
      metrics_dir, ChromeMetricsServiceClient::kBrowserMetricsName,
      &legacy_file, nullptr, nullptr);
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     base::Passed(&legacy_file), /*recursive=*/false));

  // Create a directory for storing completed metrics files. Files in this
  // directory must have embedded system profiles. If the directory can't be
  // created, the file will just be deleted below.
  base::FilePath upload_dir =
      metrics_dir.AppendASCII(ChromeMetricsServiceClient::kBrowserMetricsName);
  base::CreateDirectory(upload_dir);

  // Metrics files are typically created as a |spare_file| in the profile
  // directory (e.g. "BrowserMetrics-spare.pma") and are then rotated into
  // the |active_file| (e.g. "BrowserMetrics-active.pma") location for use
  // during the browser run. It is then moved to a time-stamped file in a
  // subdirectory (e.g. "BrowserMetrics/BrowserMetrics-1234ABCD.pma") for
  // upload when convenient.
  base::FilePath upload_file;
  base::FilePath active_file;
  base::FilePath spare_file;
  base::GlobalHistogramAllocator::ConstructFilePathsForUploadDir(
      metrics_dir, upload_dir, ChromeMetricsServiceClient::kBrowserMetricsName,
      &upload_file, &active_file, &spare_file);

  // Move any existing "active" file to the final name from which it will be
  // read when reporting initial stability metrics. If there is no file to
  // move, remove any old, existing file from before the previous session.
  if (!base::ReplaceFile(active_file, upload_file, nullptr))
    base::DeleteFile(active_file, /*recursive=*/false);

  // This is used to report results to an UMA histogram.
  enum InitResult {
    LOCAL_MEMORY_SUCCESS,
    LOCAL_MEMORY_FAILED,
    MAPPED_FILE_SUCCESS,
    MAPPED_FILE_FAILED,
    MAPPED_FILE_EXISTS,
    NO_SPARE_FILE,
    INIT_RESULT_MAX
  };
  InitResult result;

  // Create persistent/shared memory and allow histograms to be stored in
  // it. Memory that is not actualy used won't be physically mapped by the
  // system. BrowserMetrics usage, as reported in UMA, has the 99.9 percentile
  // around 4MiB as of 2017-02-16.
  const size_t kAllocSize = 8 << 20;     // 8 MiB
  const uint32_t kAllocId = 0x935DDD43;  // SHA1(BrowserMetrics)
  std::string storage = variations::GetVariationParamValueByFeature(
      base::kPersistentHistogramsFeature, "storage");

  if (storage.empty() || storage == "MappedFile") {
    // If for some reason the existing "active" file could not be moved above
    // then it is essential it be scheduled for deletion when possible and the
    // contents ignored. Because this shouldn't happen but can on an OS like
    // Windows where another process reading the file (backup, AV, etc.) can
    // prevent its alteration, it's necessary to handle this case by switching
    // to the equivalent of "LocalMemory" for this run.
    if (base::PathExists(active_file)) {
      base::File file(active_file, base::File::FLAG_OPEN |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_DELETE_ON_CLOSE);
      result = MAPPED_FILE_EXISTS;
      base::GlobalHistogramAllocator::CreateWithLocalMemory(
          kAllocSize, kAllocId,
          ChromeMetricsServiceClient::kBrowserMetricsName);
    } else {
      // Move any sparse file into the active position.
      base::ReplaceFile(spare_file, active_file, nullptr);
      // Create global allocator using the "active" file.
      if (kSpareFileRequired && !base::PathExists(active_file)) {
        result = NO_SPARE_FILE;
        base::GlobalHistogramAllocator::CreateWithLocalMemory(
            kAllocSize, kAllocId,
            ChromeMetricsServiceClient::kBrowserMetricsName);
      } else if (base::GlobalHistogramAllocator::CreateWithFile(
                     active_file, kAllocSize, kAllocId,
                     ChromeMetricsServiceClient::kBrowserMetricsName)) {
        result = MAPPED_FILE_SUCCESS;
      } else {
        result = MAPPED_FILE_FAILED;
      }
    }
    // Schedule the creation of a "spare" file for use on the next run.
    base::PostDelayedTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::LOWEST,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(
                           &base::GlobalHistogramAllocator::CreateSpareFile),
                       base::Passed(&spare_file), kAllocSize),
        base::TimeDelta::FromSeconds(kSpareFileCreateDelaySeconds));
  } else if (storage == "LocalMemory") {
    // Use local memory for storage even though it will not persist across
    // an unclean shutdown.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        kAllocSize, kAllocId, ChromeMetricsServiceClient::kBrowserMetricsName);
    result = LOCAL_MEMORY_SUCCESS;
  } else {
    // Persistent metric storage is disabled.
    return;
  }

  // Get the allocator that was just created and report result. Exit if the
  // allocator could not be created.
  UMA_HISTOGRAM_ENUMERATION("UMA.PersistentHistograms.InitResult", result,
                            INIT_RESULT_MAX);

  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator)
    return;

  // Store a copy of the system profile in this allocator.
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->RegisterPersistentAllocator(allocator->memory_allocator());

  // Create tracking histograms for the allocator and record storage file.
  allocator->CreateTrackingHistograms(
      ChromeMetricsServiceClient::kBrowserMetricsName);
}

// Create a field trial to control metrics/crash sampling for Stable on
// Windows/Android if no variations seed was applied.
void CreateFallbackSamplingTrialIfNeeded(bool has_seed,
                                         base::FeatureList* feature_list) {
#if defined(OS_WIN) || defined(OS_ANDROID)
  // Only create the fallback trial if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relavent study, or is intentionally omitted, so no fallback is
  // needed.
  if (has_seed)
    return;

  ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
      chrome::GetChannel(), feature_list);
#endif  // defined(OS_WIN) || defined(OS_ANDROID)
}

}  // namespace

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials() {}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials() {
  // Field trials that are shared by all platforms.
  InstantiateDynamicTrials();

#if defined(OS_ANDROID)
  chrome::SetupMobileFieldTrials();
#else
  chrome::SetupDesktopFieldTrials();
#endif
}

void ChromeBrowserFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    base::FeatureList* feature_list) {
  CreateFallbackSamplingTrialIfNeeded(has_seed, feature_list);
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Persistent histograms must be enabled as soon as possible.
  InstantiatePersistentHistograms();
  tracing::SetupBackgroundTracingFieldTrial();
}
