// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
/**
 * The element used to vertically position the most visited section on
 * window resize.
 * @type {Element}
 */
var topMarginElement;

/**
 * The container for the tile elements.
 * @type {Element}
 */
var tilesContainer;

/**
 * The notification displayed when a page is blacklisted.
 * @type {Element}
 */
var notification;

/**
 * The container for the theme attribution.
 * @type {Element}
 */
var attribution;

/**
 * The array of rendered tiles, ordered by appearance.
 * @type {Array.<Tile>}
 */
var tiles = [];

/**
 * The last blacklisted tile if any, which by definition should not be filler.
 * @type {?Tile}
 */
var lastBlacklistedTile = null;

/**
 * The index of the last blacklisted tile, if any. Used to determine where to
 * re-insert a tile on undo.
 * @type {number}
 */
var lastBlacklistedIndex = -1;

/**
 * True if a page has been blacklisted and we're waiting on the
 * onmostvisitedchange callback. See onMostVisitedChange() for how this
 * is used.
 * @type {boolean}
 */
var isBlacklisting = false;

/**
 * True if a blacklist has been undone and we're waiting on the
 * onmostvisitedchange callback. See onMostVisitedChange() for how this
 * is used.
 * @type {boolean}
 */
var isUndoing = false;

/**
 * Current number of tiles shown based on the window width, including filler.
 * @type {number}
 */
var numTilesShown = 0;

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var apiHandle;

/**
 * Possible background-colors of a non-custom theme. Used to determine whether
 * the homepage should be updated to support custom or non-custom themes.
 * @type {!Array.<string>}
 * @const
 */
var WHITE = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];

/**
 * Should be equal to mv-tile's -webkit-margin-start + width.
 * @type {number}
 * @const
 */
var TILE_WIDTH = 160;

/**
 * The height of the most visited section.
 * @type {number}
 * @const
 */
var MOST_VISITED_HEIGHT = 156;

/** @type {number} @const */
var MAX_NUM_TILES_TO_SHOW = 4;

/** @type {number} @const */
var MIN_NUM_TILES_TO_SHOW = 2;

/**
 * Minimum total padding to give to the left and right of the most visited
 * section. Used to determine how many tiles to show.
 * @type {number}
 * @const
 */
var MIN_TOTAL_HORIZONTAL_PADDING = 188;

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  BLACKLIST: 'mv-blacklist',  // triggers tile blacklist animation
  BLACKLIST_BUTTON: 'mv-x',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  DOMAIN: 'mv-domain',
  FAVICON: 'mv-favicon',
  FILLER: 'mv-filler',  // filler tiles
  HIDE_BLACKLIST_BUTTON: 'mv-x-hide', // hides blacklist button during animation
  HIDE_NOTIFICATION: 'mv-notice-hide',
  HIDE_TILE: 'mv-tile-hide', // hides tiles on small browser width
  PAGE: 'mv-page',  // page tiles
  THUMBNAIL: 'mv-thumb',
  TILE: 'mv-tile',
  TITLE: 'mv-title'
};

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
var IDS = {
  ATTRIBUTION: 'attribution',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  RESTORE_ALL_LINK: 'mv-restore',
  TILES: 'mv-tiles',
  TOP_MARGIN: 'mv-top-margin',
  UNDO_LINK: 'mv-undo'
};

/**
 * A Tile is either a rendering of a Most Visited page or "filler" used to
 * pad out the section when not enough pages exist.
 *
 * @param {Element} elem The element for rendering the tile.
 * @param {number=} opt_rid The RID for the corresponding Most Visited page.
 *     Should only be left unspecified when creating a filler tile.
 * @constructor
 */
function Tile(elem, opt_rid) {
  /** @type {Element} */
  this.elem = elem;

  /** @type {number|undefined} */
  this.rid = opt_rid;
}

/**
 * Updates the NTP based on the current theme.
 * @private
 */
