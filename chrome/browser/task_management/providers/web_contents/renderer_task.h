// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_RENDERER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_RENDERER_TASK_H_

#include "chrome/browser/task_management/providers/task.h"
#include "content/public/browser/navigation_entry.h"

namespace content {
class RenderProcessHost;
} // namespace content

namespace task_management {

// Defines an abstract base class for various types of renderer process tasks
// such as background contents, tab contents, ... etc.
class RendererTask : public Task {
 public:
  RendererTask(const base::string16& title,
               const gfx::ImageSkia* icon,
               base::ProcessHandle handle,
               content::RenderProcessHost* render_process_host);
  ~RendererTask() override;

  // An abstract method that will be called when the event
  // |WebContentsObserver::TitleWasSet()| occurs. This gives the freedom to
  // concrete tasks to adjust the title however they need to before they set it.
  virtual void OnTitleChanged(content::NavigationEntry* entry) = 0;

  // An abstract method that will be called when the event
  // |WebContentsObserver::DidUpdateFaviconURL()| occurs, so that concrete tasks
  // can update their favicons.
  virtual void OnFaviconChanged() = 0;

  // task_management::Task:
  void Refresh(const base::TimeDelta& update_interval) override;
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  base::string16 GetProfileName() const override;
  int64 GetV8MemoryAllocated() const override;
  int64 GetV8MemoryUsed() const override;
  bool ReportsWebCacheStats() const override;
  blink::WebCache::ResourceTypeStats GetWebCacheStats() const override;

 private:
  // The render process host of the task this object represents.
  content::RenderProcessHost* render_process_host_;

  // The unique ID of the RenderProcessHost.
  const int render_process_id_;

  // The allocated and used V8 memory (in bytes).
  int64 v8_memory_allocated_;
  int64 v8_memory_used_;

  // The WebKit resource cache statistics for this renderer.
  blink::WebCache::ResourceTypeStats webcache_stats_;

  // The profile name associated with the browser context of the render view
  // host.
  const base::string16 profile_name_;

  DISALLOW_COPY_AND_ASSIGN(RendererTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_RENDERER_TASK_H_
