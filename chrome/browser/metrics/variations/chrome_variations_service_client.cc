// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "components/variations/experiment_labels.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/upgrade_detector_impl.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace {

// Gets the version number to use for variations seed simulation. Must be called
// on a thread where IO is allowed.
base::Version GetVersionForSimulation() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  const base::Version installed_version =
      UpgradeDetectorImpl::GetCurrentlyInstalledVersion();
  if (installed_version.IsValid())
    return installed_version;
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  // TODO(asvitkine): Get the version that will be used on restart instead of
  // the current version on Android, iOS and ChromeOS.
  return base::Version(version_info::GetVersionNumber());
}

#if defined(OS_WIN)
// Clear all Variations experiment labels from Google Update Registry Labels.
// TODO(jwd): Remove this once we're confident most clients no longer have these
// labels (M57-M58 timeframe).
void ClearGoogleUpdateRegistryLabels() {
  base::ThreadRestrictions::AssertIOAllowed();

  // Note that all registry operations are done here on the UI thread as there
  // are no threading restrictions on them.
  const bool is_system_install = !InstallUtil::IsPerUserInstall();

  // Read the current bits from the registry.
  base::string16 registry_labels;
  bool success = GoogleUpdateSettings::ReadExperimentLabels(is_system_install,
                                                            &registry_labels);

  if (!success) {
    DVLOG(1) << "Error reading Variation labels from the registry.";
    return;
  }

  // Only keep the non-Variations contents of experiment_labels.
  const base::string16 labels_to_keep =
      variations::ExtractNonVariationLabels(registry_labels);

  // This is a weak check, which can give false positives if the implementation
  // of variations::ExtractNonVariationLabels changes, but should be fine for
  // temporary code.
  bool needs_clearing = labels_to_keep != registry_labels;

  UMA_HISTOGRAM_BOOLEAN("Variations.GoogleUpdateRegistryLabelsNeedClearing",
                        needs_clearing);

  if (!needs_clearing)
    return;

  GoogleUpdateSettings::SetExperimentLabels(is_system_install, labels_to_keep);
}
#endif  // defined(OS_WIN)

}  // namespace

ChromeVariationsServiceClient::ChromeVariationsServiceClient() {}

ChromeVariationsServiceClient::~ChromeVariationsServiceClient() {}

std::string ChromeVariationsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

base::SequencedWorkerPool* ChromeVariationsServiceClient::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

base::Callback<base::Version(void)>
ChromeVariationsServiceClient::GetVersionForSimulationCallback() {
  return base::Bind(&GetVersionForSimulation);
}

net::URLRequestContextGetter*
ChromeVariationsServiceClient::GetURLRequestContext() {
  return g_browser_process->system_request_context();
}

network_time::NetworkTimeTracker*
ChromeVariationsServiceClient::GetNetworkTimeTracker() {
  return g_browser_process->network_time_tracker();
}

version_info::Channel ChromeVariationsServiceClient::GetChannel() {
  return chrome::GetChannel();
}

bool ChromeVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetString(
      chromeos::kVariationsRestrictParameter, parameter);
  return true;
#else
  return false;
#endif
}

void ChromeVariationsServiceClient::OnInitialStartup() {
#if defined(OS_WIN)
  // TODO(jwd): Remove this once we're confident most clients no longer have
  // these labels (M57-M58 timeframe).
  // Do the work on a blocking pool thread, as chrome://profiler has shown that
  // it can cause jank if done on the UI thrread.
  content::BrowserThread::GetBlockingPool()->PostDelayedTask(
      FROM_HERE, base::Bind(&ClearGoogleUpdateRegistryLabels),
      base::TimeDelta::FromSeconds(5));
#endif
}