function onThemeChange() {
  var info = apiHandle.themeBackgroundInfo;
  if (!info)
    return;
  var background = [info.colorRgba,
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();
  document.body.style.background = background;
  var isCustom = !!background && WHITE.indexOf(background) == -1;
  document.body.classList.toggle('custom-theme', isCustom);
  updateAttribution(info.attributionUrl);
}

/**
 * Renders the attribution if the image is present and loadable.  Otherwise
 * hides it.
 * @param {string} url The URL of the attribution image, if any.
 * @private
 */
function updateAttribution(url) {
  if (!url) {
    attribution.hidden = true;
    return;
  }
  var attributionImage = new Image();
  attributionImage.onload = function() {
    var oldAttributionImage = attribution.querySelector('img');
    if (oldAttributionImage)
      removeNode(oldAttributionImage);
    attribution.appendChild(attributionImage);
    attribution.hidden = false;
  };
  attributionImage.onerror = function() {
    attribution.hidden = true;
  };
  attributionImage.src = url;
}

/**
 * Handles a new set of Most Visited page data.
 */
function onMostVisitedChange() {
  var pages = apiHandle.mostVisited;

  if (isBlacklisting) {
    // If this was called as a result of a blacklist, add a new replacement
    // (possibly filler) tile at the end and trigger the blacklist animation.
    var replacementTile = createTile(pages[MAX_NUM_TILES_TO_SHOW - 1]);

    tiles.push(replacementTile);
    tilesContainer.appendChild(replacementTile.elem);

    var lastBlacklistedTileElement = lastBlacklistedTile.elem;
    lastBlacklistedTileElement.addEventListener(
        'webkitTransitionEnd', blacklistAnimationDone);
    lastBlacklistedTileElement.classList.add(CLASSES.BLACKLIST);
    // In order to animate the replacement tile sliding into place, it must
    // be made visible.
    updateTileVisibility(numTilesShown + 1);

  } else if (isUndoing) {
    // If this was called as a result of an undo, re-insert the last blacklisted
    // tile in its old location and trigger the undo animation.
    tiles.splice(
        lastBlacklistedIndex, 0, lastBlacklistedTile);
    var lastBlacklistedTileElement = lastBlacklistedTile.elem;
    tilesContainer.insertBefore(
        lastBlacklistedTileElement,
        tilesContainer.childNodes[lastBlacklistedIndex]);
    lastBlacklistedTileElement.addEventListener(
        'webkitTransitionEnd', undoAnimationDone);
    // Force the removal to happen synchronously.
    lastBlacklistedTileElement.scrollTop;
    lastBlacklistedTileElement.classList.remove(CLASSES.BLACKLIST);
  } else {
    // Otherwise render the tiles using the new data without animation.
    tiles = [];
    for (var i = 0; i < MAX_NUM_TILES_TO_SHOW; ++i) {
      tiles.push(createTile(pages[i]));
    }
    renderTiles();
  }
}

/**
 * Renders the current set of tiles without animation.
 */
function renderTiles() {
  removeChildren(tilesContainer);
  for (var i = 0, length = tiles.length; i < length; ++i) {
    tilesContainer.appendChild(tiles[i].elem);
  }
}

/**
 * Creates a Tile with the specified page data. If no data is provided, a
 * filler Tile is created.
 * @param {Object} page The page data.
 * @return {Tile} The new Tile.
 */
function createTile(page) {
  var tileElement = document.createElement('div');
  tileElement.classList.add(CLASSES.TILE);

  if (page) {
    var rid = page.rid;
    tileElement.classList.add(CLASSES.PAGE);

    // The click handler for navigating to the page identified by the RID.
    tileElement.addEventListener('click', function() {
      apiHandle.navigateContentWindow(rid);
    });

    // The shadow DOM which renders the page title.
    var titleElement = page.titleElement;
    if (titleElement) {
      titleElement.classList.add(CLASSES.TITLE);
      tileElement.appendChild(titleElement);
    }

    // Render the thumbnail if present. Otherwise, fall back to a shadow DOM
    // which renders the domain.
    var thumbnailUrl = page.thumbnailUrl;

    var showDomainElement = function() {
      var domainElement = page.domainElement;
      if (domainElement) {
        domainElement.classList.add(CLASSES.DOMAIN);
        tileElement.appendChild(domainElement);
      }
    };
    if (thumbnailUrl) {
      var image = new Image();
      image.onload = function() {
        var thumbnailElement = createAndAppendElement(
            tileElement, 'div', CLASSES.THUMBNAIL);
        thumbnailElement.style.backgroundImage = 'url(' + thumbnailUrl + ')';
      };

      image.onerror = showDomainElement;
      image.src = thumbnailUrl;
    } else {
      showDomainElement();
    }

    // The button used to blacklist this page.
    var blacklistButton = createAndAppendElement(
        tileElement, 'div', CLASSES.BLACKLIST_BUTTON);
    blacklistButton.addEventListener('click', generateBlacklistFunction(rid));
    // TODO(jeremycho): i18n. See http://crbug.com/190223.
    blacklistButton.title = "Don't show on this page";

    // The page favicon, if any.
    var faviconUrl = page.faviconUrl;
    if (faviconUrl) {
      var favicon = createAndAppendElement(
          tileElement, 'div', CLASSES.FAVICON);
      favicon.style.backgroundImage = 'url(' + faviconUrl + ')';
    }
    return new Tile(tileElement, rid);
  } else {
    tileElement.classList.add(CLASSES.FILLER);
    return new Tile(tileElement);
  }
}

/**
 * Generates a function to be called when the page with the corresponding RID
 * is blacklisted.
 * @param {number} rid The RID of the page being blacklisted.
 * @return {function(Event)} A function which handles the blacklisting of the
 *     page by displaying the notification, updating state variables, and
 *     notifying Chrome.
 */
function generateBlacklistFunction(rid) {
  return function(e) {
    // Prevent navigation when the page is being blacklisted.
    e.stopPropagation();

    showNotification();
    isBlacklisting = true;
    tilesContainer.classList.add(CLASSES.HIDE_BLACKLIST_BUTTON);
    lastBlacklistedTile = getTileByRid(rid);
    lastBlacklistedIndex = tiles.indexOf(lastBlacklistedTile);
    apiHandle.deleteMostVisitedItem(rid);
  };
}

/**
 * Shows the blacklist notification and triggers a delay to hide it.
 */
function showNotification() {
  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
  notification.scrollTop;
  notification.classList.add(CLASSES.DELAYED_HIDE_NOTIFICATION);
}

/**
 * Hides the blacklist notification.
 */
function hideNotification() {
  notification.classList.add(CLASSES.HIDE_NOTIFICATION);
}

/**
 * Handles the end of the blacklist animation by removing the blacklisted tile.
 */
function blacklistAnimationDone() {
  tiles.splice(lastBlacklistedIndex, 1);
  removeNode(lastBlacklistedTile.elem);
  updateTileVisibility(numTilesShown);
  isBlacklisting = false;
  tilesContainer.classList.remove(CLASSES.HIDE_BLACKLIST_BUTTON);
  lastBlacklistedTile.elem.removeEventListener(
      'webkitTransitionEnd', blacklistAnimationDone);
}

/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  var lastBlacklistedRID = lastBlacklistedTile.rid;
  if (typeof lastBlacklistedRID != 'undefined') {
    isUndoing = true;
    apiHandle.undoMostVisitedDeletion(lastBlacklistedRID);
  }
}

