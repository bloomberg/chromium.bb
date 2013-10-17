// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_H_

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"

class BookmarkTagModelObserver;

typedef string16 BookmarkTag;

// BookmarTagModel provides a way to access and manipulate bookmarks in a
// non-hierarchical way. BookmarkTagModel view the bookmarks as a flat list,
// and each one can be marked with a collection of tags (tags are simply
// strings).
//
// BookmarkTagModel converts on demand the data from an existing BookmarkModel
// to its view of the world by considering all the titles of all the ancestors
// as tags. This view is frozen on an individual bookmarks when the
// BookmarkTagModel performs a change on the tags of this bookmarks.
//
// The Bookmark's meta info is used for storage.
//
// An observer may be attached to a BookmarkTagModel to observe relevant events.
//
// TODO(noyau): The meta info data is not preserved by copy/paste or drag and
// drop, this means that bookmarks will loose their tag information if
// manipulated that way.
//
class BookmarkTagModel : public BookmarkModelObserver {
 public:
  explicit BookmarkTagModel(BookmarkModel* bookmark_model);
  virtual ~BookmarkTagModel();

  // Returns true if the model finished loading.
  bool loaded() const { return loaded_; }

  // Add and remove observers on this object.
  void AddObserver(BookmarkTagModelObserver* observer);
  void RemoveObserver(BookmarkTagModelObserver* observer);

  // Brackets an extensive set of changes, such as during import or sync, so
  // observers can delay any expensive UI updates until it's finished.
  class ExtensiveChanges {
   public:
    friend class BookmarkTagModel;
    explicit ExtensiveChanges(BookmarkTagModel* model) : model_(model) {
      model_->BeginExtensiveChanges();
    }
   private:
    ~ExtensiveChanges() {
      model_->EndExtensiveChanges();
    }

    BookmarkTagModel* model_;
  };

  // Returns true if this bookmark model is currently in a mode where extensive
  // changes might happen, such as for import and sync. This is helpful for
  // observers that are created after the mode has started, and want to check
  // state during their own initializer.
  bool IsDoingExtensiveChanges() const;

  // Removes the given |BookmarkNode|. Observers are notified immediately.
  void Remove(const BookmarkNode* bookmark);

  // Removes all the bookmark nodes. Observers are only notified when all nodes
  // have been removed. There is no notification for individual node removals.
  void RemoveAll();

  // Returns the favicon for |node|. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const gfx::Image& GetFavicon(const BookmarkNode* bookmark);

  // Sets the title of |node|.
  void SetTitle(const BookmarkNode* bookmark, const string16& title);

  // Sets the URL of |node|.
  void SetURL(const BookmarkNode* bookmark, const GURL& url);

  // Sets the date added time of |node|.
  void SetDateAdded(const BookmarkNode* bookmark, base::Time date_added);

  // Returns the most recently added bookmark for the |url|. Returns NULL if
  // |url| is not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedBookmarkForURL(const GURL& url);

  // Creates a new bookmark.
  const BookmarkNode* AddURL(const string16& title,
                             const GURL& url,
                             const std::set<BookmarkTag>& tags);

  // Adds the |tags| to the tag list of the |bookmark|.
  void AddTagsToBookmark(const std::set<BookmarkTag>& tags,
                         const BookmarkNode* bookmark);

  // Same but to a whole collection of |bookmarks|.
  void AddTagsToBookmarks(const std::set<BookmarkTag>& tags,
                          const std::set<const BookmarkNode*>& bookmarks);

  // Remove the |tags| from the tag list of the |bookmark|. If the bookmark
  // is not tagged with one or more of the tags, these are ignored.
  void RemoveTagsFromBookmark(const std::set<BookmarkTag>& tags,
                              const BookmarkNode* bookmark);

  // Same but to a whole collection of |bookmarks|.
  void RemoveTagsFromBookmarks(const std::set<BookmarkTag>& tags,
                               const std::set<const BookmarkNode*>& bookmarks);

  // Returns all the tags set on a specific |bookmark|.
  std::set<BookmarkTag> GetTagsForBookmark(const BookmarkNode* bookmark);

  // Returns the bookmarks marked with all the given |tags|.
  std::set<const BookmarkNode*> BookmarksForTags(
      const std::set<BookmarkTag>& tags);

  // Returns the bookmarks marked with the given |tag|.
  std::set<const BookmarkNode*> BookmarksForTag(const BookmarkTag& tag);

  // Returns all tags related to the parent |tag|. If |tag| is null this method
  // will returns and sort all tags in the system. A related tag is a tag used
  // on one or more of the bookmarks tagged with |tag|. The returned tags are
  // ordered from the most common to the rarest one.
  std::vector<BookmarkTag> TagsRelatedToTag(const BookmarkTag& tag);

  // All the BookmarkModelObserver methods. See there for details.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void OnWillRemoveBookmarks(BookmarkModel* model,
                                     const BookmarkNode* parent,
                                     int old_index,
                                     const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void OnWillChangeBookmarkNode(BookmarkModel* model,
                                        const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                            const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkMetaInfoChanged(BookmarkModel* model,
                                       const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void OnWillReorderBookmarkNode(BookmarkModel* model,
                                         const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE;
  virtual void OnWillRemoveAllBookmarks(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkAllNodesRemoved(BookmarkModel* model) OVERRIDE;

 private:
  // Notifies the observers that an extensive set of changes is about to happen,
  // such as during import or sync, so they can delay any expensive UI updates
  // until it's finished.
  void BeginExtensiveChanges();
  void EndExtensiveChanges();

  // Encode the tags in a format suitable for the BookmarkNode's metaInfo and
  // set or replace the value.
  void SetTagsOnBookmark(const std::set<BookmarkTag>& tags,
                             const BookmarkNode* bookmark);

  // Build the caches of tag to bookmarks and bookmarks to tag for faster
  // access to the data. Load() is called from the constructor if possible, or
  // as soon as possible after that.
  void Load();

  // Discard tag information for all descendants of a given folder node and
  // rebuild the cache for them.
  void ReloadDescendants(const BookmarkNode* folder);

  // Clear the local cache of all mentions of |bookmark|.
  void RemoveBookmark(const BookmarkNode* bookmark);

  // Extract the tags from |bookmark| and insert it in the local cache.
  void LoadBookmark(const BookmarkNode* bookmark);

  // The model from where the data is permanently stored.
  BookmarkModel* bookmark_model_;

  // True if the model is fully loaded.
  bool loaded_;

  // The observers.
  ObserverList<BookmarkTagModelObserver> observers_;

  // Local cache for quick access.
  std::map<const BookmarkTag, std::set<const BookmarkNode*> > tag_to_bookmarks_;
  std::map<const BookmarkNode*, std::set<BookmarkTag> > bookmark_to_tags_;

  // Set to true during the creation of a new bookmark in order to send only the
  // proper notification.
  bool inhibit_change_notifications_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTagModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_H_
