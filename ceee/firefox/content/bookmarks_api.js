// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * bookmarks APIs for the CEEE Firefox add-on.  This file is loaded by
 * the overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */

/**
 * Place-holder namespace-object for helper functions used in this file.
 * @private
 */
var CEEE_bookmarks_internal_ = {
  /** Reference to the instance object for the ceee. @private */
  ceeeInstance_: null,

  /**
   * Log an informational message to the Firefox error console.
   * @private
   */
  logInfo_: null,

  /**
    * Log an error message to the Firefox error console.
    * @private
    */
  logError_: null
};

/**
 * Iterates through all of the children of a given bookmark folder, invoking
 * the callback for each child.
 *
 * @param {number} bookmarkId The id of the bookmark to find.
 * @param {Function} callback Function to invoke for each child bookmark.  If
 *     the function returns false, then the iteration breaks early.
 * @private
 */
CEEE_bookmarks_internal_.forEachChild_ = function(bookmarkId, callback) {
  try {
    var index = 0;
    // The Mozilla nsINavBookmarksService does not provide a means to directly
    // access the number of bookmarks within a folder.  Instead we iterate
    // from 0 until an exception is thrown.
    // TODO(twiz@chromium.org): Investigate if the bookmarks within a
    // folder may be sparse, and so this loop will break too early.
    do {
      var childId = CEEE_bookmarks.getIdForItemAt(bookmarkId, index++);
      // Using the !operator would coerce null values into false, which is
      // not the break condition.  We want to stop ONLY if false is returned.
      if (false === callback(childId)) {
        return;
      }
    } while(true);
  } catch (e) {
    // Catch the out-of-bounds exception thrown when accessing the element past
    // the end of the set of children, otherwise pass the exception on.
    if (e && e.result &&
        e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
      return;
    } else {
      throw e;
    }
  }
};

/** @typedef {{id: number,
 *            title: string,
 *            parentId: number,
 *            url: string,
 *            children: Array.<CEEE_bookmarks_internal_.BookmarkItem>}}
 */
CEEE_bookmarks_internal_.BookmarkItem;

/**
 * Packages an object containing the members specified by the Chrome
 * bookmark extension BookmarkItem object.
 *
 * @param {number} bookmarkId The id of the bookmark to find.
 * @param {string} title The title of the bookmark.
 * @param {number} parentId The id of the parent of the bookmark.
 * @param {Object} url A url object, as constructed by nsIIOService.  If the
 *     bookmark is a folder, then this parameter should be null.
 * @param {Array} children The array of children of the bookmark.  Should be
 *     null if a url is provided.
 * @return { {id: number, title: string, parentId: number, url: string,
 *     children: Array} } A BookmarkItem.
 * @private
 */
CEEE_bookmarks_internal_.createChromeBookmark_ = function(bookmarkId,
    title, parentId, url, children) {
  return {
    'id': bookmarkId,
    'title': title,
    'parentId': parentId,
    'url': url ? url.spec : null,
    'children': children || []};
};

/**
 * Constructs and returns a BookmarkItem structure as defined by the
 * Chrome extension bookmark model corresponding to the Mozilla places
 * bookmark with the given id.
 *
 * @param {number} bookmarkId The id of the bookmark to find.
 * @return {CEEE_bookmarks_internal_.BookmarkItem}
 *    If a bookmark with the given id is found, returns an object with
 *    the following properties defined by the Chrome bookmark API.
 * @private
 */
CEEE_bookmarks_internal_.getBookmarkItemFromId_ = function(bookmarkId) {
  var title = CEEE_bookmarks.getItemTitle(bookmarkId);
  var parentId = CEEE_bookmarks.getFolderIdForItem(bookmarkId);
  var url;

  try {
    url = CEEE_bookmarks.getBookmarkURI(bookmarkId);
  } catch (e) {
    // If a bookmark does not exist for this id, getBookmarkURI will throw
    // NS_ERROR_INVALID_ARG.  We catch that here and return null instead.
    if (!(e && e.result &&
          e.result == Components.results.NS_ERROR_INVALID_ARG)) {
      throw e;
    }
  }

  var item = this.createChromeBookmark_(bookmarkId, title, parentId, url);

  this.forEachChild_(bookmarkId, function(childId) {
      item.children.push(childId);
  });
  return item;
};