/**
 * Handles the end of the undo animation by removing the extraneous end tile.
 */
function undoAnimationDone() {
  isUndoing = false;
  tiles.splice(tiles.length - 1, 1);
  removeNode(tilesContainer.lastElementChild);
  updateTileVisibility(numTilesShown);
  lastBlacklistedTile.elem.removeEventListener(
      'webkitTransitionEnd', undoAnimationDone);
}

/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  apiHandle.undoAllMostVisitedDeletions();
}

/**
 * Handles a resize by vertically centering the most visited section
 * and triggering the tile show/hide animation if necessary.
 */
function onResize() {
  var clientHeight = document.documentElement.clientHeight;
  topMarginElement.style.marginTop =
      Math.max(0, (clientHeight - MOST_VISITED_HEIGHT) / 2) + 'px';

  var clientWidth = document.documentElement.clientWidth;
  var numTilesToShow = Math.floor(
      (clientWidth - MIN_TOTAL_HORIZONTAL_PADDING) / TILE_WIDTH);
  numTilesToShow = Math.max(MIN_NUM_TILES_TO_SHOW, numTilesToShow);
  if (numTilesToShow != numTilesShown) {
    updateTileVisibility(numTilesToShow);
    numTilesShown = numTilesToShow;
  }
}

/**
 * Triggers an animation to show the first numTilesToShow tiles and hide the
 * remaining.
 * @param {number} numTilesToShow The number of tiles to show.
 */
