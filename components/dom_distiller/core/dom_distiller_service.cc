// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"

#include "base/guid.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/dom_distiller/core/distilled_content_store.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
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

void RunArticleAvailableCallback(
    const DomDistillerService::ArticleAvailableCallback& article_cb,
    const ArticleEntry& entry,
    const DistilledArticleProto* article_proto,
    bool distillation_succeeded) {
  article_cb.Run(distillation_succeeded);
}

}  // namespace

DomDistillerService::DomDistillerService(
    scoped_ptr<DomDistillerStoreInterface> store,
    scoped_ptr<DistillerFactory> distiller_factory,
    scoped_ptr<DistillerPageFactory> distiller_page_factory,
    scoped_ptr<DistilledPagePrefs> distilled_page_prefs)
    : store_(store.Pass()),
      content_store_(new InMemoryContentStore(kDefaultMaxNumCachedEntries)),
      distiller_factory_(distiller_factory.Pass()),
      distiller_page_factory_(distiller_page_factory.Pass()),
      distilled_page_prefs_(distilled_page_prefs.Pass()) {
}

DomDistillerService::~DomDistillerService() {
}

syncer::SyncableService* DomDistillerService::GetSyncableService() const {
  return store_->GetSyncableService();
}

scoped_ptr<DistillerPage> DomDistillerService::CreateDefaultDistillerPage(
    const gfx::Size& render_view_size) {
  return distiller_page_factory_->CreateDistillerPage(render_view_size).Pass();
}

scoped_ptr<DistillerPage>
DomDistillerService::CreateDefaultDistillerPageWithHandle(
    scoped_ptr<SourcePageHandle> handle) {
  return distiller_page_factory_->CreateDistillerPageWithHandle(handle.Pass())
      .Pass();
}

const std::string DomDistillerService::AddToList(
    const GURL& url,
    scoped_ptr<DistillerPage> distiller_page,
    const ArticleAvailableCallback& article_cb) {
  ArticleEntry entry;
  const bool is_already_added = store_->GetEntryByUrl(url, &entry);

  TaskTracker* task_tracker = nullptr;
  if (is_already_added) {
    task_tracker = GetTaskTrackerForEntry(entry);
    if (task_tracker == NULL) {
      // Entry is in the store but there is no task tracker. This could
      // happen when distillation has already completed. For now just return
      // true.
      // TODO(shashishekhar): Change this to check if article is available,
      // An article may not be available for a variety of reasons, e.g.
      // distillation failure or blobs not available locally.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(article_cb, true));
      return entry.entry_id();
    }
  } else {
    GetOrCreateTaskTrackerForUrl(url, &task_tracker);
  }

  if (!article_cb.is_null()) {
    task_tracker->AddSaveCallback(
        base::Bind(&RunArticleAvailableCallback, article_cb));
  }

  if (!is_already_added) {
    task_tracker->AddSaveCallback(base::Bind(
        &DomDistillerService::AddDistilledPageToList, base::Unretained(this)));
    task_tracker->StartDistiller(distiller_factory_.get(),
                                 distiller_page.Pass());
    task_tracker->StartBlobFetcher();
  }

  return task_tracker->GetEntryId();
}

bool DomDistillerService::HasEntry(const std::string& entry_id) {
  return store_->GetEntryById(entry_id, NULL);
}

std::string DomDistillerService::GetUrlForEntry(const std::string& entry_id) {
  ArticleEntry entry;
  if (store_->GetEntryById(entry_id, &entry)) {
    return entry.pages().Get(0).url();
  }
  return "";
}

std::vector<ArticleEntry> DomDistillerService::GetEntries() const {
  return store_->GetEntries();
}

