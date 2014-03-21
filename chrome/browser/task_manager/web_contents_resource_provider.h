// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"

class TaskManager;

namespace content {
class WebContents;
}

namespace task_manager {

class RendererResource;
class TaskManagerWebContentsObserver;
class WebContentsInformation;

// Provides resources to the task manager on behalf of a chrome service that
// owns WebContentses. The chrome service is parameterized as a
// WebContentsInformation, which provides a list of WebContentses to track.
//
// This ResourceProvider is instantiated several times by the task manager, each
// with a different implementation of WebContentsInformation.
class WebContentsResourceProvider : public ResourceProvider {
 public:
  WebContentsResourceProvider(TaskManager* task_manager,
                              scoped_ptr<WebContentsInformation> info);

  // ResourceProvider implementation.
  virtual RendererResource* GetResource(int origin_pid,
                                        int child_id,
                                        int route_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // Start observing |web_contents| for changes via WebContentsObserver, and
  // add it to the task manager.
  void OnWebContentsCreated(content::WebContents* web_contents);

  // Create TaskManager resources for |web_contents|, and add them to the
  // TaskManager.
  bool AddToTaskManager(content::WebContents* web_contents);

  // Remove the task manager resources associated with |web_contents|.
  void RemoveFromTaskManager(content::WebContents* web_contents);

  // Remove a WebContentsObserver from our tracking list, and delete it.
  void DeleteObserver(TaskManagerWebContentsObserver* observer);

 protected:
  virtual ~WebContentsResourceProvider();

 private:
  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the WebContentses) to the TaskManager
  // resources. The RendererResources are owned by us and registered with
  // the TaskManager.
  std::map<content::WebContents*, RendererResource*> resources_;

  // Set of current active WebContentsObserver instances owned by this class.
  std::set<TaskManagerWebContentsObserver*> web_contents_observers_;

  // The WebContentsInformation that informs us when a new WebContents* is
  // created, and which serves as a RendererResource factory for our type.
  scoped_ptr<WebContentsInformation> info_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_