/**
 * Returns an array of BookmarkItem objects, as defined by the Chrome bookmark
 * extension API for the children of the given folder.
 *
 * @param {number} folderId The id of the folder whose children are to
 *     be returned.
 * @return {Array} An array of BookmarItem objects representing the children
 *     of the folder with id passed as folderId.
 * @private
 */
CEEE_bookmarks_internal_.getChildren_ = function(folderId) {
  var itemType = CEEE_bookmarks.getItemType(folderId);
  var children = [];

  // Bookmarks and separators do not have children, only folders have children.
  if (CEEE_bookmarks.TYPE_BOOKMARK == itemType ||
      CEEE_bookmarks.TYPE_SEPARATOR == itemType) {
    return children;
  }

  var impl = this;
  this.forEachChild_(folderId, function(childId) {
      var treeNode = impl.createChromeBookmark_(
          childId, CEEE_bookmarks.getItemTitle(childId), folderId);

      var itemType = CEEE_bookmarks.getItemType(childId);
      if (CEEE_bookmarks.TYPE_BOOKMARK == itemType) {
        var url = CEEE_bookmarks.getBookmarkURI(childId);
        if (url) {
          treeNode.url = url.spec;
        }
      } else if (CEEE_bookmarks.TYPE_SEPARATOR != itemType) {
        treeNode.children = impl.getChildren_(childId);
      }
      children.push(treeNode);
  });

  return children;
};

/**
 * Helper function for searching for a set of search terms in a given text.
 *
 * @param {!Array.<string>} querySet An array of strings, for which we are
 *      searching.
 * @param {string} text A string in which we are looking for all of the
 *     elements of querySet.
 * @return {boolean} Whether all elements in querySet are present in text.
 * @private
 */
CEEE_bookmarks_internal_.textInQuerySet_ = function(querySet, text) {
  if (!text) {
    return false;
  }

  for (var wordIndex = 0; wordIndex < querySet.length; ++wordIndex) {
    if (-1 == text.indexOf(querySet[wordIndex])) {
      return false;
    }
  }

  return true;
};

/**
 * Searches for the set of bookmarks containing all elements of the strings
 * in querySet.  The search starts at item bookmarkId, and recursively visits
 * all children items.
 * When a bookmark item is found, onMatch is called to register the match.
 * If onMatch returns false, the search terminates.
 *
 * @param {!Array.<string>} querySet Array of strings to find in the title/url
 *     of the bookmarks contained in the tree rooted at bookmarkId.
 * @param {number} bookmarkId The id of the folder/item to search.
 * @param {function(number):boolean} onMatch Function to call when a match is
 *     found.
 * @return {boolean} true if the search was exhaustive, and all bookmark
 *    children were examined.
 * @private
 */
CEEE_bookmarks_internal_.getBookmarksContainingText_ =
    function(querySet, bookmarkId, onMatch) {
  var itemType = CEEE_bookmarks.getItemType(bookmarkId);
  if (CEEE_bookmarks.TYPE_FOLDER == itemType ||
      CEEE_bookmarks.TYPE_DYNAMIC_CONTAINER == itemType) {
    var impl = this;
    // Recurse on each of the children of bookmark with id bookmarkId.
    this.forEachChild_(bookmarkId, function(childId) {
        if (!impl.getBookmarksContainingText_(querySet, childId, onMatch)) {
          return false;
        }
    });
  } else {
    var title = CEEE_bookmarks.getItemTitle(bookmarkId);
    var url = CEEE_bookmarks.getBookmarkURI(bookmarkId);
    if (url) {
      url = url.spec;
    }

    if (this.textInQuerySet_(querySet, title) ||
        this.textInQuerySet_(querySet, url)) {
      return onMatch(bookmarkId);
    }
  }

  return true;
};


/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.getChildren.
 */
CEEE_bookmarks_internal_.CMD_GET_BOOKMARK_CHILDREN =
    'bookmarks.getChildren';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.getTree.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.getBookmarkChildren_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var bookmarkId = data.args;
  var children = [];
  var impl = this;
  this.forEachChild_(bookmarkId, function(childId) {
    children.push(impl.getBookmarkItemFromId_(childId));
  });
  return children;
};

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.getTree.
 */
