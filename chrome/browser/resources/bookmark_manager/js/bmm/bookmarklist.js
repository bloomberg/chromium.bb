// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('bmm', function() {
  // require cr.ui.define
  // require cr.ui.limitInputWidth.
  // require cr.ui.contextMenuHandler
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  var listLookup = {};

  /**
   * Removes all children and appends a new child.
   * @param {!Node} parent The node to remove all children from.
   * @param {!Node} newChild The new child to append.
   */
  function replaceAllChildren(parent, newChild) {
    var n;
    while ((n = parent.lastChild)) {
      parent.removeChild(n);
    }
    parent.appendChild(newChild);
  }

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
      // HACK(arv): http://crbug.com/40902
      var self = this;
      window.addEventListener('resize', function() {
        self.fixWidth_();
      });
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
      this.loading_ = true;

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
        this.loading_ = false;
        return;
      }
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in listLookup){
        delete listLookup[id];
      }
      this.clear();
      try {
        this.startBatchAdd();
        items.forEach(function(item) {
          var li = createListItem(item);
          this.add(li);
        }, this);
      } finally {
        this.finishBatchAdd();
      }

      this.loading_ = false;
      this.fixWidth_();
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

    isSearch: function() {
      return this.parentId_[0] == 'q';
    },

    isRecent: function() {
      return this.parentId_ == 'recent';
    },

    /** @inheritDoc */
    addAt: function(item, index) {
      // Override to work around list width bug in flex box.
      List.prototype.addAt.call(this, item, index);
      this.fixWidth_();
    },

    /** @inheritDoc */
    remove: function(item) {
      // Override to work around list width bug in flex box.
      List.prototype.remove.call(this, item);
      this.fixWidth_();
    },

    /** @inheritDoc */
    clear: function() {
      // Override to work around list width bug in flex box.
      List.prototype.clear.call(this);
      this.fixWidth_();
    },

    /**
     * Handles the clicks on the list so that we can check if the user clicked
     * on a link or a folder.
     * @private
     * @param {Event} e The click event object.
     */
    handleClick_: function(e) {
      var self = this;

      function dispatch(url) {
        var event = new cr.Event('urlClicked', true, false);
        event.url = url;
        event.originalEvent = e;
        self.dispatchEvent(event);
      }

      var el = e.target;

      // Handle clicks on the links to URLs.
      if (el.href) {
        dispatch(el.href);

      // Handle middle click to open bookmark in a new tab.
      } else if (e.button == 1) {
        while (el.parentNode != this) {
          el = el.parentNode;
        }
        var node = el.bookmarkNode;
        if (!bmm.isFolder(node))
          dispatch(node.url);
      }
    },

    // Bookmark model update callbacks
    handleBookmarkChanged: function(id, changeInfo) {
      var listItem = listLookup[id];
      if (listItem) {
        listItem.bookmarkNode.title = changeInfo.title;
        if ('url' in changeInfo)
          listItem.bookmarkNode.url = changeInfo['url'];
        updateListItem(listItem, listItem.bookmarkNode);
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
    },

    /**
     * Workaround for http://crbug.com/40902
     * @private
     */
    fixWidth_: function() {
      if (this.loading_)
        return;

      // The width of the list is wrong after its content has changed.
      // Fortunately the reported offsetWidth is correct so we can detect the
      //incorrect width.
      if (list.offsetWidth != list.parentNode.clientWidth - list.offsetLeft) {
        // Set the width to the correct size. This causes the relayout.
        list.style.width = list.parentNode.clientWidth - list.offsetLeft + 'px';
        // Remove the temporary style.width in a timeout. Once the timer fires
        // the size should not change since we already fixed the width.
        window.setTimeout(function() {
          list.style.width = '';
        }, 0);
      }
    }
  };

  /**
   * The contextMenu property.
   * @type {cr.ui.Menu}
   */
  cr.ui.contextMenuHandler.addContextMenuProperty(BookmarkList);

  /**
   * Creates a new bookmark list item.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  var BookmarkListItem = cr.ui.define('li');

  BookmarkListItem.prototype = {
    __proto__: ListItem.prototype,
    /**
     * Whether the user is currently able to edit the list item.
     * @type {boolean}
     */
    get editing() {
      return this.hasAttribute('editing');
    },
    set editing(editing) {
      var oldEditing = this.editing;
      if (oldEditing == editing)
        return;

      var url = this.bookmarkNode.url;
      var title = this.bookmarkNode.title;
      var isFolder = bmm.isFolder(this.bookmarkNode);
      var listItem = this;
      var labelEl = this.firstChild;
      var urlEl = this.lastChild;
      var labelInput, urlInput;

      // Handles enter and escape which trigger reset and commit respectively.
      function handleKeydown(e) {
        // Make sure that the tree does not handle the key.
        e.stopPropagation();

        // Calling list.focus blurs the input which will stop editing the list
        // item.
        switch (e.keyIdentifier) {
          case 'U+001B':  // Esc
            labelInput.value = title;
            if (!isFolder)
              urlInput.value = url;
            // fall through
            cr.dispatchSimpleEvent(listItem, 'canceledit', true);
          case 'Enter':
            if (listItem.parentNode)
              listItem.parentNode.focus();
        }
      }

      function handleBlur(e) {
        // When the blur event happens we do not know who is getting focus so we
        // delay this a bit since we want to know if the other input got focus
        // before deciding if we should exit edit mode.
        var doc = e.target.ownerDocument;
        window.setTimeout(function() {
          var activeElement = doc.activeElement;
          if (activeElement != urlInput && activeElement != labelInput) {
            listItem.editing = false;
          }
        }, 50);
      }

      var doc = this.ownerDocument;
      if (editing) {
        this.setAttribute('editing', '');
        this.draggable = false;

        labelInput = doc.createElement('input');
        labelInput.placeholder =
            localStrings.getString('name_input_placeholder');
        replaceAllChildren(labelEl, labelInput);
        labelInput.value = title;

        if (!isFolder) {
          // To use :invalid we need to put the input inside a form
          // https://bugs.webkit.org/show_bug.cgi?id=34733
          var form = doc.createElement('form');
          urlInput = doc.createElement('input');
          urlInput.type = 'url';
          urlInput.required = true;
          urlInput.placeholder =
              localStrings.getString('url_input_placeholder');

          // We also need a name for the input for the CSS to work.
          urlInput.name = '-url-input-' + cr.createUid();
          form.appendChild(urlInput);
          replaceAllChildren(urlEl, form);
          urlInput.value = url;
        }

        function stopPropagation(e) {
          e.stopPropagation();
        }

        var eventsToStop = ['mousedown', 'mouseup', 'contextmenu', 'dblclick'];
        eventsToStop.forEach(function(type) {
          labelInput.addEventListener(type, stopPropagation);
        });
        labelInput.addEventListener('keydown', handleKeydown);
        labelInput.addEventListener('blur', handleBlur);
        cr.ui.limitInputWidth(labelInput, this, 20);
        labelInput.focus();
        labelInput.select();

        if (!isFolder) {
          eventsToStop.forEach(function(type) {
            urlInput.addEventListener(type, stopPropagation);
          });
          urlInput.addEventListener('keydown', handleKeydown);
          urlInput.addEventListener('blur', handleBlur);
          cr.ui.limitInputWidth(urlInput, this, 20);
        }

      } else {

        // Check that we have a valid URL and if not we do not change the
        // editing mode.
        if (!isFolder) {
          var urlInput = this.querySelector('.url input');
          var newUrl = urlInput.value;
          if (!urlInput.validity.valid) {
            // WebKit does not do URL fix up so we manually test if prepending
            // 'http://' would make the URL valid.
            // https://bugs.webkit.org/show_bug.cgi?id=29235
            urlInput.value = 'http://' + newUrl;
            if (!urlInput.validity.valid) {
              // still invalid
              urlInput.value = newUrl;

              // In case the item was removed before getting here we should
              // not alert.
              if (listItem.parentNode) {
                alert(localStrings.getString('invalid_url'));
              }
              urlInput.focus();
              urlInput.select();
              return;
            }
            newUrl = 'http://' + newUrl;
          }
          urlEl.textContent = this.bookmarkNode.url = newUrl;
        }

        this.removeAttribute('editing');
        this.draggable = true;

        labelInput = this.querySelector('.label input');
        var newLabel = labelInput.value;
        labelEl.textContent = this.bookmarkNode.title = newLabel;

        if (isFolder) {
          if (newLabel != title) {
            cr.dispatchSimpleEvent(this, 'rename', true);
          }
        } else if (newLabel != title || newUrl != url) {
          cr.dispatchSimpleEvent(this, 'edit', true);
        }
      }
    }
  };

  function createListItem(bookmarkNode) {
    var li = listItemPromo.cloneNode(true);
    BookmarkListItem.decorate(li);
    updateListItem(li, bookmarkNode);
    li.bookmarkId = bookmarkNode.id;
    li.bookmarkNode = bookmarkNode;
    li.draggable = true;
    listLookup[bookmarkNode.id] = li;
    return li;
  }

  function updateListItem(el, bookmarkNode) {
    var labelEl = el.firstChild;
    labelEl.textContent = bookmarkNode.title;
    if (!bmm.isFolder(bookmarkNode)) {
      labelEl.style.backgroundImage = url('chrome://favicon/' +
                                          bookmarkNode.url);

      var urlEl = el.childNodes[1];
      labelEl.href = urlEl.textContent = bookmarkNode.url;
    } else {
      labelEl.href = '#' + bookmarkNode.id;
      el.className = 'folder';
    }
  }

  var listItemPromo = (function() {
    var div = cr.doc.createElement('div');
    div.innerHTML = '<span class=label></span><span class=url></span>';
    return div;
  })();

  return {
    createListItem: createListItem,
    BookmarkList: BookmarkList,
    listLookup: listLookup
  };
});
