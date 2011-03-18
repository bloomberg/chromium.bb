// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
#pragma once

#include "build/build_config.h"

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "content/browser/cancelable_request.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_model.h"

class BookmarkIndex;
class BookmarkLoadDetails;
class BookmarkModel;
class BookmarkStorage;
class Profile;

namespace bookmark_utils {
struct TitleMatch;
}

// BookmarkNode ---------------------------------------------------------------

// BookmarkNode contains information about a starred entry: title, URL, favicon,
// star id and type. BookmarkNodes are returned from a BookmarkModel.
//
class BookmarkNode : public ui::TreeNode<BookmarkNode> {
  friend class BookmarkModel;

 public:
  enum Type {
    URL,
    FOLDER,
    BOOKMARK_BAR,
    OTHER_NODE
  };
  // Creates a new node with the specified url and id of 0
  explicit BookmarkNode(const GURL& url);
  // Creates a new node with the specified url and id.
  BookmarkNode(int64 id, const GURL& url);
  virtual ~BookmarkNode();

  // Returns the URL.
  const GURL& GetURL() const { return url_; }
  // Sets the URL to the given value.
  void SetURL(const GURL& url) { url_ = url; }

  // Returns a unique id for this node.
  // For bookmark nodes that are managed by the bookmark model, the IDs are
  // persisted across sessions.
  int64 id() const { return id_; }
  // Sets the id to the given value.
  void set_id(int64 id) { id_ = id; }

  // Returns the type of this node.
  BookmarkNode::Type type() const { return type_; }
  void set_type(BookmarkNode::Type type) { type_ = type; }

  // Returns the time the bookmark/folder was added.
  const base::Time& date_added() const { return date_added_; }
  // Sets the time the bookmark/folder was added.
  void set_date_added(const base::Time& date) { date_added_ = date; }

  // Returns the last time the folder was modified. This is only maintained
  // for folders (including the bookmark and other folder).
  const base::Time& date_folder_modified() const {
    return date_folder_modified_;
  }
  // Sets the last time the folder was modified.
  void set_date_folder_modified(const base::Time& date) {
    date_folder_modified_ = date;
  }

  // Convenience for testing if this nodes represents a folder. A folder is a
  // node whose type is not URL.
  bool is_folder() const { return type_ != URL; }

  // Is this a URL?
  bool is_url() const { return type_ == URL; }

  // Returns the favicon. In nearly all cases you should use the method
  // BookmarkModel::GetFavicon rather than this. BookmarkModel::GetFavicon
  // takes care of loading the favicon if it isn't already loaded, where as
  // this does not.
  const SkBitmap& favicon() const { return favicon_; }
  void set_favicon(const SkBitmap& icon) { favicon_ = icon; }

  // The following methods are used by the bookmark model, and are not
  // really useful outside of it.

  bool is_favicon_loaded() const { return loaded_favicon_; }
  void set_favicon_loaded(bool value) { loaded_favicon_ = value; }

  HistoryService::Handle favicon_load_handle() const {
    return favicon_load_handle_;
  }
  void set_favicon_load_handle(HistoryService::Handle handle) {
    favicon_load_handle_ = handle;
  }

  // Called when the favicon becomes invalid.
  void InvalidateFavicon();

  // Resets the properties of the node from the supplied entry.
  // This is used by the bookmark model and not really useful outside of it.
  void Reset(const history::StarredEntry& entry);

  // TODO(sky): Consider adding last visit time here, it'll greatly simplify
  // HistoryContentsProvider.

 private:
  // helper to initialize various fields during construction.
  void Initialize(int64 id);

  // Unique identifier for this node.
  int64 id_;

  // Whether the favicon has been loaded.
  bool loaded_favicon_;

  // The favicon.
  SkBitmap favicon_;

  // If non-zero, it indicates we're loading the favicon and this is the handle
  // from the HistoryService.
  HistoryService::Handle favicon_load_handle_;

  // The URL. BookmarkModel maintains maps off this URL, it is important that
  // changes to the URL is done through the bookmark model.
  GURL url_;

  // Type of node.
  BookmarkNode::Type type_;

  // Date we were created.
  base::Time date_added_;

  // Time last modified. Only used for folders.
  base::Time date_folder_modified_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNode);
};

// BookmarkModel --------------------------------------------------------------

// BookmarkModel provides a directed acyclic graph of the starred entries
// and folders. Two graphs are provided for the two entry points: those on
// the bookmark bar, and those in the other folder.
//
// An observer may be attached to observer relevant events.
//
// You should NOT directly create a BookmarkModel, instead go through the
// Profile.

