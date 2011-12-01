// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "content/browser/cancelable_request.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_model.h"

class BookmarkExpandedStateTracker;
class BookmarkIndex;
class BookmarkLoadDetails;
class BookmarkModel;
class BookmarkModelObserver;
class BookmarkStorage;
class PrefService;
class Profile;

namespace bookmark_utils {
struct TitleMatch;
}

// BookmarkNode ---------------------------------------------------------------

// BookmarkNode contains information about a starred entry: title, URL, favicon,
// id and type. BookmarkNodes are returned from BookmarkModel.
class BookmarkNode : public ui::TreeNode<BookmarkNode> {
 public:
  enum Type {
    URL,
    FOLDER,
    BOOKMARK_BAR,
    OTHER_NODE,
    MOBILE
  };

  // Creates a new node with an id of 0 and |url|.
  explicit BookmarkNode(const GURL& url);
  // Creates a new node with |id| and |url|.
  BookmarkNode(int64 id, const GURL& url);

  virtual ~BookmarkNode();

  // Returns an unique id for this node.
  // For bookmark nodes that are managed by the bookmark model, the IDs are
  // persisted across sessions.
  int64 id() const { return id_; }
  void set_id(int64 id) { id_ = id; }

  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  Type type() const { return type_; }
  void set_type(Type type) { type_ = type; }

  // Returns the time the node was added.
  const base::Time& date_added() const { return date_added_; }
  void set_date_added(const base::Time& date) { date_added_ = date; }

  // Returns the last time the folder was modified. This is only maintained
  // for folders (including the bookmark bar and other folder).
  const base::Time& date_folder_modified() const {
    return date_folder_modified_;
  }
  void set_date_folder_modified(const base::Time& date) {
    date_folder_modified_ = date;
  }

  // Convenience for testing if this node represents a folder. A folder is a
  // node whose type is not URL.
  bool is_folder() const { return type_ != URL; }
  bool is_url() const { return type_ == URL; }

  // Returns the favicon. In nearly all cases you should use the method
  // BookmarkModel::GetFavicon rather than this. BookmarkModel::GetFavicon
  // takes care of loading the favicon if it isn't already loaded, where as
  // this does not.
  const SkBitmap& favicon() const { return favicon_; }
  void set_favicon(const SkBitmap& icon) { favicon_ = icon; }

  // The following methods are used by the bookmark model, and are not
  // really useful outside of it.

  bool is_favicon_loaded() const { return is_favicon_loaded_; }
  void set_is_favicon_loaded(bool loaded) { is_favicon_loaded_ = loaded; }

  HistoryService::Handle favicon_load_handle() const {
    return favicon_load_handle_;
  }
  void set_favicon_load_handle(HistoryService::Handle handle) {
    favicon_load_handle_ = handle;
  }

  // TODO(sky): Consider adding last visit time here, it'll greatly simplify
  // HistoryContentsProvider.

 private:
  friend class BookmarkModel;

  // A helper function to initialize various fields during construction.
  void Initialize(int64 id);

  // Called when the favicon becomes invalid.
  void InvalidateFavicon();

  // The unique identifier for this node.
  int64 id_;

  // The URL of this node. BookmarkModel maintains maps off this URL, so changes
  // to the URL must be done through the BookmarkModel.
  GURL url_;

  // The type of this node. See enum above.
  Type type_;

  // Date of when this node was created.
  base::Time date_added_;

  // Date of the last modification. Only used for folders.
  base::Time date_folder_modified_;

  // The favicon of this node.
  SkBitmap favicon_;

  // Whether the favicon has been loaded.
  bool is_favicon_loaded_;

  // If non-zero, it indicates we're loading the favicon and this is the handle
  // from the HistoryService.
  HistoryService::Handle favicon_load_handle_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNode);
};

// BookmarkModel --------------------------------------------------------------

