// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/bookmark_image_service.h"

#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "components/enhanced_bookmarks/persistent_image_store.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const char kSequenceToken[] = "BookmarkImagesSequenceToken";

void ConstructPersistentImageStore(PersistentImageStore* store,
                                   const base::FilePath& path) {
  DCHECK(store);
  new (store) PersistentImageStore(path);
}

void DeleteImageStore(ImageStore* store) {
  DCHECK(store);
  delete store;
}

void RetrieveImageFromStoreRelay(
    ImageStore* store,
    const GURL& page_url,
    enhanced_bookmarks::BookmarkImageService::ImageCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> origin_loop) {
  origin_loop->PostTask(FROM_HERE, base::Bind(callback, store->Get(page_url)));
}

}  // namespace

namespace enhanced_bookmarks {
BookmarkImageService::BookmarkImageService(
    scoped_ptr<ImageStore> store,
    EnhancedBookmarkModel* enhanced_bookmark_model,
    scoped_refptr<base::SequencedWorkerPool> pool)
    : enhanced_bookmark_model_(enhanced_bookmark_model),
      store_(store.Pass()),
      pool_(pool) {
  DCHECK(CalledOnValidThread());
  enhanced_bookmark_model_->bookmark_model()->AddObserver(this);
}

BookmarkImageService::BookmarkImageService(
    const base::FilePath& path,
    EnhancedBookmarkModel* enhanced_bookmark_model,
    scoped_refptr<base::SequencedWorkerPool> pool)
    : enhanced_bookmark_model_(enhanced_bookmark_model), pool_(pool) {
  DCHECK(CalledOnValidThread());
  // PersistentImageStore has to be constructed in the thread it will be used,
  // so we are posting the construction to the thread.  However, we first
  // allocate memory and keep here. The reason is that, before
  // PersistentImageStore construction is done, it's possible that
  // another member function, that posts store_ to the thread, is called.
  // Although the construction might not be finished yet, we still want to post
  // the task since it's guaranteed to be constructed by the time it is used, by
  // the sequential thread task pool.
  //
  // Other alternatives:
  // - Using a lock or WaitableEvent for PersistentImageStore construction.
  //   But waiting on UI thread is discouraged.
  // - Posting the current BookmarkImageService instance instead of store_.
  //   But this will require using a weak pointer and can potentially block
  //   destroying BookmarkImageService.
  PersistentImageStore* store =
      (PersistentImageStore*)::operator new(sizeof(PersistentImageStore));
  store_.reset(store);
  pool_->PostNamedSequencedWorkerTask(
      kSequenceToken,
      FROM_HERE,
      base::Bind(&ConstructPersistentImageStore, store, path));
}

BookmarkImageService::~BookmarkImageService() {
  DCHECK(CalledOnValidThread());
  pool_->PostNamedSequencedWorkerTask(
      kSequenceToken,
      FROM_HERE,
      base::Bind(&DeleteImageStore, store_.release()));
}

void BookmarkImageService::Shutdown() {
  DCHECK(CalledOnValidThread());
  enhanced_bookmark_model_->bookmark_model()->RemoveObserver(this);
  enhanced_bookmark_model_ = NULL;
}

void BookmarkImageService::SalientImageForUrl(const GURL& page_url,
                                              ImageCallback callback) {
  DCHECK(CalledOnValidThread());
  SalientImageForUrl(page_url, true, callback);
}

void BookmarkImageService::RetrieveImageFromStore(const GURL& page_url,
                                                  ImageCallback callback) {
  DCHECK(CalledOnValidThread());
  pool_->PostSequencedWorkerTaskWithShutdownBehavior(
      pool_->GetNamedSequenceToken(kSequenceToken),
      FROM_HERE,
      base::Bind(&RetrieveImageFromStoreRelay,
                 base::Unretained(store_.get()),
                 page_url,
                 callback,
                 base::ThreadTaskRunnerHandle::Get()),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

void BookmarkImageService::RetrieveSalientImageForPageUrl(
    const GURL& page_url) {
  DCHECK(CalledOnValidThread());
  if (IsPageUrlInProgress(page_url))
    return;  // A request for this URL is already in progress.

  in_progress_page_urls_.insert(page_url);

  const BookmarkNode* bookmark =
      enhanced_bookmark_model_->bookmark_model()
          ->GetMostRecentlyAddedUserNodeForURL(page_url);
  GURL image_url;
  if (bookmark) {
    int width;
    int height;
    enhanced_bookmark_model_->GetThumbnailImage(
        bookmark, &image_url, &width, &height);
  }

  RetrieveSalientImage(
      page_url,
      image_url,
      "",
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      false);
}

void BookmarkImageService::FetchCallback(const GURL& page_url,
                                         ImageCallback original_callback,
                                         const ImageRecord& record) {
  DCHECK(CalledOnValidThread());
  if (!record.image.IsEmpty() || !record.url.is_empty()) {
    // Either the record was in the store or there is no image in the store, but
    // an URL for a record is present, indicating that a previous attempt to
    // download the image failed. Just return the record.
    original_callback.Run(record);
  } else {
    // There is no record in the store, and no previous attempts to retrieve
    // one. Start a request to retrieve a salient image if there is an image
    // url set on a bookmark, and then enqueue the request for the record to
    // be triggered when the retrieval is finished.
    RetrieveSalientImageForPageUrl(page_url);
    SalientImageForUrl(page_url, false, original_callback);
  }
}

void BookmarkImageService::SalientImageForUrl(const GURL& page_url,
                                              bool fetch_from_web,
                                              ImageCallback callback) {
  DCHECK(CalledOnValidThread());

  // If the request is done while the image is currently being retrieved, just
  // store the appropriate callbacks to call once the image is retrieved.
  if (IsPageUrlInProgress(page_url)) {
    callbacks_[page_url].push_back(callback);
    return;
  }

  if (!fetch_from_web) {
    RetrieveImageFromStore(page_url, callback);
  } else {
    RetrieveImageFromStore(page_url,
                           base::Bind(&BookmarkImageService::FetchCallback,
                                      base::Unretained(this),
                                      page_url,
                                      callback));
  }
}

void BookmarkImageService::ProcessNewImage(const GURL& page_url,
                                           bool update_bookmarks,
                                           const GURL& image_url,
                                           const gfx::Image& image) {
  DCHECK(CalledOnValidThread());
  PostTaskToStoreImage(image, image_url, page_url);
  if (update_bookmarks && image_url.is_valid()) {
    const BookmarkNode* bookmark =
        enhanced_bookmark_model_->bookmark_model()
            ->GetMostRecentlyAddedUserNodeForURL(page_url);
    if (bookmark) {
      const gfx::Size& size = image.Size();
      bool result = enhanced_bookmark_model_->SetOriginalImage(
          bookmark, image_url, size.width(), size.height());
      DCHECK(result);
    }
  }
}

bool BookmarkImageService::IsPageUrlInProgress(const GURL& page_url) {
  DCHECK(CalledOnValidThread());
  return in_progress_page_urls_.find(page_url) != in_progress_page_urls_.end();
}

ImageRecord BookmarkImageService::ResizeAndStoreImage(const gfx::Image& image,
                                                      const GURL& image_url,
                                                      const GURL& page_url) {
  gfx::Image resized_image = ResizeImage(image);
  ImageRecord image_info(resized_image, image_url, SK_ColorBLACK);
  if (!resized_image.IsEmpty()) {
    image_info.dominant_color = DominantColorForImage(resized_image);
    // TODO(lpromero): this should be saved all the time, even when there is an
    // empty image. http://crbug.com/451450
    pool_->PostNamedSequencedWorkerTask(
        kSequenceToken, FROM_HERE,
        base::Bind(&ImageStore::Insert, base::Unretained(store_.get()),
                   page_url, image_info));
  }
  return image_info;
}

void BookmarkImageService::PostTaskToStoreImage(const gfx::Image& image,
                                                const GURL& image_url,
                                                const GURL& page_url) {
  DCHECK(CalledOnValidThread());

  base::Callback<ImageRecord(void)> task =
      base::Bind(&BookmarkImageService::ResizeAndStoreImage,
                 base::Unretained(this), image, image_url, page_url);
  base::Callback<void(const ImageRecord&)> reply =
      base::Bind(&BookmarkImageService::OnStoreImagePosted,
                 base::Unretained(this), page_url);

  base::PostTaskAndReplyWithResult(pool_.get(), FROM_HERE, task, reply);
}

void BookmarkImageService::OnStoreImagePosted(const GURL& page_url,
                                              const ImageRecord& image) {
  DCHECK(CalledOnValidThread());
  in_progress_page_urls_.erase(page_url);
  ProcessRequests(page_url, image);
}

void BookmarkImageService::RemoveImageForUrl(const GURL& page_url) {
  DCHECK(CalledOnValidThread());
  pool_->PostNamedSequencedWorkerTask(
      kSequenceToken,
      FROM_HERE,
      base::Bind(&ImageStore::Erase, base::Unretained(store_.get()), page_url));
  in_progress_page_urls_.erase(page_url);
  ProcessRequests(page_url, ImageRecord());
}

void BookmarkImageService::ChangeImageURL(const GURL& from, const GURL& to) {
  DCHECK(CalledOnValidThread());
  pool_->PostNamedSequencedWorkerTask(kSequenceToken,
                                      FROM_HERE,
                                      base::Bind(&ImageStore::ChangeImageURL,
                                                 base::Unretained(store_.get()),
                                                 from,
                                                 to));
  in_progress_page_urls_.erase(from);
  ProcessRequests(from, ImageRecord());
}

void BookmarkImageService::ClearAll() {
  DCHECK(CalledOnValidThread());
  // Clears and executes callbacks.
  pool_->PostNamedSequencedWorkerTask(
      kSequenceToken,
      FROM_HERE,
      base::Bind(&ImageStore::ClearAll, base::Unretained(store_.get())));

  for (std::map<const GURL, std::vector<ImageCallback>>::const_iterator it =
           callbacks_.begin();
       it != callbacks_.end(); ++it) {
    ProcessRequests(it->first, ImageRecord());
  }

  in_progress_page_urls_.erase(in_progress_page_urls_.begin(),
                               in_progress_page_urls_.end());
}

void BookmarkImageService::ProcessRequests(const GURL& page_url,
                                           const ImageRecord& record) {
  DCHECK(CalledOnValidThread());

  std::vector<ImageCallback> callbacks = callbacks_[page_url];
  for (std::vector<ImageCallback>::const_iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    it->Run(record);
  }

  callbacks_.erase(page_url);
}

// BookmarkModelObserver methods.

void BookmarkImageService::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  DCHECK(CalledOnValidThread());
  for (std::set<GURL>::const_iterator iter = removed_urls.begin();
       iter != removed_urls.end();
       ++iter) {
    RemoveImageForUrl(*iter);
  }
}

void BookmarkImageService::BookmarkModelLoaded(BookmarkModel* model,
                                               bool ids_reassigned) {
}

void BookmarkImageService::BookmarkNodeMoved(BookmarkModel* model,
                                             const BookmarkNode* old_parent,
                                             int old_index,
                                             const BookmarkNode* new_parent,
                                             int new_index) {
}

void BookmarkImageService::BookmarkNodeAdded(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int index) {
}

void BookmarkImageService::OnWillChangeBookmarkNode(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  DCHECK(CalledOnValidThread());
  if (node->is_url())
    previous_url_ = node->url();
}

void BookmarkImageService::BookmarkNodeChanged(BookmarkModel* model,
                                               const BookmarkNode* node) {
  DCHECK(CalledOnValidThread());
  if (node->is_url() && previous_url_ != node->url())
    ChangeImageURL(previous_url_, node->url());
}

void BookmarkImageService::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void BookmarkImageService::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void BookmarkImageService::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  ClearAll();
}

}  // namespace enhanced_bookmarks