function updateTileVisibility(numTilesToShow) {
  for (var i = 0, length = tiles.length; i < length; ++i) {
    tiles[i].elem.classList.toggle(CLASSES.HIDE_TILE, i >= numTilesToShow);
  }
}

/**
 * Returns the tile corresponding to the specified page RID.
 * @param {number} rid The page RID being looked up.
 * @return {Tile} The corresponding tile.
 */
function getTileByRid(rid) {
  for (var i = 0, length = tiles.length; i < length; ++i) {
    var tile = tiles[i];
    if (tile.rid == rid)
      return tile;
  }
  return null;
}

/**
 * Utility function which creates an element with an optional classname and
 * appends it to the specified parent.
 * @param {Element} parent The parent to append the new element.
 * @param {string} name The name of the new element.
 * @param {string=} opt_class The optional classname of the new element.
 * @return {Element} The new element.
 */
function createAndAppendElement(parent, name, opt_class) {
  var child = document.createElement(name);
  if (opt_class)
    child.classList.add(opt_class);
  parent.appendChild(child);
  return child;
}

/**
 * Removes a node from its parent.
 * @param {Node} node The node to remove.
 */
function removeNode(node) {
  node.parentNode.removeChild(node);
}

/**
 * Removes all the child nodes on a DOM node.
 * @param {Node} node Node to remove children from.
 */
function removeChildren(node) {
  node.innerHTML = '';
}

/**
 * @return {Object} the handle to the embeddedSearch API.
 */
function getEmbeddedSearchApiHandle() {
  if (window.cideb)
    return window.cideb;
  if (window.chrome && window.chrome.embeddedSearch)
    return window.chrome.embeddedSearch;
  return null;
}

/**
 * Prepares the New Tab Page by adding listeners, rendering the current
 * theme, and the most visited pages section.
 */
function init() {
  topMarginElement = document.getElementById(IDS.TOP_MARGIN);
  tilesContainer = document.getElementById(IDS.TILES);
  notification = document.getElementById(IDS.NOTIFICATION);
  attribution = document.getElementById(IDS.ATTRIBUTION);

  // TODO(jeremycho): i18n.
  var notificationMessage = document.getElementById(IDS.NOTIFICATION_MESSAGE);
  notificationMessage.innerText = 'Thumbnail removed.';
  var undoLink = document.getElementById(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  undoLink.innerText = 'Undo';
  var restoreAllLink = document.getElementById(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  restoreAllLink.innerText = 'Restore all';
  attribution.innerText = 'Theme created by';

  var notificationCloseButton =
      document.getElementById(IDS.NOTIFICATION_CLOSE_BUTTON);
  notificationCloseButton.addEventListener('click', hideNotification);

  window.addEventListener('resize', onResize);
  onResize();

  var topLevelHandle = getEmbeddedSearchApiHandle();
  // This is to inform Chrome that the NTP is instant-extended capable i.e.
  // it should fire events like onmostvisitedchange.
  topLevelHandle.searchBox.onsubmit = function() {};

  apiHandle = topLevelHandle.newTabPage;
  apiHandle.onthemechange = onThemeChange;
  apiHandle.onmostvisitedchange = onMostVisitedChange;

  onThemeChange();
  onMostVisitedChange();
}

document.addEventListener('DOMContentLoaded', init);
})();
