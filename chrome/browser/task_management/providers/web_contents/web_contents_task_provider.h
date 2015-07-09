// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_

#include <map>

#include "chrome/browser/task_management/providers/task_provider.h"

namespace content {
class WebContents;
}  // namespace content


namespace task_management {

class WebContentsEntry;
class WebContentsTag;

// Defines a provider to provide the renderer tasks that are associated with
// various |WebContents| from various services.
// There should be no or only one instance of this class at any time.
class WebContentsTaskProvider : public TaskProvider {
 public:
  WebContentsTaskProvider();
  ~WebContentsTaskProvider() override;

  // This will be called every time we're notified that a new |WebContentsTag|
  // has been created.
  void OnWebContentsTagCreated(const WebContentsTag* tag);

  // Manually remove |tag|'s corresponding Task.
  void OnWebContentsTagRemoved(const WebContentsTag* tag);

  // task_management::TaskProvider:
  Task* GetTaskOfUrlRequest(int origin_pid,
                            int child_id,
                            int route_id) override;

  // Checks if the given |web_contents| is tracked by the provider.
  bool HasWebContents(content::WebContents* web_contents) const;

 private:
  friend class WebContentsEntry;

  // task_management::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Called when the given |web_contents| are destroyed so that we can delete
  // its associated entry.
  void DeleteEntry(content::WebContents* web_contents);

  // A map to associate a |WebContents| with its corresponding entry that we
  // create for it to be able to track it.
  typedef std::map<content::WebContents*, WebContentsEntry*> EntryMap;
  EntryMap entries_map_;

  // True if this provider is listening to WebContentsTags and updating its
  // observers, false otherwise.
  bool is_updating_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsTaskProvider);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_