CEEE_bookmarks_internal_.CMD_GET_BOOKMARK_TREE = 'bookmarks.getTree';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.getTree.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.getBookmarkTree_ = function(cmd, data) {
  var treeRoot = this.createChromeBookmark_(
      CEEE_bookmarks.placesRoot,
      CEEE_bookmarks.getItemTitle(CEEE_bookmarks.placesRoot));
  // TODO(twiz@chromium.org): This routine presently returns the root
  // of places, which includes elements outside of the normal
  // 'bookmarks scope', such as the recent browsing histroy, etc.  It
  // should be determined if bookmarksMenuFolder or
  // unfiledBookmarksFolder is more appropriate.
  treeRoot.children = this.getChildren_(CEEE_bookmarks.placesRoot);
  return treeRoot;
};

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.search.
 */
CEEE_bookmarks_internal_.CMD_SEARCH_BOOKMARKS = 'bookmarks.search';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.search.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.searchBookmarks_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var results = [];
  var query = data.args;
  // Because FireFox's JSON parsing engine does not support JSON encoding of
  // primitive types (such as strings), we explicitly check for a 'null'
  // string here to indicate that no arguments were given, and return
  // an empty set.
  // NOTE:  The check against 3 is because encoded strings will be bracketed
  // with ".  ie "a".
  if (!query || query == 'null' || query.length < 3) {
    return results;
  }

  var impl = this;
  // The query, as performed by the chrome implementation is case-insensitive.
  query = query.toLowerCase();
  query = query.substring(1, query.length - 1);
  var querySet =  query.split(' ');
  this.getBookmarksContainingText_(
      querySet,
      CEEE_bookmarks.placesRoot,
      function(bookmarkId) {

    results.push(impl.getBookmarkItemFromId_(bookmarkId));

    // Restrict the set of returned results to no greater than 50.
    // This mimics the behaviour of the API as implemented in Chrome.
    return results.length <= 50;
  });

  return results;
};

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.remove.
 */
CEEE_bookmarks_internal_.CMD_REMOVE_BOOKMARK = 'bookmarks.remove';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.remove.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.removeBookmark_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var args = CEEE_json.decode(data.args);
  if (!args || !args.id) {
    return;
  }

  var recurse = cmd == this.CMD_REMOVE_BOOKMARK_TREE;
  var itemType = CEEE_bookmarks.getItemType(args.id);
  if (CEEE_bookmarks.TYPE_FOLDER == itemType ||
      CEEE_bookmarks.TYPE_DYNAMIC_CONTAINER == itemType) {
    // Disallow removal of a non-empty folder unless the recursive argument
    // is set.
    var children = this.getChildren_(args.id);
    if (!(children && children.length > 0 && !recurse)) {
      CEEE_bookmarks.removeFolder(args.id);
    }
  } else {
    CEEE_bookmarks.removeItem(args.id);
  }
}

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.removeTree.
 */
CEEE_bookmarks_internal_.CMD_REMOVE_BOOKMARK_TREE = 'bookmarks.removeTree';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.removeTree.
  * Note:  This routine aliases to removeBookmark_, which specializes its
  * behaviour based on the contents of the command string argument.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.removeBookmarkTree_ =
  CEEE_bookmarks_internal_.removeBookmark_;


/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.create.
 */
CEEE_bookmarks_internal_.CMD_CREATE_BOOKMARK = 'bookmarks.create';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.create.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.createBookmark_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var args = CEEE_json.decode(data.args);
  if (!args) {
    return;
  }

  // If no parent id is provided, default to the unfiledBookmarksFolder.
  // Explicitly test for null, as 0 may be a valid bookmark id.
  var parentId = args.parentId;
  if (parentId == null) {
    parentId = CEEE_bookmarks.unfiledBookmarksFolder;
  }

  // If no index is provided, then insert at the end of the child-list.
  var index = args.index;
  if (index == null) {
    index = CEEE_bookmarks.DEFAULT_INDEX;
  }

  var title = args.title;
  if (!title) {
    return;
  }

  var url = args.url;
  var newBookmarkId;
  try {
    // If no url is provided, then create a folder.
    if (!url) {
      newBookmarkId = CEEE_bookmarks.createFolder(parentId,
                                                              title,
                                                              index);
    } else {
      var uri = CEEE_ioService.newURI(url, null, null);
      newBookmarkId = CEEE_bookmarks.insertBookmark(parentId,
                                                                uri,
                                                                index,
                                                                title);
    }

    return this.getBookmarkItemFromId_(newBookmarkId);
  } catch (e) {
    // If the provided url is not formatted correctly, then newURI will throw.
    // Return null to indicate failure.
    return;
  }
};

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.move.
 */
