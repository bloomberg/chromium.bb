// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('bmm', function() {
  const List = cr.ui.List;

  var listLookup = {};

  /**
   * Creates a new bookmark list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLButtonElement}
   */
  var BookmarkList = cr.ui.define('list');

  BookmarkList.prototype = {
    __proto__: List.prototype,

    decorate: function() {
      List.prototype.decorate.call(this);
      this.addEventListener('click', this.handleClick_);
    },

    parentId_: '',
    get parentId() {
      return this.parentId_;
    },
    set parentId(parentId) {
      if (this.parentId_ == parentId)
        return;

      var oldParentId = this.parentId_;
      this.parentId_ = parentId;

      var callback = cr.bind(this.handleBookmarkCallback, this);

      if (!parentId) {
        callback([]);
      } else if (/^q=/.test(parentId)) {
        chrome.bookmarks.search(parentId.slice(2), callback);
      } else if (parentId == 'recent') {
        chrome.bookmarks.getRecent(50, callback);
      } else {
        chrome.bookmarks.getChildren(parentId, callback);
      }

      cr.dispatchPropertyChange(this, 'parentId', parentId, oldParentId);
    },

    handleBookmarkCallback: function(items) {
      if (!items) {
        // Failed to load bookmarks. Most likely due to the bookmark beeing
        // removed.
        cr.dispatchSimpleEvent(this, 'invalidId');
        return;
      }
      listLookup = {};
      this.clear();
      var showFolder = this.showFolder();
      items.forEach(function(item) {
        var li = createListItem(item, showFolder);
        this.add(li);
      }, this);
      cr.dispatchSimpleEvent(this, 'load');
    },

    /**
     * The bookmark node that the list is currently displaying. If we are currently
     * displaying recent or search this returns null.
     * @type {BookmarkTreeNode}
     */
    get bookmarkNode() {
      if (this.isSearch() || this.isRecent())
        return null;
      var treeItem = bmm.treeLookup[this.parentId];
      return treeItem && treeItem.bookmarkNode;
    },

    showFolder: function() {
      return this.isSearch() || this.isRecent();
    },

    isSearch: function() {
      return this.parentId_[0] == 'q';
    },

    isRecent: function() {
      return this.parentId_ == 'recent';
    },

    /**
     * Handles the clicks on the list so that we can check if the user clicked
     * on a link or an folder.
     * @private
     * @param {Event} e The click event object.
     */
    handleClick_: function(e) {

      var el = e.target;
      if (el.href) {
        var event = this.ownerDocument.createEvent('Event');
        event.initEvent('urlClicked', true, false);
        event.url = el.href;
        event.kind = e.shiftKey ? 'window' : e.button == 1 ? 'tab' : 'self';
        this.dispatchEvent(event);
      }
    },

    // Bookmark model update callbacks
    handleBookmarkChanged: function(id, changeInfo) {
      var listItem = listLookup[id];
      if (listItem) {
        listItem.bookmarkNode.title = changeInfo.title;
        updateListItem(listItem, listItem.bookmarkNode, list.showFolder());
      }
    },

    handleChildrenReordered: function(id, reorderInfo) {
      if (this.parentId == id) {
        var self = this;
        reorderInfo.childIds.forEach(function(id, i) {
          var li = listLookup[id]
          self.addAt(li, i);
          // At this point we do not read the index from the bookmark node so we
          // do not need to update it.
          li.bookmarkNode.index = i;
        });
      }
    },

    handleCreated: function(id, bookmarkNode) {
      if (this.parentId == bookmarkNode.parentId) {
        var li = createListItem(bookmarkNode, false);
        this.addAt(li, bookmarkNode.index);
      }
    },

    handleMoved: function(id, moveInfo) {
      if (moveInfo.parentId == this.parentId ||
          moveInfo.oldParentId == this.parentId) {

        if (moveInfo.oldParentId == moveInfo.parentId) {
          var listItem = listLookup[id];
          if (listItem) {
            this.remove(listItem);
            this.addAt(listItem, moveInfo.index);
          }
        } else {
          if (moveInfo.oldParentId == this.parentId) {
            var listItem = listLookup[id];
            if (listItem) {
              this.remove(listItem);
              delete listLookup[id];
            }
          }

          if (moveInfo.parentId == list.parentId) {
            var self = this;
            chrome.bookmarks.get(id, function(bookmarkNodes) {
              var bookmarkNode = bookmarkNodes[0];
              var li = createListItem(bookmarkNode, false);
              self.addAt(li, bookmarkNode.index);
            });
          }
        }
      }
    },

    handleRemoved: function(id, removeInfo) {
      var listItem = listLookup[id];
      if (listItem) {
        this.remove(listItem);
        delete listLookup[id];
      }
    }
  };

  /**
   * The contextMenu property.
   * @type {cr.ui.Menu}
   */
  cr.ui.contextMenuHandler.addContextMenuProperty(BookmarkList);

  function createListItem(bookmarkNode, showFolder) {
    var li = listItemPromo.cloneNode(true);
    ListItem.decorate(li);
    updateListItem(li, bookmarkNode, showFolder);
    li.bookmarkId = bookmarkNode.id;
    li.bookmarkNode = bookmarkNode;
    li.draggable = true;
    listLookup[bookmarkNode.id] = li;
    return li;
  }

  function updateListItem(el, bookmarkNode, showFolder) {
    var labelEl = el.firstChild;
    const NBSP = '\u00a0';
    labelEl.textContent = bookmarkNode.title || NBSP;
    if (bookmarkNode.url) {
      labelEl.style.backgroundImage = url('chrome://favicon/' +
                                          bookmarkNode.url);
      var urlEl = el.childNodes[1].firstChild;
      urlEl.textContent = urlEl.href = bookmarkNode.url;
    } else {
      el.className = 'folder';
    }

    var folderEl = el.lastChild.firstChild;
    if (showFolder) {
      folderEl.style.display = '';
      folderEl.textContent = getFolder(bookmarkNode.parentId);
      folderEl.href = '#' + bookmarkNode.parentId;
    } else {
      folderEl.style.display = 'none';
    }
  }

  var listItemPromo = (function() {
    var div = cr.doc.createElement('div');
    div.innerHTML = '<div>' +
        '<div class=label></div>' +
        '<div><span class=url></span></div>' +
        '<div><span class=folder></span></div>' +
        '</div>';
    return div.firstChild;
  })();

  return {
    BookmarkList: BookmarkList,
    listLookup: listLookup
  };
});