scoped_ptr<ArticleEntry> DomDistillerService::RemoveEntry(
    const std::string& entry_id) {
  scoped_ptr<ArticleEntry> entry(new ArticleEntry);
  entry->set_entry_id(entry_id);
  TaskTracker* task_tracker = GetTaskTrackerForEntry(*entry);
  if (task_tracker != NULL) {
    task_tracker->CancelSaveCallbacks();
  }

  if (!store_->GetEntryById(entry_id, entry.get())) {
    return scoped_ptr<ArticleEntry>();
  }

  if (store_->RemoveEntry(*entry)) {
    return entry.Pass();
  }
  return scoped_ptr<ArticleEntry>();
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewEntry(
    ViewRequestDelegate* delegate,
    scoped_ptr<DistillerPage> distiller_page,
    const std::string& entry_id) {
  ArticleEntry entry;
  if (!store_->GetEntryById(entry_id, &entry)) {
    return scoped_ptr<ViewerHandle>();
  }

  TaskTracker* task_tracker = nullptr;
  bool was_created = GetOrCreateTaskTrackerForEntry(entry, &task_tracker);
  scoped_ptr<ViewerHandle> viewer_handle = task_tracker->AddViewer(delegate);
  if (was_created) {
    task_tracker->StartDistiller(distiller_factory_.get(),
                                 distiller_page.Pass());
    task_tracker->StartBlobFetcher();
  }

  return viewer_handle.Pass();
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewUrl(
    ViewRequestDelegate* delegate,
    scoped_ptr<DistillerPage> distiller_page,
    const GURL& url) {
  if (!url.is_valid()) {
    return scoped_ptr<ViewerHandle>();
  }

  TaskTracker* task_tracker = nullptr;
  bool was_created = GetOrCreateTaskTrackerForUrl(url, &task_tracker);
  scoped_ptr<ViewerHandle> viewer_handle = task_tracker->AddViewer(delegate);
  // If a distiller is already running for one URL, don't start another.
  if (was_created) {
    task_tracker->StartDistiller(distiller_factory_.get(),
                                 distiller_page.Pass());
    task_tracker->StartBlobFetcher();
  }

  return viewer_handle.Pass();
}

bool DomDistillerService::GetOrCreateTaskTrackerForUrl(
    const GURL& url,
    TaskTracker** task_tracker) {
  ArticleEntry entry;
  if (store_->GetEntryByUrl(url, &entry)) {
    return GetOrCreateTaskTrackerForEntry(entry, task_tracker);
  }

  *task_tracker = GetTaskTrackerForUrl(url);
  if (*task_tracker) {
    return false;
  }

  ArticleEntry skeleton_entry = CreateSkeletonEntryForUrl(url);
  *task_tracker = CreateTaskTracker(skeleton_entry);
  return true;
}

TaskTracker* DomDistillerService::GetTaskTrackerForUrl(const GURL& url) const {
  for (TaskList::const_iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    if ((*it)->HasUrl(url)) {
      return *it;
    }
  }
  return nullptr;
}

TaskTracker* DomDistillerService::GetTaskTrackerForEntry(
    const ArticleEntry& entry) const {
  const std::string& entry_id = entry.entry_id();
  for (TaskList::const_iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    if ((*it)->HasEntryId(entry_id)) {
      return *it;
    }
  }
  return nullptr;
}

bool DomDistillerService::GetOrCreateTaskTrackerForEntry(
    const ArticleEntry& entry,
    TaskTracker** task_tracker) {
  *task_tracker = GetTaskTrackerForEntry(entry);
  if (!*task_tracker) {
    *task_tracker = CreateTaskTracker(entry);
    return true;
  }
  return false;
}

TaskTracker* DomDistillerService::CreateTaskTracker(const ArticleEntry& entry) {
  TaskTracker::CancelCallback cancel_callback =
      base::Bind(&DomDistillerService::CancelTask, base::Unretained(this));
  TaskTracker* tracker =
      new TaskTracker(entry, cancel_callback, content_store_.get());
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

void DomDistillerService::AddDistilledPageToList(
    const ArticleEntry& entry,
    const DistilledArticleProto* article_proto,
    bool distillation_succeeded) {
  DCHECK(IsEntryValid(entry));
  if (distillation_succeeded) {
    DCHECK(article_proto);
    DCHECK_GT(article_proto->pages_size(), 0);
    store_->AddEntry(entry);
    DCHECK_EQ(article_proto->pages_size(), entry.pages_size());
  }
}

void DomDistillerService::AddObserver(DomDistillerObserver* observer) {
  DCHECK(observer);
  store_->AddObserver(observer);
}

void DomDistillerService::RemoveObserver(DomDistillerObserver* observer) {
  DCHECK(observer);
  store_->RemoveObserver(observer);
}

DistilledPagePrefs* DomDistillerService::GetDistilledPagePrefs() {
  return distilled_page_prefs_.get();
}

}  // namespace dom_distiller
