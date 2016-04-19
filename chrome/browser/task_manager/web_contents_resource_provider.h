// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"

class TaskManager;

namespace content {
class RenderFrameHost;
class SiteInstance;
class WebContents;
}

namespace task_manager {

class RendererResource;
class TaskManagerWebContentsEntry;
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
                              std::unique_ptr<WebContentsInformation> info);

  // ResourceProvider implementation.
  RendererResource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) override;
  void StartUpdating() override;
  void StopUpdating() override;

  // Start observing |web_contents| for changes via WebContentsObserver, and
  // add it to the task manager.
  void OnWebContentsCreated(content::WebContents* web_contents);

  // Remove a TaskManagerWebContentsEntry from our tracking list, and delete it.
  void DeleteEntry(content::WebContents* web_contents,
                   TaskManagerWebContentsEntry* entry);

  TaskManager* task_manager() { return task_manager_; }
  WebContentsInformation* info() { return info_.get(); }

 protected:
  ~WebContentsResourceProvider() override;

 private:
  typedef std::map<content::WebContents*, TaskManagerWebContentsEntry*>
      EntryMap;

  TaskManager* task_manager_;

  // For every WebContents we maintain an entry, which holds the task manager's
  // RendererResources for that WebContents, and observes the WebContents for
  // changes.
  EntryMap entries_;

  // The WebContentsInformation that informs us when a new WebContents* is
  // created, and which serves as a RendererResource factory for our type.
  std::unique_ptr<WebContentsInformation> info_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_RESOURCE_PROVIDER_H_
