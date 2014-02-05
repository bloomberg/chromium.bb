// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/article_entry.h"

class GURL;

namespace syncer {
class SyncableService;
}

namespace dom_distiller {

class DistilledArticleProto;
class DistillerFactory;
class DomDistillerObserver;
class DomDistillerStoreInterface;
class TaskTracker;
class ViewerHandle;
class ViewRequestDelegate;

// Provide a view of the article list and ways of interacting with it.
class DomDistillerService {
 public:
  typedef base::Callback<void(bool)> ArticleAvailableCallback;

  DomDistillerService(scoped_ptr<DomDistillerStoreInterface> store,
                      scoped_ptr<DistillerFactory> distiller_factory);
  ~DomDistillerService();

  syncer::SyncableService* GetSyncableService() const;

  // Distill the article at |url| and add the resulting entry to the DOM
  // distiller list. |article_cb| is invoked with true if article is
  // available offline.
  const std::string AddToList(const GURL& url,
                              const ArticleAvailableCallback& article_cb);

  // Gets the full list of entries.
  std::vector<ArticleEntry> GetEntries() const;

  // Removes the specified entry from the dom distiller store.
  scoped_ptr<ArticleEntry> RemoveEntry(const std::string& entry_id);

  // Request to view an article by entry id. Returns a null pointer if no entry
  // with |entry_id| exists. The ViewerHandle should be destroyed before the
  // ViewRequestDelegate. The request will be cancelled when the handle is
  // destroyed (or when this service is destroyed).
  scoped_ptr<ViewerHandle> ViewEntry(ViewRequestDelegate* delegate,
                                     const std::string& entry_id);

  // Request to view an article by url.
  scoped_ptr<ViewerHandle> ViewUrl(ViewRequestDelegate* delegate,
                                   const GURL& url);

  void AddObserver(DomDistillerObserver* observer);
  void RemoveObserver(DomDistillerObserver* observer);

 private:
  void CancelTask(TaskTracker* task);
  void AddDistilledPageToList(const ArticleEntry& entry,
                              const DistilledArticleProto* article_proto,
                              bool distillation_succeeded);

  TaskTracker* CreateTaskTracker(const ArticleEntry& entry);

  TaskTracker* GetTaskTrackerForEntry(const ArticleEntry& entry) const;

  // Gets the task tracker for the given |url| or |entry|. If no appropriate
  // tracker exists, this will create one, initialize it, and add it to
  // |tasks_|.
  TaskTracker* GetOrCreateTaskTrackerForUrl(const GURL& url);
  TaskTracker* GetOrCreateTaskTrackerForEntry(const ArticleEntry& entry);

  scoped_ptr<DomDistillerStoreInterface> store_;
  scoped_ptr<DistillerFactory> distiller_factory_;

  typedef ScopedVector<TaskTracker> TaskList;
  TaskList tasks_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerService);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
