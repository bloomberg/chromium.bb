// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_process_host.h"

namespace task_management {

namespace {

// Gets the profile name associated with the browser context of the given
// |render_process_host| from the profile info cache.
base::string16 GetRendererProfileName(
    const content::RenderProcessHost* render_process_host) {
  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  DCHECK(profile);
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index =
      cache.GetIndexOfProfileWithPath(profile->GetOriginalProfile()->GetPath());
  if (index != std::string::npos)
    return cache.GetNameOfProfileAtIndex(index);

  return base::string16();
}

}  // namespace

RendererTask::RendererTask(const base::string16& title,
                           const gfx::ImageSkia* icon,
                           base::ProcessHandle handle,
                           content::RenderProcessHost* render_process_host)
    : Task(title, icon, handle),
      render_process_host_(render_process_host),
      render_process_id_(render_process_host->GetID()),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      webcache_stats_(),
      profile_name_(GetRendererProfileName(render_process_host)) {
}

RendererTask::~RendererTask() {
}

void RendererTask::Refresh(const base::TimeDelta& update_interval) {
  Task::Refresh(update_interval);

  // TODO(afakhry):
  // 1- Add code to disable this refresh if it was never requested by clients of
  //    the task manager.
  // 2- Figure out a way to refresh V8 and WebCache stats cleanly either by
  //    working with amistry, or by implementing something myself.
}

Task::Type RendererTask::GetType() const {
  return Task::RENDERER;
}

int RendererTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

base::string16 RendererTask::GetProfileName() const {
  return profile_name_;
}

int64 RendererTask::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

int64 RendererTask::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

bool RendererTask::ReportsWebCacheStats() const {
  return true;
}

blink::WebCache::ResourceTypeStats RendererTask::GetWebCacheStats() const {
  return webcache_stats_;
}

}  // namespace task_management
