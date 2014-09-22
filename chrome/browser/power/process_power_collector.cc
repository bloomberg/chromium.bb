// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power/process_power_collector.h"

#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "components/power/origin_power_map.h"
#include "components/power/origin_power_map_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#endif

namespace {
const int kSecondsPerSample = 30;
}

ProcessPowerCollector::PerProcessData::PerProcessData(
    scoped_ptr<base::ProcessMetrics> metrics,
    const GURL& origin,
    Profile* profile)
    : metrics_(metrics.Pass()),
      profile_(profile),
      last_origin_(origin),
      last_cpu_percent_(0),
      seen_this_cycle_(true) {
}

ProcessPowerCollector::PerProcessData::PerProcessData()
    : profile_(NULL),
      last_cpu_percent_(0.0),
      seen_this_cycle_(false) {
}

ProcessPowerCollector::PerProcessData::~PerProcessData() {
}

ProcessPowerCollector::ProcessPowerCollector()
    : scale_factor_(1.0) {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
#endif
}

ProcessPowerCollector::~ProcessPowerCollector() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
#endif
}

#if defined(OS_CHROMEOS)
void ProcessPowerCollector::PowerChanged(
    const power_manager::PowerSupplyProperties& prop) {
  if (prop.battery_state() ==
      power_manager::PowerSupplyProperties::DISCHARGING) {
    if (!timer_.IsRunning())
      StartTimer();
    scale_factor_ = prop.battery_discharge_rate();
  } else {
    timer_.Stop();
  }
}
#endif

void ProcessPowerCollector::Initialize() {
  StartTimer();
}

double ProcessPowerCollector::UpdatePowerConsumptionForTesting() {
  return UpdatePowerConsumption();
}

void ProcessPowerCollector::StartTimer() {
  DCHECK(!timer_.IsRunning());
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kSecondsPerSample),
               this,
               &ProcessPowerCollector::HandleUpdateTimeout);
}

double ProcessPowerCollector::UpdatePowerConsumption() {
  double total_cpu_percent = SynchronizeProcesses();

  for (ProcessMetricsMap::iterator it = metrics_map_.begin();
       it != metrics_map_.end();
       ++it) {
    // Invalidate the process for the next cycle.
    it->second->set_seen_this_cycle(false);
  }

  RecordCpuUsageByOrigin(total_cpu_percent);
  return total_cpu_percent;
}

void ProcessPowerCollector::HandleUpdateTimeout() {
  UpdatePowerConsumption();
}

double ProcessPowerCollector::SynchronizeProcesses() {
  // Update all tabs.
  for (TabContentsIterator it; !it.done(); it.Next()) {
    content::RenderProcessHost* render_process = it->GetRenderProcessHost();
    // Skip incognito web contents.
    if (render_process->GetBrowserContext()->IsOffTheRecord())
      continue;
    UpdateProcessInMap(render_process, it->GetLastCommittedURL().GetOrigin());
  }

  // Iterate over all profiles to find all app windows to attribute all apps.
  ProfileManager* pm = g_browser_process->profile_manager();
  std::vector<Profile*> open_profiles = pm->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator it = open_profiles.begin();
       it != open_profiles.end();
       ++it) {
    const extensions::AppWindowRegistry::AppWindowList& app_windows =
        extensions::AppWindowRegistry::Get(*it)->app_windows();
    for (extensions::AppWindowRegistry::AppWindowList::const_iterator itr =
             app_windows.begin();
         itr != app_windows.end();
         ++itr) {
      content::WebContents* web_contents = (*itr)->web_contents();

      UpdateProcessInMap(web_contents->GetRenderProcessHost(),
                         web_contents->GetLastCommittedURL().GetOrigin());
    }
  }

  // Remove invalid processes and sum up the cpu cycle.
  double total_cpu_percent = 0.0;
  ProcessMetricsMap::iterator it = metrics_map_.begin();
  while (it != metrics_map_.end()) {
    if (!it->second->seen_this_cycle()) {
      metrics_map_.erase(it++);
      continue;
    }

    total_cpu_percent += it->second->last_cpu_percent();
    ++it;
  }

  return total_cpu_percent;
}

void ProcessPowerCollector::RecordCpuUsageByOrigin(double total_cpu_percent) {
  DCHECK_GE(total_cpu_percent, 0);
  if (total_cpu_percent == 0)
    return;

  for (ProcessMetricsMap::iterator it = metrics_map_.begin();
       it != metrics_map_.end();
       ++it) {
    double last_process_power_usage = it->second->last_cpu_percent();
    last_process_power_usage *= scale_factor_ / total_cpu_percent;

    GURL origin = it->second->last_origin();
    power::OriginPowerMap* origin_power_map =
        power::OriginPowerMapFactory::GetForBrowserContext(
            it->second->profile());
    // |origin_power_map| can be NULL, if the profile is a guest profile in
    // Chrome OS.
    if (!origin_power_map)
      continue;
    origin_power_map->AddPowerForOrigin(origin, last_process_power_usage);
  }

  // Iterate over all profiles to let them know we've finished updating.
  ProfileManager* pm = g_browser_process->profile_manager();
  std::vector<Profile*> open_profiles = pm->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator it = open_profiles.begin();
       it != open_profiles.end();
       ++it) {
    power::OriginPowerMap* origin_power_map =
        power::OriginPowerMapFactory::GetForBrowserContext(*it);
    if (!origin_power_map)
      continue;
    origin_power_map->OnAllOriginsUpdated();
  }
}

void ProcessPowerCollector::UpdateProcessInMap(
    const content::RenderProcessHost* rph,
    const GURL& origin) {
  base::ProcessHandle handle = rph->GetHandle();
  if (metrics_map_.find(handle) == metrics_map_.end()) {
    metrics_map_[handle] = linked_ptr<PerProcessData>(new PerProcessData(
#if defined(OS_MACOSX)
        scoped_ptr<base::ProcessMetrics>(
            base::ProcessMetrics::CreateProcessMetrics(handle, NULL)),
#else
        scoped_ptr<base::ProcessMetrics>(
            base::ProcessMetrics::CreateProcessMetrics(handle)),
#endif
        origin,
        Profile::FromBrowserContext(rph->GetBrowserContext())));
  }

  linked_ptr<PerProcessData>& process_data = metrics_map_[handle];
  process_data->set_last_cpu_percent(std::max(0.0,
      cpu_usage_callback_.is_null() ? process_data->metrics()->GetCPUUsage()
                                    : cpu_usage_callback_.Run(handle)));
  process_data->set_seen_this_cycle(true);
}
