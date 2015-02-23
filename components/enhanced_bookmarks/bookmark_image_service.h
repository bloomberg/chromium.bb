// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/enhanced_bookmarks/image_record.h"
#include "components/enhanced_bookmarks/image_store.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace bookmarks {
class BookmarkNode;
}

namespace enhanced_bookmarks {

class EnhancedBookmarkModel;

// The BookmarkImageService stores salient images for bookmarks.
class BookmarkImageService : public KeyedService,
                             public bookmarks::BookmarkModelObserver,
                             public base::NonThreadSafe {
 public:
  BookmarkImageService(const base::FilePath& path,
                       EnhancedBookmarkModel* enhanced_bookmark_model,
                       scoped_refptr<base::SequencedWorkerPool> pool);
  BookmarkImageService(scoped_ptr<ImageStore> store,
                       EnhancedBookmarkModel* enhanced_bookmark_model,
                       scoped_refptr<base::SequencedWorkerPool> pool);

  ~BookmarkImageService() override;

  typedef base::Callback<void(const ImageRecord&)> ImageCallback;

  // KeyedService:
  void Shutdown() override;

  // Returns a salient image for a URL. This may trigger a network request for
  // the image if the image was not retrieved before and if a bookmark node has
  // a URL for this salient image available.  The image (which may be empty) is
  // sent via the callback. The callback may be called synchronously if it is
  // possible. The callback is always triggered on the main thread.
  void SalientImageForUrl(const GURL& page_url, ImageCallback callback);

  // bookmarks::BookmarkModelObserver:
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         int old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         int new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

 protected:
  // Returns true if the image for the page_url is currently being fetched.
  bool IsPageUrlInProgress(const GURL& page_url);

  // Stores the new image to local storage. If update_bookmarks is true, relates
  // the corresponding bookmark to image_url.
  void ProcessNewImage(const GURL& page_url,
                       bool update_bookmarks,
                       const GURL& image_url,
                       const gfx::Image& image);

  // Resizes large images to proper size that fits device display. This method
  // should _not_ run on the UI thread.
  virtual gfx::Image ResizeImage(gfx::Image image) = 0;

  // Sets a new image for a bookmark. If the given page_url is bookmarked and
  // the image is retrieved from the image_url, then the image is locally
  // stored. If update_bookmark is true the URL is also added to the bookmark.
  virtual void RetrieveSalientImage(
      const GURL& page_url,
      const GURL& image_url,
      const std::string& referrer,
      net::URLRequest::ReferrerPolicy referrer_policy,
      bool update_bookmark) = 0;

  // Retrieves a salient image for a given page_url by downloading the image in
  // one of the bookmark.
  virtual void RetrieveSalientImageForPageUrl(const GURL& page_url);

  // PageUrls currently in the progress of being retrieved.
  std::set<GURL> in_progress_page_urls_;

  // Cached pointer to the bookmark model.
  EnhancedBookmarkModel* enhanced_bookmark_model_;

 private:
  // If fetch_from_web is true, retrieves the salient image via a network
  // request; else only gets the image from local storage.
  void SalientImageForUrl(const GURL& page_url,
                          bool fetch_from_web,
                          ImageCallback stack_callback);

  // Processes the requests that have been waiting on an image.
  void ProcessRequests(const GURL& page_url, const ImageRecord& image);

  // Once an image is retrieved this method calls ResizeImage() and updates the
  // store with the smaller image, then returns the newly formed ImageRecord.
  // This is typically called on |pool_|, the background sequenced worker pool
  // for this object.
  ImageRecord ResizeAndStoreImage(const gfx::Image& image,
                                  const GURL& image_url,
                                  const GURL& page_url);

  // Calls |StoreImage| in the background.  This should only be called from the
  // main thread.
  void PostTaskToStoreImage(const gfx::Image& image,
                            const GURL& image_url,
                            const GURL& page_url);

  // Called when |StoreImage| as been posted.  This should only be called from
  // the main thread.
  void OnStoreImagePosted(const GURL& page_url, const ImageRecord& image);

  // Called when retrieving an image from the image store fails, to trigger
  // retrieving the image from the url stored in the bookmark (if any).
  void FetchCallback(const GURL& page_url,
                     ImageCallback original_callback,
                     const ImageRecord& record);

  // Remove the image stored for this bookmark (if it exists). Called when a
  // bookmark is deleted.
  void RemoveImageForUrl(const GURL& url);

  // Moves an image from one url to another.
  void ChangeImageURL(const GURL& from, const GURL& to);

  // Removes all the entries in the image service.
  void ClearAll();

  // The image store can only be accessed from the blocking pool.
  // RetrieveImageFromStore starts a request to retrieve the image and returns
  // the result via a callback. RetrieveImageFromStore must be called on the
  // main thread and the callback will be called on the main thread as well. The
  // callback will always be called. The returned image is nil if the image is
  // not present in the store.
  void RetrieveImageFromStore(const GURL& page_url, ImageCallback callback);

  // Maps a pageUrl to an image.
  scoped_ptr<ImageStore> store_;

  // All the callbacks waiting for a particular image.
  std::map<const GURL, std::vector<ImageCallback>> callbacks_;

  // When a bookmark is changed, two messages are received on the
  // bookmarkModelObserver, one with the old state, one with the new. The url
  // before the change is saved in this instance variable.
  GURL previous_url_;

  // The worker pool to enqueue the requests onto.
  scoped_refptr<base::SequencedWorkerPool> pool_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkImageService);
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_H_

