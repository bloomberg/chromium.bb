// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/web_contents_task_provider.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/task_management/providers/web_contents/subframe_task.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::RenderFrameHost;
using content::SiteInstance;
using content::WebContents;

namespace task_management {

// Defines an entry for each WebContents that will be tracked by the provider.
// The entry is used to observe certain events in its corresponding WebContents
// and then it notifies the provider or the render task (representing the
// WebContents) of these events.
// The entry owns the created tasks representing the WebContents, and it is
// itself owned by the provider.
class WebContentsEntry : public content::WebContentsObserver {
 public:
  WebContentsEntry(content::WebContents* web_contents,
                   WebContentsTaskProvider* provider);
  ~WebContentsEntry() override;

  // Creates all the tasks associated with each |RenderFrameHost| in this
  // entry's WebContents.
  void CreateAllTasks();

  // Clears all the tasks in this entry. The provider's observer will be
  // notified if |notify_observer| is true.
  void ClearAllTasks(bool notify_observer);

  // Returns the |RendererTask| that corresponds to the given
  // |render_frame_host| or |nullptr| if the given frame is not tracked by this
  // entry.
  RendererTask* GetTaskForFrame(RenderFrameHost* render_frame_host) const;

  // content::WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void RenderViewReady() override;
  void WebContentsDestroyed() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;

 private:
  // Defines a callback for WebContents::ForEachFrame() to create a
  // corresponding task for the given |render_frame_host| and notifying the
  // provider's observer of the new task.
  void CreateTaskForFrame(RenderFrameHost* render_frame_host);

  // Clears the task that corresponds to the given |render_frame_host| and
  // notifies the provider's observer of the tasks removal.
  void ClearTaskForFrame(RenderFrameHost* render_frame_host);

  // The provider that owns this entry.
  WebContentsTaskProvider* provider_;

  // The RenderFrameHosts associated with this entry's WebContents that we're
  // tracking mapped by their SiteInstances.
  typedef std::vector<RenderFrameHost*> FramesList;
  typedef std::map<SiteInstance*, FramesList> SiteInstanceToFramesMap;
  SiteInstanceToFramesMap frames_by_site_instance_;

  // The RendererTasks that we create for the task manager, mapped by their
  // RenderFrameHosts.
  typedef std::map<RenderFrameHost*, RendererTask*> FramesToTasksMap;
  FramesToTasksMap tasks_by_frames_;

  // States whether we did record a main frame for this entry.
  SiteInstance* main_frame_site_instance_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsEntry);
};

////////////////////////////////////////////////////////////////////////////////

WebContentsEntry::WebContentsEntry(content::WebContents* web_contents,
                                   WebContentsTaskProvider* provider)
    : WebContentsObserver(web_contents),
      provider_(provider),
      frames_by_site_instance_(),
      tasks_by_frames_(),
      main_frame_site_instance_(nullptr) {
}

WebContentsEntry::~WebContentsEntry() {
  ClearAllTasks(false);
}

void WebContentsEntry::CreateAllTasks() {
  DCHECK(web_contents()->GetMainFrame());
  web_contents()->ForEachFrame(base::Bind(&WebContentsEntry::CreateTaskForFrame,
                                          base::Unretained(this)));
}

void WebContentsEntry::ClearAllTasks(bool notify_observer) {
  for (auto& pair : frames_by_site_instance_) {
    FramesList& frames_list = pair.second;
    DCHECK(!frames_list.empty());
    RendererTask* task = tasks_by_frames_[frames_list[0]];
    if (notify_observer)
      provider_->NotifyObserverTaskRemoved(task);
    delete task;
  }

  frames_by_site_instance_.clear();
  tasks_by_frames_.clear();
  main_frame_site_instance_ = nullptr;
}

RendererTask* WebContentsEntry::GetTaskForFrame(
    RenderFrameHost* render_frame_host) const {
  auto itr = tasks_by_frames_.find(render_frame_host);
  if (itr == tasks_by_frames_.end())
    return nullptr;

  return itr->second;
}

