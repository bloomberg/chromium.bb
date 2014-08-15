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
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_page.h"

class GURL;

namespace syncer {
class SyncableService;
}

namespace dom_distiller {

class DistilledArticleProto;
class DistilledContentStore;
class DistillerFactory;
class DistillerPageFactory;
class DomDistillerObserver;
class DomDistillerStoreInterface;
class TaskTracker;
class ViewerHandle;
class ViewRequestDelegate;

// Service for interacting with the Dom Distiller.
// Construction, destruction, and usage of this service must happen on the same
// thread. Callbacks will be called on that same thread.
class DomDistillerServiceInterface {
 public:
  typedef base::Callback<void(bool)> ArticleAvailableCallback;
  virtual ~DomDistillerServiceInterface() {}

  virtual syncer::SyncableService* GetSyncableService() const = 0;

  // Distill the article at |url| and add the resulting entry to the DOM
  // distiller list. |article_cb| is always invoked, and the bool argument to it
  // represents whether the article is available offline.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |url|.
  virtual const std::string AddToList(
      const GURL& url,
      scoped_ptr<DistillerPage> distiller_page,
      const ArticleAvailableCallback& article_cb) = 0;

  // Returns whether an article stored has the given entry id.
  virtual bool HasEntry(const std::string& entry_id) = 0;

  // Returns the source URL given an entry ID. If the entry ID article has
  // multiple pages, this will return the URL of the first page. Returns an
  // empty string if there is no entry associated with the given entry ID.
  virtual std::string GetUrlForEntry(const std::string& entry_id) = 0;

  // Gets the full list of entries.
  virtual std::vector<ArticleEntry> GetEntries() const = 0;

  // Removes the specified entry from the dom distiller store.
  virtual scoped_ptr<ArticleEntry> RemoveEntry(const std::string& entry_id) = 0;

  // Request to view an article by entry id. Returns a null pointer if no entry
  // with |entry_id| exists. The ViewerHandle should be destroyed before the
  // ViewRequestDelegate. The request will be cancelled when the handle is
  // destroyed (or when this service is destroyed), which also ensures that
  // the |delegate| is not called after that.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |entry_id|.
  virtual scoped_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const std::string& entry_id) = 0;

  // Request to view an article by url.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |url|.
  virtual scoped_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const GURL& url) = 0;

  // Creates a default DistillerPage.
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) = 0;
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) = 0;

  virtual void AddObserver(DomDistillerObserver* observer) = 0;
  virtual void RemoveObserver(DomDistillerObserver* observer) = 0;

  // Returns the DistilledPagePrefs owned by the instance of
  // DomDistillerService.
  virtual DistilledPagePrefs* GetDistilledPagePrefs() = 0;

 protected:
  DomDistillerServiceInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DomDistillerServiceInterface);
};

// Provide a view of the article list and ways of interacting with it.
class DomDistillerService : public DomDistillerServiceInterface {
 public:
  DomDistillerService(scoped_ptr<DomDistillerStoreInterface> store,
                      scoped_ptr<DistillerFactory> distiller_factory,
                      scoped_ptr<DistillerPageFactory> distiller_page_factory,
                      scoped_ptr<DistilledPagePrefs> distilled_page_prefs);
  virtual ~DomDistillerService();

  // DomDistillerServiceInterface implementation.
  virtual syncer::SyncableService* GetSyncableService() const OVERRIDE;
  virtual const std::string AddToList(
      const GURL& url,
      scoped_ptr<DistillerPage> distiller_page,
      const ArticleAvailableCallback& article_cb) OVERRIDE;
  virtual bool HasEntry(const std::string& entry_id) OVERRIDE;
  virtual std::string GetUrlForEntry(const std::string& entry_id) OVERRIDE;
  virtual std::vector<ArticleEntry> GetEntries() const OVERRIDE;
  virtual scoped_ptr<ArticleEntry> RemoveEntry(
      const std::string& entry_id) OVERRIDE;
  virtual scoped_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const std::string& entry_id) OVERRIDE;
  virtual scoped_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const GURL& url) OVERRIDE;
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) OVERRIDE;
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) OVERRIDE;
  virtual void AddObserver(DomDistillerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DomDistillerObserver* observer) OVERRIDE;
  virtual DistilledPagePrefs* GetDistilledPagePrefs() OVERRIDE;

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
  scoped_ptr<DistilledContentStore> content_store_;
  scoped_ptr<DistillerFactory> distiller_factory_;
  scoped_ptr<DistillerPageFactory> distiller_page_factory_;
  scoped_ptr<DistilledPagePrefs> distilled_page_prefs_;

  typedef ScopedVector<TaskTracker> TaskList;
  TaskList tasks_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerService);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