class BookmarkModel : public NotificationObserver, public BookmarkService {
  friend class BookmarkCodecTest;
  friend class BookmarkModelTest;
  friend class BookmarkStorage;

 public:
  explicit BookmarkModel(Profile* profile);
  virtual ~BookmarkModel();

  // Loads the bookmarks. This is called by Profile upon creation of the
  // BookmarkModel. You need not invoke this directly.
  void Load();

  // Returns the root node. The bookmark bar node and other node are children of
  // the root node.
  const BookmarkNode* root_node() { return &root_; }

  // Returns the bookmark bar node. This is NULL until loaded.
  const BookmarkNode* GetBookmarkBarNode() { return bookmark_bar_node_; }

  // Returns the 'other' node. This is NULL until loaded.
  const BookmarkNode* other_node() { return other_node_; }

  // Returns the parent the last node was added to. This never returns NULL
  // (as long as the model is loaded).
  const BookmarkNode* GetParentForNewNodes();

  void AddObserver(BookmarkModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BookmarkModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Notify the observes that an import is about to happen, so they can
  // delay any expensive UI updates until it is finished.
  void BeginImportMode();
  void EndImportMode();

  // Unstars or deletes the specified entry. Removing a folder entry recursively
  // unstars all nodes. Observers are notified immediately.
  void Remove(const BookmarkNode* parent, int index);

  // Moves the specified entry to a new location.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Duplicates a bookmark node and inserts it at a new location.
  void Copy(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Returns the favicon for |node|. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const SkBitmap& GetFavicon(const BookmarkNode* node);

  // Sets the title of the specified node.
  void SetTitle(const BookmarkNode* node, const string16& title);

  // Sets the URL of the specified bookmark node.
  void SetURL(const BookmarkNode* node, const GURL& url);

  // Returns true if the model finished loading.
  virtual bool IsLoaded();

  // Returns the set of nodes with the specified URL.
  void GetNodesByURL(const GURL& url, std::vector<const BookmarkNode*>* nodes);

  // Returns the most recently added node for the url. Returns NULL if url is
  // not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedNodeForURL(const GURL& url);

  // Returns all the bookmarked urls. This method is thread safe.
  virtual void GetBookmarks(std::vector<GURL>* urls);

  // Returns true if there are bookmarks, otherwise returns false. This method
  // is thread safe.
  bool HasBookmarks();

  // Returns true if there is a bookmark for the specified URL. This method is
  // thread safe. See BookmarkService for more details on this.
  virtual bool IsBookmarked(const GURL& url);

  // Blocks until loaded; this is NOT invoked on the main thread. See
  // BookmarkService for more details on this.
  virtual void BlockTillLoaded();

  // Returns the node with the specified id, or NULL if there is no node with
  // the specified id.
  const BookmarkNode* GetNodeByID(int64 id);

  // Adds a new folder node at the specified position.
  const BookmarkNode* AddFolder(const BookmarkNode* parent,
                                int index,
                                const string16& title);

  // Adds a url at the specified position.
  const BookmarkNode* AddURL(const BookmarkNode* parent,
                             int index,
                             const string16& title,
                             const GURL& url);

  // Adds a url with a specific creation date.
  const BookmarkNode* AddURLWithCreationTime(const BookmarkNode* parent,
                                             int index,
                                             const string16& title,
                                             const GURL& url,
                                             const base::Time& creation_time);

  // Sorts the children of |parent|, notifying observers by way of the
  // BookmarkNodeChildrenReordered method.
  void SortChildren(const BookmarkNode* parent);

  // This is the convenience that makes sure the url is starred or not starred.
  // If is_starred is false, all bookmarks for URL are removed. If is_starred is
  // true and there are no bookmarks for url, a bookmark is created.
  void SetURLStarred(const GURL& url,
                     const string16& title,
                     bool is_starred);

  // Sets the date modified time of the specified node.
  void SetDateFolderModified(const BookmarkNode* parent, const base::Time time);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created folders from showing up in the
  // combobox of most recently modified folders.
  void ResetDateFolderModified(const BookmarkNode* node);

  void GetBookmarksWithTitlesMatching(
      const string16& text,
      size_t max_count,
      std::vector<bookmark_utils::TitleMatch>* matches);

  Profile* profile() const { return profile_; }

  bool is_root(const BookmarkNode* node) const { return node == &root_; }
  bool is_bookmark_bar_node(const BookmarkNode* node) const {
    return node == bookmark_bar_node_;
  }
  bool is_other_bookmarks_node(const BookmarkNode* node) const {
    return node == other_node_;
  }
  // Returns whether the given node is one of the permanent nodes - root node,
  // bookmark bar node or other bookmarks node.
  bool is_permanent_node(const BookmarkNode* node) const {
    return is_root(node) ||
           is_bookmark_bar_node(node) ||
           is_other_bookmarks_node(node);
  }

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

  // Returns whether the bookmarks file changed externally.
  bool file_changed() const { return file_changed_; }

  // Returns the next node ID.
  int64 next_node_id() const { return next_node_id_; }

 private:
  // Used to order BookmarkNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Implementation of IsBookmarked. Before calling this the caller must
  // obtain a lock on url_lock_.
  bool IsBookmarkedNoLock(const GURL& url);

  // Overriden to notify the observer the favicon has been loaded.
  void FaviconLoaded(const BookmarkNode* node);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNode(BookmarkNode* node, std::set<GURL>* removed_urls);

  // Invoked when loading is finished. Sets loaded_ and notifies observers.
  // BookmarkModel takes ownership of |details|.
  void DoneLoading(BookmarkLoadDetails* details);

  // Populates nodes_ordered_by_url_set_ from root.
  void PopulateNodesByURL(BookmarkNode* node);

  // Removes the node from its parent, sends notification, and deletes it.
  // type specifies how the node should be removed.
  void RemoveAndDeleteNode(BookmarkNode* delete_me);

  // Adds the node at the specified position and sends notification. If
  // was_bookmarked is true, it indicates a bookmark already existed for the
  // URL.
  BookmarkNode* AddNode(BookmarkNode* parent,
                        int index,
                        BookmarkNode* node,
                        bool was_bookmarked);

  // Implementation of GetNodeByID.
  const BookmarkNode* GetNodeByID(const BookmarkNode* node, int64 id);

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const BookmarkNode* parent, int index, bool allow_end);

  // Creates the bookmark bar/other nodes. These call into
  // CreateRootNodeFromStarredEntry.
  BookmarkNode* CreateBookmarkNode();
  BookmarkNode* CreateOtherBookmarksNode();

  // Creates a root node (either the bookmark bar node or other node) from the
  // specified starred entry.
  BookmarkNode* CreateRootNodeFromStarredEntry(
      const history::StarredEntry& entry);

  // Notification that a favicon has finished loading. If we can decode the
  // favicon, FaviconLoaded is invoked.
  void OnFaviconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon);

