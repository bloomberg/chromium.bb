// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var localStrings = new LocalStrings;

  /**
   * A running count of bookmark tiles that we create so that each will have
   * a unique ID.
   */
  var tileId = 0;

  /**
   * The maximum number of tiles that we will display on this page.  If there
   * are not enough spaces to show all bookmarks, we'll include a link to the
   * Bookmarks manager.
   * TODO(csilv): Eliminate the need for this restraint.
   * @type {number}
   * @const
   */
  var MAX_BOOKMARK_TILES = 18;

  /**
   * The root node ID. We use this to determine removable items (direct children
   * aren't removable).
   * @type {number}
   * @const
   */
  var ROOT_NODE_ID = "0";

  /**
   * Creates a new bookmark object.
   * @param {Object} data The url and title.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function Bookmark(data) {
    var el = $('bookmark-template').cloneNode(true);
    el.__proto__ = Bookmark.prototype;
    el.data = data;
    el.initialize();

    return el;
  }

  Bookmark.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialize the bookmark object.
     */
    initialize: function() {
      var id = tileId++;
      this.id = 'bookmark_tile_' + id;

      var title = this.querySelector('.title');
      title.textContent = this.data.title;

      if (this.data.url) {
        var button = this.querySelector('.button');
        button.href = title.href = this.data.url;
      }

      var faviconDiv = this.querySelector('.favicon');
      var faviconUrl;
      if (this.data.url) {
        faviconUrl = 'chrome://favicon/size/16/' + this.data.url;
        chrome.send('getFaviconDominantColor', [faviconUrl, this.id]);
      } else {
        faviconUrl = 'chrome://theme/IDR_BOOKMARK_BAR_FOLDER';
        // TODO(csilv): Should we vary this color by platform?
        this.stripeColor = '#919191';
      }
      faviconDiv.style.backgroundImage = url(faviconUrl);

      if (this.canBeRemoved())
        this.classList.add('removable');

      this.addEventListener('click', this.handleClick_.bind(this));
    },

    /**
     * Sets the color of the favicon dominant color bar.
     * @param {string} color The css-parsable value for the color.
     */
    set stripeColor(color) {
      this.querySelector('.color-stripe').style.backgroundColor = color;
    },

    /**
     * Set the size and position of the bookmark tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = size + 'px';
      this.style.height = heightForWidth(size) + 'px';

      this.style.left = x + 'px';
      this.style.right = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Invoked when a bookmark is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        this.handleDelete_();
        e.preventDefault();
      } else if (!this.data.url) {
        chrome.send('getBookmarksData', [this.data.id]);
        e.preventDefault();
      }
    },

    /**
     * Delete this bookmark from the data model.
     * @private
     */
    handleDelete_: function() {
      chrome.send('removeBookmark', [this.data.id]);
      this.parentNode.tilePage.removeTile(this.tile, true);
    },

    /** @inheritDoc */
    removeFromChrome: function() {
      this.handleDelete_();
    },

    /**
     * All bookmarks except for children of the root node.
     * @return {boolean} Whether or not the item can be removed.
     */
    canBeRemoved: function() {
      return this.data.parentId !== ROOT_NODE_ID;
    },
  };

  /**
   * Creates a new bookmark title object.
   * @param {Object} data The url and title.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function BookmarkTitle(data) {
    var el = cr.doc.createElement('div');
    el.__proto__ = BookmarkTitle.prototype;
    el.initialize(data);

    return el;
  }

  BookmarkTitle.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialize the bookmark title object.
     */
    initialize: function(data) {
      this.className = 'title-crumb';
      this.folderId = data.id;
      this.textContent = data.parentId ? data.title :
          localStrings.getString('bookmarksPage');

      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Invoked when a bookmark title is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      chrome.send('getBookmarksData', [this.folderId]);
    },
  };

  var TilePage = ntp4.TilePage;

  var bookmarksPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 3,
    // The most tiles we will show in a row.
    maxColCount: 6,

    // The smallest a tile can be.
    minTileWidth: 150,
    // The biggest a tile can be.
    maxTileWidth: 150,
  };
  TilePage.initGridValues(bookmarksPageGridValues);

  /**
   * Calculates the height for a bookmarks tile for a given width. The size
   * is based on a desired size of 96x72 ratio.
   * @return {number} The height.
   */
  function heightForWidth(width) {
    // The 2s are for borders, the 31 is for the title.
    return (width - 2) * 72 / 96 + 2 + 31;
  }

  /**
   * Creates a new BookmarksPage object.
   * @constructor
   * @extends {TilePage}
   */
  function BookmarksPage() {
    var el = new TilePage(bookmarksPageGridValues);
    el.__proto__ = BookmarksPage.prototype;
    el.initialize();

    return el;
  }

  BookmarksPage.prototype = {
    __proto__: TilePage.prototype,

    /**
     * Initialize the bookmarks page object.
     */
    initialize: function() {
      this.classList.add('bookmarks-page');

      // Insert the bookmark titles header which is unique to bookmark pages.
      this.insertBefore($('bookmarks-title-wrapper'), this.firstChild);

      // Insert the top & bottom links for the Bookmarks Manager page.
      var pageContent = this.querySelector('.tile-page-content');
      var topWrapper = $('bookmarks-top-link-wrapper');
      pageContent.insertBefore(topWrapper, pageContent.firstChild);
      topWrapper.hidden = false;
      pageContent.appendChild($('bookmarks-bottom-link-wrapper'));
    },

    /**
     * Build the bookmark titles bar (ie, navigation hiearchy).
     * @param {Array} items The parent hiearchy of the current folder.
     * @private
     */
    updateBookmarkTitles_: function(items) {
      var wrapper = $('bookmarks-title-wrapper');
      var title = wrapper.querySelector('.section-title');
      title.innerHTML = '';

      for (var i = items.length - 1; i > 0; i--) {
        title.appendChild(new BookmarkTitle(items[i]));

        var separator = document.createElement('hr');
        separator.className = 'bookmark-separator';
        title.appendChild(separator);
      }

      var titleCrumb = new BookmarkTitle(items[0]);
      titleCrumb.classList.add('title-crumb-active');
      title.appendChild(titleCrumb);
    },

    /**
     * Build the bookmark tiles.
     * @param {Array} items The contents of the current folder.
     * @private
     */
    updateBookmarkTiles_: function(items) {
      this.removeAllTiles();
      var tile_count = Math.min(items.length, MAX_BOOKMARK_TILES);
      for (var i = 0; i < tile_count; i++)
        this.appendTile(new Bookmark(items[i]), false);

      var link = $('bookmarks-top-link-wrapper').querySelector('a');
      link.href = 'chrome://bookmarks/#' + this.id;

      var wrapper = $('bookmarks-bottom-link-wrapper');
      if (items.length > MAX_BOOKMARK_TILES) {
        var link = wrapper.querySelector('a');
        link.href = 'chrome://bookmarks/#' + this.id;
        wrapper.hidden = false;
      } else {
        wrapper.hidden = true;
      }
    },

    /**
     * Determine whether a bookmark ID matches a folder in the current
     * hierarchy.
     * @param {string} id The bookmark ID to search for.
     * @private
     */
    isBookmarkInParentHierarchy_: function(id) {
      var titlesWrapper = $('bookmarks-title-wrapper');
      var titles = titlesWrapper.querySelectorAll('.title-crumb');
      for (var i = 0; i < titles.length; i++) {
        var bookmarkTitle = titles[i];
        if (bookmarkTitle.folderId == id) {
          return true;
        }
      }
      return false;
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(dataTransfer) {
      return false;
    },

    /** @inheritDoc */
    heightForWidth: heightForWidth,

    /**
     * Invoked before a batch import begins.  We will ignore added/changed
     * notifications while the operation is in progress.
     */
    bookmarkImportBegan: function() {
      this.importing = true;
    },

    /**
     * Invoked after a batch import finishes.  We will reload the bookmarks
     * page to reflect the new state.
     */
    bookmarkImportEnded: function() {
      this.importing = false;
      chrome.send('getBookmarksData', []);
    },

    /**
     * Invoked when a node has been added.
     * @param {string} id The id of the newly created bookmark node.
     * @param {Object} bookmark The new bookmark node.
     */
    bookmarkNodeAdded: function(id, bookmark) {
      if (this.importing) return;
      if (this.id == bookmark.parentId)
        this.addTileAt(new Bookmark(bookmark), bookmark.index, false);
    },

    /**
     * Invoked when the title or url of a node changes.
     * @param {string} id The id of the changed node.
     * @param {Object} changeInfo Details of the changed node.
     */
    bookmarkNodeChanged: function(id, changeInfo) {
      if (this.importing) return;

      // If the current folder or parent is being re-named, reload the page.
      // TODO(csilv): Optimize this to reload just the titles.
      if (this.isBookmarkInParentHierarchy_(id)) {
        chrome.send('getBookmarksData', [this.id]);
        return;
      }

      // If the target item is contained in this folder, update just that item.
      for (var i = 0; i < this.tiles.length; i++) {
        var tile = this.tiles[i];
        var data = tile.firstChild.data;

        if (data.id == id) {
          data.title = changeInfo.title;
          var title = tile.querySelector('.title');
          title.textContent = data.title;

          if (changeInfo.url) {
            data.url = changeInfo.url;
            var button = tile.querySelector('.button');
            button.href = title.href = data.url;
          }
          break;
        }
      }
    },

    /**
     * Invoked when the children (just direct children, not descendants) of
     * a folder have been reordered in some way, such as sorted.
     * @param {string} id The id of the reordered bookmark node.
     * @param {!Object} reorderInfo Details of the reordered bookmark node.
     */
    bookmarkNodeChildrenReordered: function(id, reorderInfo) {
      if (this.id == id)
        chrome.send('getBookmarksData', [this.id]);
    },

    /**
     * Invoked when a node has moved.
     * @param {string} id The id of the moved bookmark node.
     * @param {!Object} moveInfo Details of the moved bookmark.
     */
    bookmarkNodeMoved: function(id, moveInfo) {
      // TODO(csilv): Optimize this by doing less than reloading the folder.
      // Reload the current page if the target item is the current folder
      // or a parent of the current folder.
      if (this.isBookmarkInParentHierarchy_(id)) {
        chrome.send('getBookmarksData', [this.id]);
        return;
      }

      // Reload the current page if the target item is being moved to/from the
      // current folder.
      if (this.id == moveInfo.parentId ||
          this.id == moveInfo.oldParentId) {
        chrome.send('getBookmarksData', [this.id]);
      }
    },

    /**
     * Invoked when a node has been removed from a folder.
     * @param {string} id The id of the removed bookmark node.
     * @param {!Object} removeInfo Details of the removed bookmark node.
     */
    bookmarkNodeRemoved: function(id, removeInfo) {
      // If the target item is the visibile folder or a parent folder, load
      // the parent of the removed item.
      if (this.isBookmarkInParentHierarchy_(id)) {
        chrome.send('getBookmarksData', [removeInfo.parentId]);
        return;
      }

      // If the target item is contained in the visible folder, find the
      // matching tile and delete it.
      if (this.id == removeInfo.parentId) {
        for (var i = 0; i < this.tiles.length; i++) {
          var tile = this.tiles[i];
          if (tile.firstChild.data.id == id) {
            this.removeTile(tile, false);
            break;
          }
        }
      }
    },

    /**
     * Set the bookmark data that should be displayed, replacing any existing
     * data.
     * @param {Object} data Data that shoudl be displayed. Contains arrays
     *   'items' and 'navigationItems'.
     */
    set data(data) {
      this.id = data.navigationItems[0].id;
      this.updateBookmarkTiles_(data.items);
      this.updateBookmarkTitles_(data.navigationItems);
    },

    /** @inheritDoc */
    get extraBottomPadding() {
      return 40;
    },
  };

  /**
   * Initializes and renders the bookmark chevron canvas.  This needs to be
   * performed after the page has been loaded so that we have access to the
   * style sheet values.
   */
  function initBookmarkChevron() {
    var wrapperStyle = window.getComputedStyle($('bookmarks-title-wrapper'));
    var width = 10;
    var height = parseInt(wrapperStyle.height, 10);
    var ctx = document.getCSSCanvasContext('2d', 'bookmark-chevron',
                                           width, height);
    ctx.strokeStyle = wrapperStyle.borderBottomColor;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(width, height / 2);
    ctx.lineTo(0, height);
    ctx.stroke();
  };

  /**
   * Set the dominant color for a bookmark tile.  This is the callback method
   * from a request made when the tile was created.
   * @param {string} id The ID of the bookmark tile.
   * @param {string} color The color represented as a CSS string.
   */
  function setBookmarksFaviconDominantColor(id, color) {
    var tile = $(id);
    if (tile)
      tile.stripeColor = color;
  };

  return {
    BookmarksPage: BookmarksPage,
    initBookmarkChevron: initBookmarkChevron,
  };
});

window.addEventListener('load', ntp4.initBookmarkChevron);