void WebContentsEntry::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  ClearTaskForFrame(render_frame_host);
}

void WebContentsEntry::RenderFrameHostChanged(RenderFrameHost* old_host,
                                              RenderFrameHost* new_host) {
  ClearTaskForFrame(old_host);
  CreateTaskForFrame(new_host);
}

void WebContentsEntry::RenderViewReady() {
  ClearAllTasks(true);
  CreateAllTasks();
}

void WebContentsEntry::WebContentsDestroyed() {
  ClearAllTasks(true);
  provider_->DeleteEntry(web_contents());
}

void WebContentsEntry::RenderProcessGone(base::TerminationStatus status) {
  ClearAllTasks(true);
}

void WebContentsEntry::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  auto itr = tasks_by_frames_.find(web_contents()->GetMainFrame());
  if (itr == tasks_by_frames_.end()) {
    // TODO(afakhry): Validate whether this actually happens in practice.
    NOTREACHED();
    return;
  }

  // Listening to WebContentsObserver::TitleWasSet() only is not enough in
  // some cases when the the webpage doesn't have a title. That's why we update
  // the title here as well.
  itr->second->UpdateTitle();

  // Call RendererTask::UpdateFavicon() to set the current favicon to the
  // default favicon. If the page has a non-default favicon,
  // RendererTask::OnFaviconUpdated() will update the current favicon once
  // FaviconDriver figures out the correct favicon for the page.
  itr->second->UpdateFavicon();
}

void WebContentsEntry::TitleWasSet(content::NavigationEntry* entry,
                                   bool explicit_set) {
  auto itr = tasks_by_frames_.find(web_contents()->GetMainFrame());
  if (itr == tasks_by_frames_.end()) {
    // TODO(afakhry): Validate whether this actually happens in practice.
    NOTREACHED();
    return;
  }

  itr->second->UpdateTitle();
}

void WebContentsEntry::CreateTaskForFrame(RenderFrameHost* render_frame_host) {
  DCHECK_EQ(0U, tasks_by_frames_.count(render_frame_host));

  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance->GetProcess()->HasConnection())
    return;

  bool site_instance_exists =
      frames_by_site_instance_.count(site_instance) != 0U;
  bool is_main_frame = (render_frame_host == web_contents()->GetMainFrame());
  bool site_instance_is_main = (site_instance == main_frame_site_instance_);

  RendererTask* new_task = nullptr;
  // We don't create a task if there's one for this site_instance AND
  // if this is not the main frame or we did record a main frame for the entry.
  if (!site_instance_exists || (is_main_frame && !site_instance_is_main)) {
    if (is_main_frame) {
      const WebContentsTag* tag =
          WebContentsTag::FromWebContents(web_contents());
      CHECK(tag);
      new_task = tag->CreateTask();
      main_frame_site_instance_ = site_instance;
    } else {
      new_task = new SubframeTask(render_frame_host, web_contents());
    }
  }

  if (site_instance_exists) {
    // One of the existing frame hosts for this site instance.
    FramesList& existing_frames_for_site_instance =
        frames_by_site_instance_[site_instance];
    RenderFrameHost* existing_rfh = existing_frames_for_site_instance[0];
    RendererTask* old_task = tasks_by_frames_[existing_rfh];

    if (!new_task) {
      // We didn't create any new task, so we keep appending the old one.
      tasks_by_frames_[render_frame_host] = old_task;
    } else {
      // Overwrite all the existing old tasks with the new one, and delete the
      // old one.
      for (RenderFrameHost* frame : existing_frames_for_site_instance)
        tasks_by_frames_[frame] = new_task;

      provider_->NotifyObserverTaskRemoved(old_task);
      delete old_task;
    }
  }

  frames_by_site_instance_[site_instance].push_back(render_frame_host);

  if (new_task) {
    tasks_by_frames_[render_frame_host] = new_task;
    provider_->NotifyObserverTaskAdded(new_task);
  }
}

