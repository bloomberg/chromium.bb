// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_H_

#include "base/basictypes.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "ui/gfx/image/image_skia.h"

namespace task_management {

// Defines a task that corresponds to a tab, an app, an extension, ... etc. It
// represents one row in the task manager table. Multiple tasks can share the
// same process, in which case they're grouped together in the task manager
// table. See |task_management::TaskGroup| which represents a process possibly
// shared by multiple tasks.
class Task {
 public:
  enum Type {
    UNKNOWN = 0,
    BROWSER,        /* The main browser process. */
    RENDERER,       /* A normal WebContents renderer process. */
    EXTENSION,      /* An extension or app process. */
    GUEST,          /* A browser plugin guest process. */
    PLUGIN,         /* A plugin process. */
    WORKER,         /* A web worker process. */
    NACL,           /* A NativeClient loader or broker process. */
    UTILITY,        /* A browser utility process. */
    ZYGOTE,         /* A Linux zygote process. */
    SANDBOX_HELPER, /* A sandbox helper process. */
    GPU,            /* A graphics process. */
  };

  // Create a task with the given |title| and the given favicon |icon|. This
  // task runs on a process whose handle is |handle|.
  Task(const base::string16& title,
       const gfx::ImageSkia* icon,
       base::ProcessHandle handle);
  virtual ~Task();

  // Activates this TaskManager's task by bringing its container to the front
  // (if possible).
  virtual void Activate();

  // Will be called to let the task refresh itself between refresh cycles.
  // |update_interval| is the time since the last task manager refresh.
  // the |refresh_flags| indicate which resources should be calculated on each
  // refresh.
  virtual void Refresh(const base::TimeDelta& update_interval,
                       int64 refresh_flags);

  // Will receive this notification through the task manager from
  // |ChromeNetworkDelegate::OnNetworkBytesReceived()|. The task will add to the
  // |current_byte_count_| in this refresh cycle.
  void OnNetworkBytesRead(int64 bytes_read);

  // Returns the task type.
  virtual Type GetType() const = 0;

  // This is the unique ID of the BrowserChildProcessHost/RenderProcessHost. It
  // is not the PID nor the handle of the process.
  // For a task that represents the browser process, the return value is 0. For
  // other tasks that represent renderers and other child processes, the return
  // value is whatever unique IDs of their hosts in the browser process.
  virtual int GetChildProcessUniqueID() const = 0;

  // The name of the profile associated with the browser context of the render
  // view host that this task represents (if this task represents a renderer).
  virtual base::string16 GetProfileName() const;

  // Getting the Sqlite used memory (in bytes). Not all tasks reports Sqlite
  // memory, in this case a default invalid value of -1 will be returned.
  // Check for whether the task reports it or not first.
  bool ReportsSqliteMemory() const;
  virtual int64 GetSqliteMemoryUsed() const;

  // Getting the allocated and used V8 memory (in bytes). Not all tasks reports
  // V8 memory, in this case a default invalid value of -1 will be returned.
  // Check for whether the task reports it or not first.
  bool ReportsV8Memory() const;
  virtual int64 GetV8MemoryAllocated() const;
  virtual int64 GetV8MemoryUsed() const;

  // Checking if the task reports Webkit resource cache statistics and getting
  // them if it does.
  virtual bool ReportsWebCacheStats() const;
  virtual blink::WebCache::ResourceTypeStats GetWebCacheStats() const;

  // Checking whether the task reports network usage.
  bool ReportsNetworkUsage() const;

  int64 task_id() const { return task_id_; }
  int64 network_usage() const { return network_usage_; }
  const base::string16& title() const { return title_; }
  const gfx::ImageSkia& icon() const { return icon_; }
  const base::ProcessHandle& process_handle() const { return process_handle_; }
  const base::ProcessId& process_id() const { return process_id_; }

 protected:
  void set_title(const base::string16& new_title) { title_ = new_title; }
  void set_icon(const gfx::ImageSkia& new_icon) { icon_ = new_icon; }

 private:
  // The unique ID of this task.
  const int64 task_id_;

  // The task's network usage in the current refresh cycle measured in bytes per
  // second. A value of -1 means this task doesn't report network usage data.
  int64 network_usage_;

  // The current network bytes received by this task during the current refresh
  // cycle. A value of -1 means this task has never been notified of any network
  // usage.
  int64 current_byte_count_;

  // The title of the task.
  base::string16 title_;

  // The favicon.
  gfx::ImageSkia icon_;

  // The handle of the process on which this task is running.
  const base::ProcessHandle process_handle_;

  // The PID of the process on which this task is running.
  const base::ProcessId process_id_;

  DISALLOW_COPY_AND_ASSIGN(Task);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_H_
