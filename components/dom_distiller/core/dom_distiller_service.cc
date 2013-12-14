// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"

#include "base/guid.h"
#include "base/message_loop/message_loop.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

ArticleEntry CreateSkeletonEntryForUrl(const GURL& url) {
  ArticleEntry skeleton;
  skeleton.set_entry_id(base::GenerateGUID());
  ArticleEntryPage* page = skeleton.add_pages();
  page->set_url(url.spec());

  DCHECK(IsEntryValid(skeleton));
  return skeleton;
}

}  // namespace

DomDistillerService::DomDistillerService(
    scoped_ptr<DomDistillerStoreInterface> store,
    scoped_ptr<DistillerFactory> distiller_factory)
    : store_(store.Pass()), distiller_factory_(distiller_factory.Pass()) {}

DomDistillerService::~DomDistillerService() {}

syncer::SyncableService* DomDistillerService::GetSyncableService() const {
  return store_->GetSyncableService();
}

void DomDistillerService::AddToList(const GURL& url) {
  if (store_->GetEntryByUrl(url, NULL)) {
    return;
  }
  TaskTracker* task_tracker = GetTaskTrackerForUrl(url);
  task_tracker->SetSaveCallback(base::Bind(
      &DomDistillerService::AddDistilledPageToList, base::Unretained(this)));
  task_tracker->StartDistiller(distiller_factory_.get());
}

std::vector<ArticleEntry> DomDistillerService::GetEntries() const {
  return store_->GetEntries();
}

void DomDistillerService::RemoveEntry(
    const std::string& entry_id) {
  ArticleEntry entry;
  if (!store_->GetEntryById(entry_id, &entry)) {
    return;
  }
  store_->RemoveEntry(entry);
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewEntry(
    ViewRequestDelegate* delegate,
    const std::string& entry_id) {
  ArticleEntry entry;
  if (!store_->GetEntryById(entry_id, &entry)) {
    return scoped_ptr<ViewerHandle>();
  }

  TaskTracker* task_tracker = GetTaskTrackerForEntry(entry);
  scoped_ptr<ViewerHandle> viewer_handle = task_tracker->AddViewer(delegate);
  task_tracker->StartDistiller(distiller_factory_.get());

  return viewer_handle.Pass();
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewUrl(
    ViewRequestDelegate* delegate,
    const GURL& url) {
  if (!url.is_valid()) {
    return scoped_ptr<ViewerHandle>();
  }

  TaskTracker* task_tracker = GetTaskTrackerForUrl(url);
  scoped_ptr<ViewerHandle> viewer_handle = task_tracker->AddViewer(delegate);
  task_tracker->StartDistiller(distiller_factory_.get());

  return viewer_handle.Pass();
}

TaskTracker* DomDistillerService::GetTaskTrackerForUrl(const GURL& url) {
  ArticleEntry entry;
  if (store_->GetEntryByUrl(url, &entry)) {
    return GetTaskTrackerForEntry(entry);
  }

  for (TaskList::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    if ((*it)->HasUrl(url)) {
      return *it;
    }
  }

  ArticleEntry skeleton_entry = CreateSkeletonEntryForUrl(url);
  TaskTracker* task_tracker = CreateTaskTracker(skeleton_entry);
  store_->AddEntry(skeleton_entry);
  return task_tracker;
}

TaskTracker* DomDistillerService::GetTaskTrackerForEntry(
    const ArticleEntry& entry) {
  const std::string& entry_id = entry.entry_id();
  for (TaskList::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    if ((*it)->HasEntryId(entry_id)) {
      return *it;
    }
  }

  TaskTracker* task_tracker = CreateTaskTracker(entry);

  return task_tracker;
}

TaskTracker* DomDistillerService::CreateTaskTracker(const ArticleEntry& entry) {
  TaskTracker::CancelCallback cancel_callback =
      base::Bind(&DomDistillerService::CancelTask, base::Unretained(this));
  TaskTracker* tracker = new TaskTracker(entry, cancel_callback);
  tasks_.push_back(tracker);
  return tracker;
}

void DomDistillerService::CancelTask(TaskTracker* task) {
  TaskList::iterator it = std::find(tasks_.begin(), tasks_.end(), task);
  if (it != tasks_.end()) {
    tasks_.weak_erase(it);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, task);
  }
}

void DomDistillerService::AddDistilledPageToList(const ArticleEntry& entry,
                                                 DistilledPageProto* proto) {
  DCHECK(IsEntryValid(entry));
  DCHECK(proto);

  store_->UpdateEntry(entry);
}

void DomDistillerService::AddObserver(DomDistillerObserver* observer) {
  DCHECK(observer);
  store_->AddObserver(observer);
}

void DomDistillerService::RemoveObserver(DomDistillerObserver* observer) {
  DCHECK(observer);
  store_->RemoveObserver(observer);
}

}  // namespace dom_distiller