CEEE_bookmarks_internal_.CMD_MOVE_BOOKMARK = 'bookmarks.move';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.move.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.moveBookmark_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var args = CEEE_json.decode(data.args);
  var id = args.id;
  var parentId = args.parentId;

  if (parentId == null) {
    parentId = CEEE_bookmarks.getFolderIdForItem(id);
  }

  if (parentId == null) {
    return;
  }

  var index = args.index;
  if (!index) {
    index = CEEE_bookmarks.DEFAULT_INDEX;
  }

  CEEE_bookmarks.moveItem(id, parentId, index);
}

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.update.
 */
CEEE_bookmarks_internal_.CMD_SET_BOOKMARK_TITLE = 'bookmarks.update';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.update.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.setBookmarkTitle_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var args = CEEE_json.decode(data.args);
  if (!args.id) {
    return;
  }

  CEEE_bookmarks.setItemTitle(args.id, args.title);
  return this.getBookmarkItemFromId_(args.id);
}

/**
 * Command name for Chrome extension bookmarks API function,
 * chrome.bookmarks.get.
 */
CEEE_bookmarks_internal_.CMD_GET_BOOKMARKS = 'bookmarks.get';

/**
  * Implementation of Chrome extension bookmarks API function,
  * chrome.bookmarks.get.
  * See
  * http://dev.chromium.org/developers/design-documents/extensions/bookmarks-api
  */
CEEE_bookmarks_internal_.getBookmarks_ = function(cmd, data) {
  if (!data || !data.args) {
    return;
  }

  var args = CEEE_json.decode(data.args);
  var bookmarks = [];
  if (!args) {
    bookmarks.push(this.getBookmarkItemFromId_(
        CEEE_bookmarks.placesRoot));
  } else {
    for (var bookmarkId = 0; bookmarkId < args.length; ++bookmarkId) {
      bookmarks.push(this.getBookmarkItemFromId_(
          args[bookmarkId]));
    }
  }

  return bookmarks;
}

/**
  * Initialization routine for the CEEE Bookmark API module.
  * @param {!Object} ceeeInstance Reference to the global ceee instance.
  * @return {Object} Reference to the bookmark module.
  * @public
  */
function CEEE_initialize_bookmarks(ceeeInstance) {
  CEEE_bookmarks_internal_.ceeeInstance_ = ceeeInstance;
  var bookmarks = CEEE_bookmarks_internal_;

  ceeeInstance.registerExtensionHandler(bookmarks.CMD_GET_BOOKMARK_CHILDREN,
                                        bookmarks,
                                        bookmarks.getBookmarkChildren_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_GET_BOOKMARK_TREE,
                                        bookmarks,
                                        bookmarks.getBookmarkTree_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_SEARCH_BOOKMARKS,
                                        bookmarks,
                                        bookmarks.searchBookmarks_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_REMOVE_BOOKMARK,
                                        bookmarks,
                                        bookmarks.removeBookmark_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_REMOVE_BOOKMARK_TREE,
                                        bookmarks,
                                        bookmarks.removeBookmarkTree_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_CREATE_BOOKMARK,
                                        bookmarks,
                                        bookmarks.createBookmark_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_MOVE_BOOKMARK,
                                        bookmarks,
                                        bookmarks.moveBookmark_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_SET_BOOKMARK_TITLE,
                                        bookmarks,
                                        bookmarks.setBookmarkTitle_);
  ceeeInstance.registerExtensionHandler(bookmarks.CMD_GET_BOOKMARKS,
                                        bookmarks,
                                        bookmarks.getBookmarks_);

  bookmarks.logInfo_ = ceeeInstance.logInfo;
  bookmarks.logError_ = ceeeInstance.logError;

  return bookmarks;
};
