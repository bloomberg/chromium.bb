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
   * The maximum number of tiles that we will display on this page. If there
   * are not enough spaces to show all bookmarks, we'll include a link to the
   * Bookmarks manager.
   * TODO(csilv): Eliminate the need for this restraint.
   * @type {number}
   * @const
   */
  var MAX_BOOKMARK_TILES = 18;

  /**
   * The root node's ID. We need this to determine removable items (direct
   * children aren't removable).
   * @type {number}
   * @const
   */
  var ROOT_NODE_ID = '0';

  /**
   * The bookmark bar's ID. We need this to allow combining the root and
   * bookmarks bars at the top level of the folder hierarchy/bookmarks page.
   * aren't removable).
   * @type {number}
   * @const
   */
  var BOOKMARKS_BAR_ID = '1';

  /**
   * Creates a new bookmark object.
   * @param {Object} data The url and title.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function Bookmark(data) {
    var el = $('bookmark-template').cloneNode(true);
    el.__proto__ = Bookmark.prototype;
    el.initialize(data);

    return el;
  }

  Bookmark.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialize the bookmark object.
     * @param {Object} data The bookmark data (url, title, etc).
     */
    initialize: function(data) {
      this.data = data;
      this.hidden = false;

      var id = tileId++;
      this.id = 'bookmark_tile_' + id;

      var title = this.querySelector('.title');
      title.textContent = this.data.title;

      // Sets the tooltip.
      this.title = this.data.title;

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
     * Set some data on the drag when it starts.
     * @param {DataTransfer} dataTransfer A data transfer object from the
     * current drag.
     */
    setDragData: function(dataTransfer) {
      // OS X requires extra data drag data to consider a drag useful, so we're
      // appending some semi-useful data at the end to force drags to work on
      // this OS. Don't use this data for anything -- this is just a hack to
      // ensure drag works the same as on other platforms.
      dataTransfer.setData('Text', this.data.id);
    },

    /**
     * Invoked when a bookmark is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        e.preventDefault();
        this.removeFromChrome();
      } else if (!this.data.url) {
        chrome.send('getBookmarksData', [this.data.id]);
        e.preventDefault();
      }
    },

    /** @inheritDoc */
    removeFromChrome: function() {
      chrome.send('removeBookmark', [this.data.id]);
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
    minTileWidth: 64,
    // The biggest a tile can be.
    maxTileWidth: 96,

    // The padding between tiles, as a fraction of the tile width.
    tileSpacingFraction: 1 / 2,
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
      var titleWrapper = $('bookmarks-title-wrapper')
      titleWrapper.hidden = false;
      this.insertBefore(titleWrapper, this.firstChild);

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

      var folder_id = this.id == ROOT_NODE_ID ? BOOKMARKS_BAR_ID : this.id;
      var top_link = $('bookmarks-top-link-wrapper').querySelector('a');
      top_link.href = 'chrome://bookmarks/#' + folder_id;

      var wrapper = $('bookmarks-bottom-link-wrapper');
      if (items.length > MAX_BOOKMARK_TILES) {
        var bottom_link = wrapper.querySelector('a');
        bottom_link.href = 'chrome://bookmarks/#' + folder_id;
        wrapper.hidden = false;
      } else {
        wrapper.hidden = true;
      }

      if (this.id === ROOT_NODE_ID && !tile_count && !cr.isChromeOS)
        this.showImportPromo_();
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
        if (bookmarkTitle.folderId == id)
          return true;
      }
      return false;
    },

    /**
     * Tells if we're in currently in the given folder.
     * @param {String} id The folder node's ID.
     * @returns {boolean} If it's in that folder (visually).
     */
    currentlyInFolder_: function(id) {
      return id === this.id || (this.id === ROOT_NODE_ID &&
                                id === BOOKMARKS_BAR_ID);
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(e) {
      var tile = ntp4.getCurrentlyDraggingTile();
      if (tile)
        return !!tile.querySelector('.most-visited, .bookmark');
      // If there was no dragging tile, look for a URL in the drag data.
      return e.dataTransfer && e.dataTransfer.types &&
             e.dataTransfer.types.indexOf('url') != -1;
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
     * @param {boolean} fromCurrentPage True if the action was from this page.
     */
    bookmarkNodeAdded: function(id, bookmark, fromCurrentPage) {
      if (this.importing) return;
      if (this.currentlyInFolder_(bookmark.parentId)) {
        // Hide the import promo if it exists.
        this.hideImportPromo_();
        // Only add the item if it should be visible.
        if (bookmark.index < MAX_BOOKMARK_TILES) {
          // If source of the add came from this page, show an animated
          // insertion, otherwise just quietly do it.
          this.addTileAt(new Bookmark(bookmark), bookmark.index,
                         fromCurrentPage);
          // Delete extra tiles if they exist.
          while (this.tiles.length > MAX_BOOKMARK_TILES) {
            var tile = this.tiles[this.tiles.length - 1];
            this.removeTile(tile, false);
          }
          this.repositionTiles_();
        }
      }
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
      if (this.currentlyInFolder_(id))
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
      if (this.currentlyInFolder_(moveInfo.parentId) ||
          this.currentlyInFolder_(moveInfo.oldParentId)) {
        chrome.send('getBookmarksData', [this.id]);
      }
    },

    /**
     * Invoked when a node has been removed from a folder.
     * @param {string} id The id of the removed bookmark node.
     * @param {!Object} removeInfo Details of the removed bookmark node.
     * @param {boolearn} fromCurrentPage If the event was from this page.
     */
    bookmarkNodeRemoved: function(id, removeInfo, fromCurrentPage) {
      // If the target item is the visibile folder or a parent folder, load
      // the parent of the removed item.
      if (this.isBookmarkInParentHierarchy_(id)) {
        chrome.send('getBookmarksData', [removeInfo.parentId]);
        return;
      }

      // If the target item is contained in the visible folder, find the
      // matching tile and delete it.
      if (this.currentlyInFolder_(removeInfo.parentId)) {
        for (var i = 0; i < this.tiles.length; i++) {
          var tile = this.tiles[i];
          if (tile.firstChild.data.id == id) {
            this.removeTile(tile, fromCurrentPage);
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

    /** @inheritDoc */
    setDropEffect: function(dataTransfer) {
      var tile = ntp4.getCurrentlyDraggingTile();
      if (tile && tile.querySelector('.bookmark'))
        ntp4.setCurrentDropEffect(dataTransfer, 'move');
      else
        ntp4.setCurrentDropEffect(dataTransfer, 'copy');
    },

    /** @inheritDoc */
    addDragData: function(dataTransfer, index) {
      var parentId = ROOT_NODE_ID == this.id ? BOOKMARKS_BAR_ID : this.id;
      var currentlyDraggingTile = ntp4.getCurrentlyDraggingTile();
      if (currentlyDraggingTile) {
        var tileContents = currentlyDraggingTile.firstChild;
        if (tileContents.classList.contains('most-visited')) {
          this.generateBookmarkForLink(parentId, index,
                                       tileContents.textContent,
                                       tileContents.href);
        }
      } else {
        this.addOutsideData_(dataTransfer, parentId, index);
      }
    },

    /**
     * If we got an outside drag from a page or the URL bar.
     * @param {!DataTransfer} dataTransfer The drag data transfer object.
     * @param {number} parentId The parent ID for the current bookmark level.
     * @param {number} index Position it will be inserted relative to siblings.
     */
    addOutsideData_: function(dataTransfer, parentId, index) {
      var url = dataTransfer.getData('url');
      assert(url && url !== window.location.href);
      // Look for any text/html types first (links, etc.).
      var title, html = dataTransfer.getData('text/html');
      if (html) {
        // NOTE: Don't insert this into the DOM! It could XSS the page!
        var node = this.ownerDocument.createElement('div');
        node.innerHTML = html;
        var text = node.textContent, img = node.firstChild;
        // OS X prepends a <meta> tag to the html for some reason...
        if ('META' == img.nodeName)
          img = img.nextSibling;
        // Use the combined text (if it exists), otherwise fall back to an
        // image's alternate text (if they dragged an image onto this page).
        title = text || ('IMG' == img.nodeName && img.alt);
      }
      // If we *still* don't have a title, just use the URL.
      if (!title)
        title = url;
      // Create a bookmark for the dropped data.
      this.generateBookmarkForLink(parentId, index, title, url);
    },

    /**
     * Show the 'Import bookmarks' promo.
     * @private
     */
    showImportPromo_: function() {
      var importTemplate = $('bookmarks-import-data-link-template');
      var importWrapper = importTemplate.cloneNode(true);
      importWrapper.id = '';
      importWrapper.hidden = false;
      this.querySelector('.tile-page-content').appendChild(importWrapper);
    },

    /**
     * Hide the 'Import bookmarks' promo.
     * @private
     */
    hideImportPromo_: function() {
      var wrapper = this.querySelector('.bookmarks-import-data-link-wrapper');
      if (wrapper)
        wrapper.parentNode.removeChild(wrapper);
    },

    /**
     * Create a bookmark from a title/url.
     * @param {!string} parentId Stringified int64 of the parent node's ID.
     * @param {number} index Sibling relative index, i.e. 3rd on this level.
     * @param {string} title More human readable title of the bookmark.
     * @param {string} url URL of the bookmark to be created.
     */
    generateBookmarkForLink: function(parentId, index, title, url) {
      // Bookmark creation actually only *requires* a parent ID, as if we just
      // pass a parent ID it's created as a folder with blank title as a child
      // of that parent.
      assert(parentId);
      chrome.send('createBookmark', [parentId, index, title, url]);
    },

    /** @inheritDoc */
    tileMoved: function(tile, prevIndex) {
      var parentId = ROOT_NODE_ID == this.id ? BOOKMARKS_BAR_ID : this.id;
      chrome.send('moveBookmark', [tile.firstChild.data.id, parentId,
                                   tile.index + (prevIndex < tile.index)]);
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
  }

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
  }

  return {
    BookmarksPage: BookmarksPage,
    initBookmarkChevron: initBookmarkChevron,
  };
});

window.addEventListener('load', ntp4.initBookmarkChevron);
