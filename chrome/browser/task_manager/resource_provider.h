// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/web/WebCache.h"

class PrefRegistrySimple;
class TaskManagerModel;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class ImageSkia;
}

namespace task_manager {

#define TASKMANAGER_RESOURCE_TYPE_LIST(def) \
    def(BROWSER)         /* The main browser process. */ \
    def(RENDERER)        /* A normal WebContents renderer process. */ \
    def(EXTENSION)       /* An extension or app process. */ \
    def(NOTIFICATION)    /* A notification process. */ \
    def(GUEST)           /* A browser plugin guest process. */ \
    def(PLUGIN)          /* A plugin process. */ \
    def(WORKER)          /* A web worker process. */ \
    def(NACL)            /* A NativeClient loader or broker process. */ \
    def(UTILITY)         /* A browser utility process. */ \
    def(ZYGOTE)          /* A Linux zygote process. */ \
    def(SANDBOX_HELPER)  /* A sandbox helper process. */ \
    def(GPU)             /* A graphics process. */

#define TASKMANAGER_RESOURCE_TYPE_LIST_ENUM(a)   a,
#define TASKMANAGER_RESOURCE_TYPE_LIST_AS_STRING(a)   case a: return #a;

// A resource represents one row in the task manager.
// Resources from similar processes are grouped together by the task manager.
class Resource {
 public:
  virtual ~Resource() {}

  enum Type {
    UNKNOWN = 0,
    TASKMANAGER_RESOURCE_TYPE_LIST(TASKMANAGER_RESOURCE_TYPE_LIST_ENUM)
  };

  virtual base::string16 GetTitle() const = 0;
  virtual base::string16 GetProfileName() const = 0;
  virtual gfx::ImageSkia GetIcon() const = 0;
  virtual base::ProcessHandle GetProcess() const = 0;
  virtual int GetUniqueChildProcessId() const = 0;
  virtual Type GetType() const = 0;
  virtual int GetRoutingID() const;

  virtual bool ReportsCacheStats() const;
  virtual blink::WebCache::ResourceTypeStats GetWebCoreCacheStats() const;

  virtual bool ReportsSqliteMemoryUsed() const;
  virtual size_t SqliteMemoryUsedBytes() const;

  virtual bool ReportsV8MemoryStats() const;
  virtual size_t GetV8MemoryAllocated() const;
  virtual size_t GetV8MemoryUsed() const;

  // A helper function for ActivateProcess when selected resource refers
  // to a Tab or other window containing web contents.  Returns NULL by
  // default because not all resources have an associated web contents.
  virtual content::WebContents* GetWebContents() const;

  // Whether this resource does report the network usage accurately.
  // This controls whether 0 or N/A is displayed when no bytes have been
  // reported as being read. This is because some plugins do not report the
  // bytes read and we don't want to display a misleading 0 value in that
  // case.
  virtual bool SupportNetworkUsage() const = 0;

  // Called when some bytes have been read and support_network_usage returns
  // false (meaning we do have network usage support).
  virtual void SetSupportNetworkUsage() = 0;

  // The TaskManagerModel periodically refreshes its data and call this
  // on all live resources.
  virtual void Refresh() {}

  virtual void NotifyResourceTypeStats(
      const blink::WebCache::ResourceTypeStats& stats) {}
  virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                 size_t v8_memory_used) {}

  static const char* GetResourceTypeAsString(const Type type) {
    switch (type) {
      TASKMANAGER_RESOURCE_TYPE_LIST(TASKMANAGER_RESOURCE_TYPE_LIST_AS_STRING)
      default: return "UNKNOWN";
    }
  }

 protected:
  Resource() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Resource);
};

#undef TASKMANAGER_RESOURCE_TYPE_LIST
#undef TASKMANAGER_RESOURCE_TYPE_LIST_ENUM
#undef TASKMANAGER_RESOURCE_TYPE_LIST_AS_STRING

// ResourceProviders are responsible for adding/removing resources to the task
// manager. The task manager notifies the ResourceProvider that it is ready
// to receive resource creation/termination notifications with a call to
// StartUpdating(). At that point, the resource provider should call
// AddResource with all the existing resources, and after that it should call
// AddResource/RemoveResource as resources are created/terminated.
// The provider remains the owner of the resource objects and is responsible
// for deleting them (when StopUpdating() is called).
// After StopUpdating() is called the provider should also stop reporting
// notifications to the task manager.
// Note: ResourceProviders have to be ref counted as they are used in
// MessageLoop::InvokeLater().
class ResourceProvider : public base::RefCountedThreadSafe<ResourceProvider> {
 public:
  // Should return the resource associated to the specified ids, or NULL if
  // the resource does not belong to this provider.
  virtual Resource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) = 0;
  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ResourceProvider>;

  virtual ~ResourceProvider() {}
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDER_H_