  // Invoked from the node to load the favicon. Requests the favicon from the
  // favicon service.
  void LoadFavicon(BookmarkNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFaviconLoadRequests(BookmarkNode* node);

  // NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Generates and returns the next node ID.
  int64 generate_next_node_id();

  // Sets the maximum node ID to the given value.
  // This is used by BookmarkCodec to report the maximum ID after it's done
  // decoding since during decoding codec assigns node IDs.
  void set_next_node_id(int64 id) { next_node_id_ = id; }

  // Records that the bookmarks file was changed externally.
  void SetFileChanged();

  // Creates and returns a new BookmarkLoadDetails. It's up to the caller to
  // delete the returned object.
  BookmarkLoadDetails* CreateLoadDetails();

  NotificationRegistrar registrar_;

  Profile* profile_;

  // Whether the initial set of data has been loaded.
  bool loaded_;

  // Whether the bookmarks file was changed externally. This is set after
  // loading is complete and once set the value never changes.
  bool file_changed_;

  // The root node. This contains the bookmark bar node and the 'other' node as
  // children.
  BookmarkNode root_;

  BookmarkNode* bookmark_bar_node_;
  BookmarkNode* other_node_;

  // The maximum ID assigned to the bookmark nodes in the model.
  int64 next_node_id_;

  // The observers.
  ObserverList<BookmarkModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: nodes_ordered_by_url_set_ is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around url_lock_.
  typedef std::multiset<BookmarkNode*, NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  base::Lock url_lock_;

  // Used for loading favicons and the empty history request.
  CancelableRequestConsumerTSimple<BookmarkNode*> load_consumer_;

  // Reads/writes bookmarks to disk.
  scoped_refptr<BookmarkStorage> store_;

  scoped_ptr<BookmarkIndex> index_;

  base::WaitableEvent loaded_signal_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