void WebContentsEntry::ClearTaskForFrame(RenderFrameHost* render_frame_host) {
  auto itr = tasks_by_frames_.find(render_frame_host);
  if (itr == tasks_by_frames_.end())
    return;

  RendererTask* task = itr->second;
  tasks_by_frames_.erase(itr);
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  FramesList& frames = frames_by_site_instance_[site_instance];
  frames.erase(std::find(frames.begin(), frames.end(), render_frame_host));

  if (frames.empty()) {
    frames_by_site_instance_.erase(site_instance);
    provider_->NotifyObserverTaskRemoved(task);
    delete task;

    if (site_instance == main_frame_site_instance_)
      main_frame_site_instance_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////

WebContentsTaskProvider::WebContentsTaskProvider()
    : entries_map_(),
      is_updating_(false) {
}

WebContentsTaskProvider::~WebContentsTaskProvider() {
  if (is_updating_) {
    StopUpdating();
  }
}

void WebContentsTaskProvider::OnWebContentsTagCreated(
    const WebContentsTag* tag) {
  DCHECK(tag);
  content::WebContents* web_contents = tag->web_contents();
  DCHECK(web_contents);

  // TODO(afakhry): Check if we need this check. It seems that we no longer
  // need it in the new implementation.
  if (entries_map_.count(web_contents)) {
    // This case may happen if we added a WebContents while collecting all the
    // pre-existing ones at the time |StartUpdating()| was called, but the
    // notification of its connection hasn't been fired yet. In this case we
    // ignore it since we're already tracking it.
    return;
  }

  WebContentsEntry* entry = new WebContentsEntry(web_contents, this);
  entries_map_[web_contents] = entry;
  entry->CreateAllTasks();
}

void WebContentsTaskProvider::OnWebContentsTagRemoved(
    const WebContentsTag* tag) {
  DCHECK(tag);
  content::WebContents* web_contents = tag->web_contents();
  DCHECK(web_contents);

  auto itr = entries_map_.find(web_contents);
  DCHECK(itr != entries_map_.end());
  WebContentsEntry* entry = itr->second;

  // Must manually clear the tasks and notify the observer.
  entry->ClearAllTasks(true);
  entries_map_.erase(itr);
  delete entry;
}

Task* WebContentsTaskProvider::GetTaskOfUrlRequest(int origin_pid,
                                                   int child_id,
                                                   int route_id) {
  // If an origin PID was specified then the URL request originated in a plugin
  // working on the WebContents' behalf, so ignore it.
  if (origin_pid)
    return nullptr;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(child_id, route_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  auto itr = entries_map_.find(web_contents);
  if (itr == entries_map_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return nullptr;
  }

  return itr->second->GetTaskForFrame(rfh);
}

bool WebContentsTaskProvider::HasWebContents(
    content::WebContents* web_contents) const {
  return entries_map_.count(web_contents) != 0;
}

void WebContentsTaskProvider::StartUpdating() {
  is_updating_ = true;

  // 1- Collect all pre-existing WebContents from the WebContentsTagsManager.
  WebContentsTagsManager* tags_manager = WebContentsTagsManager::GetInstance();
  for (auto& tag : tags_manager->tracked_tags())
    OnWebContentsTagCreated(tag);

  // 2- Start observing newly connected ones.
  tags_manager->SetProvider(this);
}

void WebContentsTaskProvider::StopUpdating() {
  is_updating_ = false;

  // 1- Stop observing.
  WebContentsTagsManager::GetInstance()->ClearProvider();

  // 2- Clear storage.
  STLDeleteValues(&entries_map_);
}

void WebContentsTaskProvider::DeleteEntry(content::WebContents* web_contents) {
  auto itr = entries_map_.find(web_contents);
  if (itr == entries_map_.end()) {
    NOTREACHED();
    return;
  }

  WebContentsEntry* entry = itr->second;
  entries_map_.erase(itr);

  // The entry we're about to delete is our caller, however its' still fine to
  // delete it.
  delete entry;
}

}  // namespace task_management