// BookmarkModel provides a directed acyclic graph of URLs and folders.
// Three graphs are provided for the three entry points: those on the 'bookmarks
// bar', those in the 'other bookmarks' folder and those in the 'mobile' folder.
//
// An observer may be attached to observe relevant events.
//
// You should NOT directly create a BookmarkModel, instead go through the
// Profile.
class BookmarkModel : public content::NotificationObserver,
                      public BookmarkService {
 public:
  explicit BookmarkModel(Profile* profile);
  virtual ~BookmarkModel();

  static void RegisterUserPrefs(PrefService* prefs);

  // Invoked prior to destruction to release any necessary resources.
  void Cleanup();

  // Loads the bookmarks. This is called by Profile upon creation of the
  // BookmarkModel. You need not invoke this directly.
  void Load();

  // Returns true if the model finished loading.
  // This is virtual so it can be mocked.
  virtual bool IsLoaded() const;

  // Returns the root node. The 'bookmark bar' node and 'other' node are
  // children of the root node.
  const BookmarkNode* root_node() { return &root_; }

  // Returns the 'bookmark bar' node. This is NULL until loaded.
  const BookmarkNode* bookmark_bar_node() { return bookmark_bar_node_; }

  // Returns the 'other' node. This is NULL until loaded.
  const BookmarkNode* other_node() { return other_node_; }

  // Returns the 'mobile' node. This is NULL until loaded.
  const BookmarkNode* mobile_node() { return mobile_node_; }

  bool is_root_node(const BookmarkNode* node) const { return node == &root_; }

  // Returns whether the given |node| is one of the permanent nodes - root node,
  // 'bookmark bar' node, 'other' node or 'mobile' node.
  bool is_permanent_node(const BookmarkNode* node) const {
    return node == &root_ ||
           node == bookmark_bar_node_ ||
           node == other_node_ ||
           node == mobile_node_;
  }

  Profile* profile() const { return profile_; }

  // Returns the parent the last node was added to. This never returns NULL
  // (as long as the model is loaded).
  const BookmarkNode* GetParentForNewNodes();

  void AddObserver(BookmarkModelObserver* observer);
  void RemoveObserver(BookmarkModelObserver* observer);

  // Notifies the observers that an import is about to happen, so they can delay
  // any expensive UI updates until it's finished.
  void BeginImportMode();
  void EndImportMode();

  // Removes the node at the given |index| from |parent|. Removing a folder node
  // recursively removes all nodes. Observers are notified immediately.
  void Remove(const BookmarkNode* parent, int index);

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Inserts a copy of |node| into |new_parent| at |index|.
  void Copy(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Returns the favicon for |node|. If the favicon has not yet been
  // loaded it is loaded and the observer of the model notified when done.
  const SkBitmap& GetFavicon(const BookmarkNode* node);

  // Sets the title of |node|.
  void SetTitle(const BookmarkNode* node, const string16& title);

  // Sets the URL of |node|.
  void SetURL(const BookmarkNode* node, const GURL& url);

  // Returns the set of nodes with the |url|.
  void GetNodesByURL(const GURL& url, std::vector<const BookmarkNode*>* nodes);

  // Returns the most recently added node for the |url|. Returns NULL if |url|
  // is not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedNodeForURL(const GURL& url);

  // Returns true if there are bookmarks, otherwise returns false.
  // This method is thread safe.
  bool HasBookmarks();

  // Returns true if there is a bookmark with the |url|.
  // This method is thread safe.
  // See BookmarkService for more details on this.
  virtual bool IsBookmarked(const GURL& url) OVERRIDE;

  // Returns all the bookmarked urls.
  // This method is thread safe.
  // See BookmarkService for more details on this.
  virtual void GetBookmarks(std::vector<GURL>* urls) OVERRIDE;

  // Blocks until loaded; this is NOT invoked on the main thread.
  // See BookmarkService for more details on this.
  virtual void BlockTillLoaded() OVERRIDE;

  // Returns the node with |id|, or NULL if there is no node with |id|.
  const BookmarkNode* GetNodeByID(int64 id) const;

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

  // Sets the date when the folder was modified.
  void SetDateFolderModified(const BookmarkNode* node, const base::Time time);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created folders from showing up in the
  // combobox of most recently modified folders.
  void ResetDateFolderModified(const BookmarkNode* node);

  void GetBookmarksWithTitlesMatching(
      const string16& text,
      size_t max_count,
      std::vector<bookmark_utils::TitleMatch>* matches);

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

  // Returns whether the bookmarks file changed externally.
  bool file_changed() const { return file_changed_; }

  // Returns the next node ID.
  int64 next_node_id() const { return next_node_id_; }

  // Returns the object responsible for tracking the set of expanded nodes in
  // the bookmark editor.
  BookmarkExpandedStateTracker* expanded_state_tracker() {
    return expanded_state_tracker_.get();
  }

 private:
  friend class BookmarkCodecTest;
  friend class BookmarkModelTest;
  friend class BookmarkStorage;

  // Used to order BookmarkNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
      return n1->url() < n2->url();
    }
  };

  // Implementation of IsBookmarked. Before calling this the caller must obtain
  // a lock on |url_lock_|.
  bool IsBookmarkedNoLock(const GURL& url);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNode(BookmarkNode* node, std::set<GURL>* removed_urls);

  // Invoked when loading is finished. Sets loaded_ and notifies observers.
  // BookmarkModel takes ownership of |details|.
  void DoneLoading(BookmarkLoadDetails* details);

  // Populates |nodes_ordered_by_url_set_| from root.
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
  const BookmarkNode* GetNodeByID(const BookmarkNode* node, int64 id) const;

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const BookmarkNode* parent, int index, bool allow_end);

  // Creates one of the possible permanent nodes (bookmark bar node, other node
  // and mobile node) from |type|.
  BookmarkNode* CreatePermanentNode(BookmarkNode::Type type);

  // Notification that a favicon has finished loading. If we can decode the
  // favicon, FaviconLoaded is invoked.
  void OnFaviconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon);

  // Invoked from the node to load the favicon. Requests the favicon from the
  // favicon service.
  void LoadFavicon(BookmarkNode* node);

  // Called to notify the observers that the favicon has been loaded.
  void FaviconLoaded(const BookmarkNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFaviconLoadRequests(BookmarkNode* node);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Generates and returns the next node ID.
  int64 generate_next_node_id();

  // Sets the maximum node ID to the given value.
  // This is used by BookmarkCodec to report the maximum ID after it's done
  // decoding since during decoding codec assigns node IDs.
  void set_next_node_id(int64 id) { next_node_id_ = id; }

  // Creates and returns a new BookmarkLoadDetails. It's up to the caller to
  // delete the returned object.
  BookmarkLoadDetails* CreateLoadDetails();

  content::NotificationRegistrar registrar_;

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
  BookmarkNode* mobile_node_;

  // The maximum ID assigned to the bookmark nodes in the model.
  int64 next_node_id_;

  // The observers.
  ObserverList<BookmarkModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: |nodes_ordered_by_url_set_| is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around |url_lock_|.
  typedef std::multiset<BookmarkNode*, NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  base::Lock url_lock_;

  // Used for loading favicons and the empty history request.
  CancelableRequestConsumerTSimple<BookmarkNode*> load_consumer_;

  // Reads/writes bookmarks to disk.
  scoped_refptr<BookmarkStorage> store_;

  scoped_ptr<BookmarkIndex> index_;

  base::WaitableEvent loaded_signal_;

  scoped_ptr<BookmarkExpandedStateTracker> expanded_state_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_H_
