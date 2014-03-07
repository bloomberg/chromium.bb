// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/task_tracker.h"

#include "base/message_loop/message_loop.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace dom_distiller {

ViewerHandle::ViewerHandle(CancelCallback callback)
    : cancel_callback_(callback) {}

ViewerHandle::~ViewerHandle() {
  if (!cancel_callback_.is_null()) {
    cancel_callback_.Run();
  }
}

TaskTracker::TaskTracker(const ArticleEntry& entry, CancelCallback callback)
    : cancel_callback_(callback),
      entry_(entry),
      distilled_article_(),
      distillation_complete_(false),
      weak_ptr_factory_(this) {}

TaskTracker::~TaskTracker() { DCHECK(viewers_.empty()); }

void TaskTracker::StartDistiller(DistillerFactory* factory) {
  if (distiller_) {
    return;
  }
  if (entry_.pages_size() == 0) {
    return;
  }

  GURL url(entry_.pages(0).url());
  DCHECK(url.is_valid());

  distiller_ = factory->CreateDistiller();
  distiller_->DistillPage(url,
                          base::Bind(&TaskTracker::OnDistilledArticleReady,
                                     weak_ptr_factory_.GetWeakPtr()),
                          base::Bind(&TaskTracker::OnArticleDistillationUpdated,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void TaskTracker::StartBlobFetcher() {
  // TODO(cjhopman): There needs to be some local storage for the distilled
  // blob. When that happens, this should start some task to fetch the blob for
  // |entry_| and asynchronously notify |this| when it is done.
}

void TaskTracker::AddSaveCallback(const SaveCallback& callback) {
  DCHECK(!callback.is_null());
  save_callbacks_.push_back(callback);
  if (distillation_complete_) {
    // Distillation for this task has already completed, and so it can be
    // immediately saved.
    ScheduleSaveCallbacks(true);
  }
}

scoped_ptr<ViewerHandle> TaskTracker::AddViewer(ViewRequestDelegate* delegate) {
  viewers_.push_back(delegate);
  if (distillation_complete_) {
    // Distillation for this task has already completed, and so the delegate can
    // be immediately told of the result.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TaskTracker::NotifyViewer,
                   weak_ptr_factory_.GetWeakPtr(),
                   delegate));
  }
  return scoped_ptr<ViewerHandle>(new ViewerHandle(base::Bind(
      &TaskTracker::RemoveViewer, weak_ptr_factory_.GetWeakPtr(), delegate)));
}

const std::string& TaskTracker::GetEntryId() const { return entry_.entry_id(); }

bool TaskTracker::HasEntryId(const std::string& entry_id) const {
  return entry_.entry_id() == entry_id;
}

bool TaskTracker::HasUrl(const GURL& url) const {
  for (int i = 0; i < entry_.pages_size(); ++i) {
    if (entry_.pages(i).url() == url.spec()) {
      return true;
    }
  }
  return false;
}

void TaskTracker::RemoveViewer(ViewRequestDelegate* delegate) {
  viewers_.erase(std::remove(viewers_.begin(), viewers_.end(), delegate));
  if (viewers_.empty()) {
    MaybeCancel();
  }
}

void TaskTracker::MaybeCancel() {
  if (!save_callbacks_.empty() || !viewers_.empty()) {
    // There's still work to be done.
    return;
  }

  // The cancel callback should not delete this. To ensure that it doesn't, grab
  // a weak pointer and check that it has not been invalidated.
  base::WeakPtr<TaskTracker> self(weak_ptr_factory_.GetWeakPtr());
  cancel_callback_.Run(this);
  DCHECK(self);
}

void TaskTracker::CancelSaveCallbacks() { ScheduleSaveCallbacks(false); }

void TaskTracker::ScheduleSaveCallbacks(bool distillation_succeeded) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TaskTracker::DoSaveCallbacks,
                 weak_ptr_factory_.GetWeakPtr(),
                 distillation_succeeded));
}

void TaskTracker::DoSaveCallbacks(bool distillation_succeeded) {
  if (!save_callbacks_.empty()) {
    for (size_t i = 0; i < save_callbacks_.size(); ++i) {
      DCHECK(!save_callbacks_[i].is_null());
      save_callbacks_[i].Run(
          entry_, distilled_article_.get(), distillation_succeeded);
    }

    save_callbacks_.clear();
    MaybeCancel();
  }
}

void TaskTracker::NotifyViewer(ViewRequestDelegate* delegate) {
  DCHECK(distillation_complete_);
  delegate->OnArticleReady(distilled_article_.get());
}

void TaskTracker::OnDistilledArticleReady(
    scoped_ptr<DistilledArticleProto> distilled_article) {
  distilled_article_ = distilled_article.Pass();
  bool distillation_successful = false;
  if (distilled_article_->pages_size() > 0) {
    distillation_successful = true;
    entry_.set_title(distilled_article_->title());
    // Reset the pages.
    entry_.clear_pages();
    for (int i = 0; i < distilled_article_->pages_size(); ++i) {
      sync_pb::ArticlePage* page = entry_.add_pages();
      page->set_url(distilled_article_->pages(i).url());
    }
  }

  distillation_complete_ = true;

  for (size_t i = 0; i < viewers_.size(); ++i) {
    NotifyViewer(viewers_[i]);
  }

  // Already inside a callback run SaveCallbacks directly.
  DoSaveCallbacks(distillation_successful);
}

void TaskTracker::OnArticleDistillationUpdated(
    const ArticleDistillationUpdate& article_update) {
  for (size_t i = 0; i < viewers_.size(); ++i) {
    viewers_[i]->OnArticleUpdated(article_update);
  }
}

}  // namespace dom_distiller
