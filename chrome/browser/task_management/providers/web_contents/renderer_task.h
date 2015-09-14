// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_RENDERER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_RENDERER_TASK_H_

#include "chrome/browser/task_management/providers/task.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/navigation_entry.h"

class ProcessResourceUsage;

namespace content {
class RenderProcessHost;
class WebContents;
} // namespace content

namespace task_management {

// Defines an abstract base class for various types of renderer process tasks
// such as background contents, tab contents, ... etc.
class RendererTask : public Task,
                     public favicon::FaviconDriverObserver {
 public:
  RendererTask(const base::string16& title,
               const gfx::ImageSkia* icon,
               content::WebContents* web_contents,
               content::RenderProcessHost* render_process_host);
  ~RendererTask() override;

  // An abstract method that will be called when the event
  // WebContentsObserver::DidNavigateMainFrame() occurs. This gives the
  // freedom to concrete tasks to adjust the title however they need to before
  // they set it.
  virtual void UpdateTitle() = 0;

  // An abstract method that will be called when the event
  // FaviconDriverObserver::OnFaviconUpdated() occurs, so that concrete tasks
  // can update their favicons.
  virtual void UpdateFavicon() = 0;

  // task_management::Task:
  void Activate() override;
  void Refresh(const base::TimeDelta& update_interval,
               int64 refresh_flags) override;
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  base::string16 GetProfileName() const override;
  int64 GetV8MemoryAllocated() const override;
  int64 GetV8MemoryUsed() const override;
  bool ReportsWebCacheStats() const override;
  blink::WebCache::ResourceTypeStats GetWebCacheStats() const override;

  // favicon::FaviconDriverObserver:
  void OnFaviconAvailable(const gfx::Image& image) override;
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        bool icon_url_changed) override;

 protected:
  // Returns the title of the given |web_contents|.
  static base::string16 GetTitleFromWebContents(
      content::WebContents* web_contents);

  // Returns the favicon of the given |web_contents| if any, and returns
  // |nullptr| otherwise.
  static const gfx::ImageSkia* GetFaviconFromWebContents(
      content::WebContents* web_contents);

  // Prefixes the given renderer |title| with the appropriate string based on
  // whether it's an app, an extension, incognito or a background page or
  // contents.
  static const base::string16 PrefixRendererTitle(const base::string16& title,
                                                  bool is_app,
                                                  bool is_extension,
                                                  bool is_incognito,
                                                  bool is_background);

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  // The WebContents of the task this object represents.
  content::WebContents* web_contents_;

  // The render process host of the task this object represents.
  content::RenderProcessHost* render_process_host_;

  // The Mojo service wrapper that will provide us with the V8 memory usage and
  // the WebCache resource stats of the render process represented by this
  // object.
  scoped_ptr<ProcessResourceUsage> renderer_resources_sampler_;

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
