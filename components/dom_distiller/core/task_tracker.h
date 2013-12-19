// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

class GURL;

namespace dom_distiller {

class DistilledPageProto;

// A handle to a request to view a DOM distiller entry or URL. The request will
// be cancelled when the handle is destroyed.
class ViewerHandle {
  typedef base::Callback<void()> CancelCallback;

 public:
  explicit ViewerHandle(CancelCallback callback);
  ~ViewerHandle();

 private:
  CancelCallback cancel_callback_;
  DISALLOW_COPY_AND_ASSIGN(ViewerHandle);
};

// Interface for a DOM distiller entry viewer. Implement this to make a view
// request and receive the data for an entry when it becomes available.
class ViewRequestDelegate {
 public:
  virtual ~ViewRequestDelegate() {}
  // Called when the distilled article contents are available. The
  // DistilledPageProto is owned by a TaskTracker instance and is invalidated
  // when the corresponding ViewerHandle is destroyed (or when the
  // DomDistillerService is destroyed).
  virtual void OnArticleReady(DistilledPageProto* proto) = 0;
};

// A TaskTracker manages the various tasks related to viewing, saving,
// distilling, and fetching an article's distilled content.
//
// There are two sources of distilled content, a Distiller and the BlobFetcher.
// At any time, at most one of each of these will be in-progress (if one
// finishes, the other will be cancelled).
//
// There are also two consumers of that content, a view request and a save
// request. There is at most one save request (i.e. we are either adding this to
// the reading list or we aren't), and may be multiple view requests. When
// the distilled content is ready, each of these requests will be notified.
//
// A view request is cancelled by deleting the corresponding ViewerHandle. Once
// all view requests are cancelled (and the save callback has been called if
// appropriate) the cancel callback will be called.
//
// After creating a TaskTracker, a consumer of distilled content should be added
// and at least one of the sources should be started.
class TaskTracker {
 public:
  typedef base::Callback<void(TaskTracker*)> CancelCallback;
  typedef base::Callback<void(const ArticleEntry&, DistilledPageProto*)>
      SaveCallback;

  TaskTracker(const ArticleEntry& entry, CancelCallback callback);
  ~TaskTracker();

  // |factory| will not be stored after this call.
  void StartDistiller(DistillerFactory* factory);
  void StartBlobFetcher();

  void SetSaveCallback(SaveCallback callback);

  // The ViewerHandle should be destroyed before the ViewRequestDelegate.
  scoped_ptr<ViewerHandle> AddViewer(ViewRequestDelegate* delegate);

  bool HasEntryId(const std::string& entry_id) const;
  bool HasUrl(const GURL& url) const;

 private:
  void OnDistilledDataReady(scoped_ptr<DistilledPageProto> distilled_page);
  void DoSaveCallback();

  void RemoveViewer(ViewRequestDelegate* delegate);
  void NotifyViewer(ViewRequestDelegate* delegate);

  void MaybeCancel();

  CancelCallback cancel_callback_;
  SaveCallback save_callback_;

  scoped_ptr<Distiller> distiller_;

  // A ViewRequestDelegate will be added to this list when a view request is
  // made and removed when the corresponding ViewerHandle is destroyed.
  std::vector<ViewRequestDelegate*> viewers_;

  ArticleEntry entry_;
  scoped_ptr<DistilledPageProto> distilled_page_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<TaskTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_TASK_TRACKER_H_
