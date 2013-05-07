// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview The local InstantExtended NTP and suggestions dropdown.
 */

(function() {
<include src="../../../../ui/webui/resources/js/assert.js">


/**
 * True if this a Google page and not some other search provider.  Used to
 * determine whether to show the logo and fakebox.
 * @type {boolean}
 * @const
 */
var isGooglePage = location.href.indexOf('isGoogle') != -1;

// ==========================================================
//  Enums
// ==========================================================


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  BLACKLIST: 'mv-blacklist', // triggers tile blacklist animation
  BLACKLIST_BUTTON: 'mv-x',
  CUSTOM_THEME: 'custom-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  DOMAIN: 'mv-domain',
  FAKEBOX_ANIMATE: 'fakebox-animate', // triggers fakebox animation
  FAKEBOX_FOCUS: 'fakebox-focused', // Applies focus styles to the fakebox
  FAVICON: 'mv-favicon',
  GOOGLE_PAGE: 'google-page', // shows the Google logo and fakebox
  HIDE_BLACKLIST_BUTTON: 'mv-x-hide', // hides blacklist button during animation
  HIDE_NOTIFICATION: 'mv-notice-hide',
  HIDE_NTP: 'hide-ntp', // hides NTP and disables scrollbars
  HIDE_TILE: 'mv-tile-hide', // hides tiles on small browser width
  HOVERED: 'hovered',
  PAGE: 'mv-page', // page tiles
  PAGE_READY: 'mv-page-ready',  // page tile when ready
  ROW: 'mv-row',  // tile row
  SEARCH: 'search',
  SELECTED: 'selected', // a selected suggestion (if any)
  SUGGESTION: 'suggestion',
  SUGGESTION_CONTENTS: 'suggestion-contents',
  SUGGESTIONS_BOX: 'suggestions-box',
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
  ACTIVE_SUGGESTIONS_CONTAINER: 'active-suggestions-container',
  ATTRIBUTION: 'attribution',
  CURSOR: 'cursor',
  FAKEBOX: 'fakebox',
  LOGO: 'logo',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  PENDING_SUGGESTIONS_CONTAINER: 'pending-suggestions-container',
  RESTORE_ALL_LINK: 'mv-restore',
  SUGGESTION_LOADER: 'suggestion-loader',
  SUGGESTION_STYLE: 'suggestion-style',
  SUGGESTION_TEXT_PREFIX: 'suggestion-text-',
  TILES: 'mv-tiles',
  TOP_MARGIN: 'mv-top-margin',
  UNDO_LINK: 'mv-undo'
};

// =============================================================================
//  NTP implementation
// =============================================================================


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
 * The "fakebox" - an input field that looks like a regular searchbox.  When it
 * is focused, any text the user types goes directly into the omnibox.
 * @type {Element}
 */
var fakebox;


/**
 * The container for NTP elements that should be hidden when suggestions are
 * visible.
 * @type {Element}
 */
var ntpContents;


/**
 * The array of rendered tiles, ordered by appearance.
 * @type {!Array.<Tile>}
 */
var tiles = [];


/**
 * The last blacklisted tile if any, which by definition should not be filler.
 * @type {?Tile}
 */
var lastBlacklistedTile = null;


/**
 * True if a page has been blacklisted and we're waiting on the
 * onmostvisitedchange callback. See onMostVisitedChange() for how this
 * is used.
 * @type {boolean}
 */
var isBlacklisting = false;


/**
 * Current number of tiles columns shown based on the window width, including
 * those that just contain filler.
 * @type {number}
 */
var numColumnsShown = 0;


/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/**
 * Possible background-colors of a non-custom theme. Used to determine whether
 * the homepage should be updated to support custom or non-custom themes.
 * @type {!Array.<string>}
 * @const
 */
var WHITE = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];


/**
 * Total tile width. Should be equal to mv-tile's width + 2 * border-width.
 * @private {number}
 * @const
 */
var TILE_WIDTH = 140;


/**
 * Margin between tiles. Should be equal to mv-tile's -webkit-margin-start.
 * @private {number}
 * @const
 */
var TILE_MARGIN_START = 20;


/**
 * The height of the most visited section.
 * @type {number}
 * @const
 */
var MOST_VISITED_HEIGHT = 296;


/** @type {number} @const */
var MAX_NUM_TILES_TO_SHOW = 8;


/** @type {number} @const */
var MIN_NUM_COLUMNS = 2;


/** @type {number} @const */
var MAX_NUM_COLUMNS = 4;


/** @type {number} @const */
var NUM_ROWS = 2;


/**
 * Minimum total padding to give to the left and right of the most visited
 * section. Used to determine how many tiles to show.
 * @type {number}
 * @const
 */
var MIN_TOTAL_HORIZONTAL_PADDING = 200;


/**
 * The filename for a most visited iframe src which shows a page title.
 * @type {string}
 * @const
 */
var MOST_VISITED_TITLE_IFRAME = 'title.html';


/**
 * The filename for a most visited iframe src which shows a thumbnail image.
 * @type {string}
 * @const
 */
var MOST_VISITED_THUMBNAIL_IFRAME = 'thumbnail.html';


/**
 * The hex color for most visited tile elements.
 * @type {string}
 * @const
 */
var MOST_VISITED_COLOR = '777777';


/**
 * The hex color for most visited tile titles when using a custom theme.
 * @type {string}
 * @const
 */
var MOST_VISITED_THEME_TITLE_COLOR = 'ffffff';


/**
 * The font family for most visited tile elements.
 * @type {string}
 * @const
 */
var MOST_VISITED_FONT_FAMILY = '';


/**
 * The font size for most visited tile elements.
 * @type {number}
 * @const
 */
var MOST_VISITED_FONT_SIZE = 11;


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
  if (!isNtpVisible())
    return;

  var info = ntpApiHandle.themeBackgroundInfo;
  if (!info)
    return;
  var background = [info.colorRgba,
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();
  document.body.style.background = background;
  var isCustom = !!background && WHITE.indexOf(background) == -1;
  document.body.classList.toggle(CLASSES.CUSTOM_THEME, isCustom);
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
  var pages = ntpApiHandle.mostVisited;

  if (isBlacklisting) {
    // Trigger the blacklist animation and re-render the tiles when it
    // completes.
    var lastBlacklistedTileElement = lastBlacklistedTile.elem;
    lastBlacklistedTileElement.addEventListener(
        'webkitTransitionEnd', blacklistAnimationDone);
    lastBlacklistedTileElement.classList.add(CLASSES.BLACKLIST);

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
 * Renders the current set of tiles.
 */
function renderTiles() {
  var rows = tilesContainer.children;
  for (var i = 0; i < rows.length; ++i) {
    removeChildren(rows[i]);
  }

  for (var i = 0, length = tiles.length;
       i < Math.min(length, numColumnsShown * NUM_ROWS); ++i) {
    rows[Math.floor(i / numColumnsShown)].appendChild(tiles[i].elem);
  }
}


/**
 * Builds a URL to display a most visited tile component in an iframe.
 * @param {string} filename The desired most visited component filename.
 * @param {number} rid The restricted ID.
 * @param {string} color The text color for text in the iframe.
 * @param {string} fontFamily The font family for text in the iframe.
 * @param {number} fontSize The font size for text in the iframe.
 * @param {boolean} textShadow True if text should be drawn with a shadow.
 * @return {string} An URL to display the most visited component in an iframe.
 */
function getMostVisitedIframeUrl(filename, rid, color, fontFamily, fontSize,
    textShadow) {
  return 'chrome-search://most-visited/' + encodeURIComponent(filename) + '?' +
      ['rid=' + encodeURIComponent(rid),
       'c=' + encodeURIComponent(color),
       'f=' + encodeURIComponent(fontFamily),
       'fs=' + encodeURIComponent(fontSize),
       'ts=' + (textShadow ? '1' : '')].join('&');
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
      ntpApiHandle.navigateContentWindow(rid);
    });

    // The iframe which renders the page title.
    var titleElement = document.createElement('iframe');
    var usingCustomTheme = document.body.classList.contains(
        CLASSES.CUSTOM_THEME);

    titleElement.src = getMostVisitedIframeUrl(
        MOST_VISITED_TITLE_IFRAME, rid,
        usingCustomTheme ? MOST_VISITED_THEME_TITLE_COLOR : MOST_VISITED_COLOR,
        MOST_VISITED_FONT_FAMILY, MOST_VISITED_FONT_SIZE, usingCustomTheme);
    titleElement.hidden = true;
    titleElement.onload = function() { titleElement.hidden = false; };
    titleElement.className = CLASSES.TITLE;
    tileElement.appendChild(titleElement);

    // The iframe which renders either a thumbnail or domain element.
    var thumbnailElement = document.createElement('iframe');
    thumbnailElement.src = getMostVisitedIframeUrl(
        MOST_VISITED_THUMBNAIL_IFRAME, rid, MOST_VISITED_COLOR,
        MOST_VISITED_FONT_FAMILY, MOST_VISITED_FONT_SIZE, false);
    thumbnailElement.hidden = true;
    thumbnailElement.onload = function() {
      thumbnailElement.hidden = false;
      tileElement.classList.add(CLASSES.PAGE_READY);
    };
    thumbnailElement.className = CLASSES.THUMBNAIL;
    tileElement.appendChild(thumbnailElement);

    // The button used to blacklist this page.
    var blacklistButton = createAndAppendElement(
        tileElement, 'div', CLASSES.BLACKLIST_BUTTON);
    blacklistButton.addEventListener('click', generateBlacklistFunction(rid));
    blacklistButton.title = templateData.removeThumbnailTooltip;

    // The page favicon, if any.
    var faviconUrl = page.faviconUrl;
    if (faviconUrl) {
      var favicon = createAndAppendElement(
          tileElement, 'div', CLASSES.FAVICON);
      favicon.style.backgroundImage = 'url(' + faviconUrl + ')';
    }
    return new Tile(tileElement, rid);
  } else {
    return new Tile(tileElement);
  }
}


/**
 * Generates a function to be called when the page with the corresponding RID
 * is blacklisted.
 * @param {number} rid The RID of the page being blacklisted.
 * @return {function(Event)} A function which handles the blacklisting of the
 *     page by updating state variables and notifying Chrome.
 */
function generateBlacklistFunction(rid) {
  return function(e) {
    // Prevent navigation when the page is being blacklisted.
    e.stopPropagation();

    isBlacklisting = true;
    tilesContainer.classList.add(CLASSES.HIDE_BLACKLIST_BUTTON);
    lastBlacklistedTile = getTileByRid(rid);
    ntpApiHandle.deleteMostVisitedItem(rid);
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
 * Handles the end of the blacklist animation by showing the notification and
 * re-rendering the new set of tiles.
 */
function blacklistAnimationDone() {
  showNotification();
  isBlacklisting = false;
  tilesContainer.classList.remove(CLASSES.HIDE_BLACKLIST_BUTTON);
  lastBlacklistedTile.elem.removeEventListener(
      'webkitTransitionEnd', blacklistAnimationDone);
  // Need to call explicitly to re-render the tiles, since the initial
  // onmostvisitedchange issued by the blacklist function only triggered
  // the animation.
  onMostVisitedChange();
}


/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  var lastBlacklistedRID = lastBlacklistedTile.rid;
  if (typeof lastBlacklistedRID != 'undefined')
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedRID);
}


/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  ntpApiHandle.undoAllMostVisitedDeletions();
}


/**
 * Handles a resize by vertically centering the most visited section
 * and re-rendering the tiles if the number of columns has changed.
 */
function onResize() {
  // The Google page uses a fixed layout instead.
  if (!isGooglePage) {
    var clientHeight = document.documentElement.clientHeight;
    topMarginElement.style.marginTop =
        Math.max(0, (clientHeight - MOST_VISITED_HEIGHT) / 2) + 'px';
  }

  var tileRequiredWidth = TILE_WIDTH + TILE_MARGIN_START;
  // Adds margin-start to the available width to compensate the extra margin
  // counted above for the first tile (which does not have a margin-start).
  var availableWidth = document.documentElement.clientWidth +
      TILE_MARGIN_START - MIN_TOTAL_HORIZONTAL_PADDING;
  var numColumnsToShow = Math.floor(availableWidth / tileRequiredWidth);
  numColumnsToShow = Math.max(MIN_NUM_COLUMNS,
                              Math.min(MAX_NUM_COLUMNS, numColumnsToShow));
  if (numColumnsToShow != numColumnsShown) {
    numColumnsShown = numColumnsToShow;
    renderTiles();
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
 * Hides the NTP.
 */
function hideNtp() {
  if (fakebox && isFakeboxFocused() &&
      !document.body.classList.contains(CLASSES.FAKEBOX_ANIMATE)) {
    // The user has typed in the fakebox - initiate the fakebox animation,
    // which upon termination will hide the NTP.
    setFakeboxFocus(false);
    fakebox.addEventListener('webkitTransitionEnd', fakeboxAnimationDone);
    document.body.classList.add(CLASSES.FAKEBOX_ANIMATE);
  } else if (!fakebox ||
      !document.body.classList.contains(CLASSES.FAKEBOX_ANIMATE)) {
    // The user has typed in the omnibox - hide the NTP immediately.
    document.body.classList.add(CLASSES.HIDE_NTP);
    clearCustomTheme();
  }
}


/**
 * Shows the NTP (destroys the activeBox if exists and reloads the custom
 * theme).
 */
function showNtp() {
  hideActiveSuggestions();
  document.body.classList.remove(CLASSES.HIDE_NTP);
  onThemeChange();
}

/**
 * Clears the custom theme (if any).
 */
function clearCustomTheme() {
  document.body.style.background = '';
  document.body.classList.remove(CLASSES.CUSTOM_THEME);
}


/**
 * @return {boolean} True if the NTP is visible.
 */
function isNtpVisible() {
  return !document.body.classList.contains(CLASSES.HIDE_NTP);
}


/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  return document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @return {boolean} True if the fakebox has focus.
 */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS);
}


/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in the fakebox.
 */
function isFakeboxClick(event) {
  return fakebox.contains(event.target);
}


/**
 * Cleans up the fakebox animation, hides the NTP, and shows suggestions.
 * @param {!Event} e The webkitTransitionEnd event.
 */
function fakeboxAnimationDone(event) {
  if (event.propertyName == '-webkit-transform') {
    document.body.classList.add(CLASSES.HIDE_NTP);
    clearCustomTheme();
    document.body.classList.remove(CLASSES.FAKEBOX_ANIMATE);
    fakebox.removeEventListener('webkitTransitionEnd', fakeboxAnimationDone);
  }
}

// =============================================================================
//  Dropdown Implementation
// =============================================================================


/**
 * Possible behaviors for navigateContentWindow.
 * @enum {number}
 */
var WindowOpenDisposition = {
  CURRENT_TAB: 1,
  NEW_BACKGROUND_TAB: 2
};


/**
 * The JavaScript button event value for a middle click.
 * @type {number}
 * @const
 */
var MIDDLE_MOUSE_BUTTON = 1;


/**
 * The maximum number of suggestions to show.
 * @type {number}
 * @const
 */
var MAX_SUGGESTIONS_TO_SHOW = 5;


/**
 * Assume any native suggestion with a score higher than this value has been
 * inlined by the browser.
 * @type {number}
 * @const
 */
var INLINE_SUGGESTION_THRESHOLD = 1200;


/**
 * The color code for a query.
 * @type {number}
 * @const
 */
var SUGGESTION_QUERY_COLOR = 0x000000;


/**
 * The color code for a suggestion display URL.
 * @type {number}
 * @const
 */
var SUGGESTION_URL_COLOR = 0x009933;


/**
 * The color code for a suggestion title.
 * @type {number}
 * @const
 */
var SUGGESTION_TITLE_COLOR = 0x666666;


/**
 * A top position which is off-screen.
 * @type {string}
 * @const
 */
var OFF_SCREEN = '-1000px';


/**
 * The expected origin of a suggestion iframe.
 * @type {string}
 * @const
 */
var SUGGESTION_ORIGIN = 'chrome-search://suggestion';


/**
 * Suggestion provider type corresponding to a verbatim URL suggestion.
 * @type {string}
 * @const
 */
var VERBATIM_URL_TYPE = 'url-what-you-typed';


/**
 * Suggestion provider type corresponding to a verbatim search suggestion.
 * @type {string}
 * @const
 */
var VERBATIM_SEARCH_TYPE = 'search-what-you-typed';


/**
 * "Up" arrow keycode.
 * @type {number}
 * @const
 */
var KEY_UP_ARROW = 38;


/**
 * "Down" arrow keycode.
 * @type {number}
 * @const
 */
var KEY_DOWN_ARROW = 40;


/**
 * "Esc" keycode.
 * @type {number}
 * @const
 */
var KEY_ESC = 27;

/**
 * Pixels of padding inside a suggestion div for displaying its icon.
 * @type {number}
 * @const
 */
var SUGGESTION_ICON_PADDING = 26;


/**
 * Pixels by which iframes should be moved down relative to their wrapping
 * suggestion div.
 */
var SUGGESTION_TOP_OFFSET = 4;


/**
 * The displayed suggestions.
 * @type {SuggestionsBox}
 */
var activeBox;


/**
 * The suggestions being rendered.
 * @type {SuggestionsBox}
 */
var pendingBox;


/**
 * A pool of iframes to display suggestions.
 * @type {IframePool}
 */
var iframePool;


/**
 * A serial number for the next suggestions rendered.
 * @type {number}
 */
var nextRequestId = 0;


/**
 * The omnibox input value during the last onnativesuggestions event.
 * @type {string}
 */
var lastInputValue = '';


/**
 * True if updateSuggestions() was deferred due to suggestion iframes not being
 * ready and false otherwise.
 * @type {boolean}
 */
var updateSuggestionsWasDeferred = false;


/**
 * @param {Object} suggestion A suggestion.
 * @param {boolean} inVerbatimMode Are we in verbatim mode?
 * @return {boolean} True if the suggestion should be selected.
 */
function shouldSelectSuggestion(suggestion, inVerbatimMode) {
  var isVerbatimUrl = suggestion.type == VERBATIM_URL_TYPE;
  var inlineableSuggestion = suggestion.type != VERBATIM_SEARCH_TYPE &&
      suggestion.rankingData.relevance > INLINE_SUGGESTION_THRESHOLD;
  // Verbatim URLs should always be selected. Otherwise, select suggestions
  // with a high enough score unless we are in verbatim mode (e.g. backspacing
  // away).
  return isVerbatimUrl || (!inVerbatimMode && inlineableSuggestion);
}


/**
 * Extract the desired navigation behavior from a click button.
 * @param {number} button The Event#button property of a click event.
 * @return {WindowOpenDisposition} The desired behavior for
 *     navigateContentWindow.
 */
function getDispositionFromClickButton(button) {
  if (button == MIDDLE_MOUSE_BUTTON)
    return WindowOpenDisposition.NEW_BACKGROUND_TAB;
  return WindowOpenDisposition.CURRENT_TAB;
}


/**
 * @return {boolean} True if all suggestion iframes are ready.
 */
function suggestionIframesReady() {
  return iframePool.ready() && $(IDS.SUGGESTION_LOADER).loaded;
}


/**
 * Manages a pool of chrome-search suggestion result iframes.
 * @constructor
 */
function IframePool() {
}

IframePool.prototype = {
  /**
   * HTML iframe elements.
   * @type {Array.<Element>}
   * @private
   */
  iframes_: [],

  /**
   * Number of iframes pending load.
   * @type {number}
   * @private
   */
  pendingCount_: 0,

  /**
   * Initializes the pool with blank result template iframes, positioned off
   * screen.
   */
  init: function() {
    for (var i = 0; i < 2 * MAX_SUGGESTIONS_TO_SHOW; ++i) {
      var iframe = document.createElement('iframe');
      iframe.className = CLASSES.SUGGESTION_CONTENTS;
      iframe.id = IDS.SUGGESTION_TEXT_PREFIX + i;
      iframe.src = 'chrome-search://suggestion/result.html';
      iframe.style.top = OFF_SCREEN;
      iframe.addEventListener('mouseover', function(e) {
        if (activeBox)
          activeBox.hover(e.currentTarget.id);
      }, false);
      iframe.addEventListener('mouseout', function(e) {
        if (activeBox)
          activeBox.unhover(e.currentTarget.id);
      }, false);
      this.pendingCount_++;
      var self = this;
      iframe.onload = function() {
        self.pendingCount_--;
        callDeferredUpdateSuggestions();
      };
      document.body.appendChild(iframe);
      this.iframes_.push(iframe);
    }
  },

  /**
   * @return {boolean} True if all suggestion iframes are loaded.
   */
  ready: function() {
    return this.pendingCount_ == 0;
  },

  /**
   * Reserves a free suggestion iframe from the pool.
   * @return {Element} An iframe suitable for holding a suggestion.
   */
  reserve: function() {
    return this.iframes_.pop();
  },

  /**
   * Releases a suggestion iframe back into the pool.
   * @param {Element} iframe The iframe to return to the pool.
   */
  release: function(iframe) {
    this.iframes_.push(iframe);
    iframe.style.top = OFF_SCREEN;
  }
};


/**
 * An individual suggestion.
 * @param {!Object} data Autocomplete fields for this suggestion.
 * @constructor
 */
function Suggestion(data) {
  assert(data);
  /**
   * Autocomplete fields for this suggestion.
   * @type {!Object}
   * @private
   */
  this.data_ = data;
}

Suggestion.prototype = {
  /**
   * Releases the iframe reserved for this suggestion.
   */
  destroy: function() {
    if (this.iframe_)
      iframePool.release(this.iframe_);
  },

  /**
   * Creates and appends the placeholder div for this suggestion to box.
   * @param {Element} box A suggestions box.
   * @param {boolean} selected True if the suggestion should be drawn as
   *     selected and false otherwise.
   */
  appendToBox: function(box, selected) {
    var div = document.createElement('div');
    div.classList.add(CLASSES.SUGGESTION);
    div.classList.toggle(CLASSES.SELECTED, selected);
    div.classList.toggle(CLASSES.SEARCH, this.data_.is_search);
    box.appendChild(div);
    this.div_ = div;
  },

  /**
   * Repositions the suggestion iframe to align with its expected dropdown
   * position.
   * @param {boolean} isRtl True if rendering right-to-left and false if not.
   * @param {number} startMargin Leading space before suggestion.
   * @param {number} totalMargin Total non-content space on suggestion line.
   */
  reposition: function(isRtl, startMargin, totalMargin) {
    // Add in the expected parent offset and the top margin.
    this.iframe_.style.top = this.div_.offsetTop + SUGGESTION_TOP_OFFSET + 'px';
    // Call parseInt to enforce that startMargin and totalMargin are really
    // numbers since we're interpolating CSS.
    startMargin = parseInt(startMargin, 10);
    totalMargin = parseInt(totalMargin, 10);
    if (isFinite(startMargin) && isFinite(totalMargin)) {
      this.iframe_.style[isRtl ? 'right' : 'left'] = startMargin + 'px';
      this.iframe_.style.width = '-webkit-calc(100% - ' +
          (totalMargin + SUGGESTION_ICON_PADDING) + 'px)';
    }
  },

  /**
   * Updates the suggestion selection state.
   * @param {boolean} selected True if drawn selected or false if not.
   */
  select: function(selected) {
    this.div_.classList.toggle(CLASSES.SELECTED, selected);
  },

  /**
   * Updates the suggestion hover state.
   * @param {boolean} hovered True if drawn hovered or false if not.
   */
  hover: function(hovered) {
    this.div_.classList.toggle(CLASSES.HOVERED, hovered);
  },

  /**
   * @param {Window} iframeWindow The content window of an iframe.
   * @return {boolean} True if this suggestion's iframe has the specified
   *     window and false if not.
   */
  hasIframeWindow: function(iframeWindow) {
    return this.iframe_.contentWindow == iframeWindow;
  },

  /**
   * @param {string} id An element id.
   * @return {boolean} True if this suggestion's iframe has the specified id
   *     and false if not.
   */
  hasIframeId: function(id) {
    return this.iframe_.id == id;
  },

  /**
   * The iframe element for this suggestion.
   * @type {Element}
   */
  set iframe(iframe) {
    this.iframe_ = iframe;
  },

  /**
   * The restricted id associated with this suggestion.
   * @type {number}
   */
  get restrictedId() {
    return this.data_.rid;
  },
};


/**
 * Displays a suggestions box.
 * @param {string} inputValue The user text that prompted these suggestions.
 * @param {!Array.<!Object>} suggestionData Suggestion data to display.
 * @param {number} selectedIndex The index of the suggestion selected.
 * @constructor
 */
function SuggestionsBox(inputValue, suggestionData, selectedIndex) {
  /**
   * The user text that prompted these suggestions.
   * @type {string}
   * @private
   */
  this.inputValue_ = inputValue;

  /**
   * The index of the suggestion currently selected, whether by default or
   * because the user arrowed down to it.
   * @type {number}
   * @private
   */
  this.selectedIndex_ = selectedIndex;

  /**
   * The index of the suggestion currently under the mouse pointer.
   * @type {number}
   * @private
   */
  this.hoveredIndex_ = -1;

  /**
   * A stamp to distinguish this suggestions box from others.
   * @type {number}
   * @private
   */
  this.requestId_ = nextRequestId++;

  /**
   * The ordered suggestions this box is displaying.
   * @type {Array.<Suggestion>}
   * @private
   */
  this.suggestions_ = [];
  for (var i = 0; i < suggestionData.length; ++i) {
    this.suggestions_.push(new Suggestion(suggestionData[i]));
  }

  /**
   * The container for this suggestions box. #pending-suggestion-container
   * if inactive and #active-suggestion-container if active.
   * @type {Element}
   * @private
   */
  this.container_ = $(IDS.PENDING_SUGGESTIONS_CONTAINER);
  assert(this.container_);
}

SuggestionsBox.prototype = {
  /**
   * Releases suggestion iframes and ignores any load done message for the
   * current suggestions.
   */
  destroy: function() {
    while (this.suggestions_.length > 0) {
      this.suggestions_.pop().destroy();
    }
    this.responseId = -1;
  },

  /**
   * Starts rendering new suggestions.
   */
  loadSuggestions: function() {
    // Create a placeholder DOM in the invisible container.
    this.container_.innerHTML = '';

    var box = document.createElement('div');
    box.className = CLASSES.SUGGESTIONS_BOX;
    this.container_.appendChild(box);

    var iframesToLoad = {};
    for (var i = 0; i < this.suggestions_.length; ++i) {
      var suggestion = this.suggestions_[i];
      suggestion.appendToBox(box, i == this.selectedIndex_);
      var iframe = iframePool.reserve();
      suggestion.iframe = iframe;
      iframesToLoad[iframe.id] = suggestion.restrictedId;
    }

    // Ask the loader iframe to populate the iframes just reserved.
    var loadRequest = {
      load: iframesToLoad,
      requestId: this.requestId_,
      style: {
        queryColor: SUGGESTION_QUERY_COLOR,
        urlColor: SUGGESTION_URL_COLOR,
        titleColor: SUGGESTION_TITLE_COLOR
      }
    };
    $(IDS.SUGGESTION_LOADER).contentWindow.postMessage(loadRequest,
        SUGGESTION_ORIGIN);
  },

  /**
   * @param {number} responseId The id of a request that just finished
   *     rendering.
   * @return {boolean} Whether the request is for the suggestions in this box.
   */
  isResponseCurrent: function(responseId) {
    return responseId == this.requestId_;
  },

  /**
   * Moves suggestion iframes into position.
   */
  repositionSuggestions: function() {
    // Note: This may be called before margins are ready. In that case,
    // suggestion iframes will initially be too large and then size down
    // onmarginchange.
    var startMargin = searchboxApiHandle.startMargin;
    var totalMargin = window.innerWidth - searchboxApiHandle.width;
    var isRtl = searchboxApiHandle.rtl;
    for (var i = 0; i < this.suggestions_.length; ++i) {
      this.suggestions_[i].reposition(isRtl, startMargin, totalMargin);
    }
  },

  /**
   * Selects the suggestion before the current selection.
   */
  selectPrevious: function() {
    this.changeSelection_(this.selectedIndex_ - 1);
  },

  /**
   * Selects the suggestion after the current selection.
   */
  selectNext: function() {
    this.changeSelection_(this.selectedIndex_ + 1);
  },

  /**
   * @return {boolean} True if a suggestion is selected by default or because
   * the user arrowed down to it.
   */
  hasSelectedSuggestion: function() {
    return this.selectedIndex_ != -1;
  },

  /**
   * Clears the selected suggestion.
   */
  clearSelection: function() {
    this.selectedIndex_ = -1;
    var oldSelection = this.container_.querySelector('.' + CLASSES.SELECTED);
    if (oldSelection)
      oldSelection.classList.remove(CLASSES.SELECTED);
  },

  /**
   * Changes the current selected suggestion index.
   * @param {number} index The new selection to suggest.
   * @private
   */
  changeSelection_: function(index) {
    var numSuggestions = this.suggestions_.length;
    this.selectedIndex_ = Math.min(numSuggestions - 1, Math.max(-1, index));

    this.redrawSelection_();
    this.redrawHover_();
  },

  /**
   * Redraws the selected suggestion.
   * @private
   */
  redrawSelection_: function() {
    var oldSelection = this.container_.querySelector('.' + CLASSES.SELECTED);
    if (oldSelection)
      oldSelection.classList.remove(CLASSES.SELECTED);
    if (!this.hasSelectedSuggestion()) {
      searchboxApiHandle.setValue(this.inputValue_);
    } else {
      this.suggestions_[this.selectedIndex_].select(true);
      searchboxApiHandle.setRestrictedValue(
          this.suggestions_[this.selectedIndex_].restrictedId);
    }
  },

  /**
   * @param {!Window} iframeWindow The window of the iframe that was clicked.
   * @return {?number} The restricted ID of the iframe that was clicked, or
   *     null if there was none.
   */
  getClickTarget: function(iframeWindow) {
    for (var i = 0; i < this.suggestions_.length; ++i) {
      if (this.suggestions_[i].hasIframeWindow(iframeWindow))
        return this.suggestions_[i].restrictedId;
    }
    return null;
  },

  /**
   * Called when the user hovers on the specified iframe to update hoveredIndex_
   * and draw a hover background.
   * @param {string} iframeId The id of the iframe hovered.
   */
  hover: function(iframeId) {
    this.hoveredIndex_ = -1;
    for (var i = 0; i < this.suggestions_.length; ++i) {
      if (this.suggestions_[i].hasIframeId(iframeId)) {
        this.hoveredIndex_ = i;
        break;
      }
    }
    this.redrawHover_();
  },

  /**
   * Called when the user unhovers the specified iframe to clear the current
   * hover.
   * @param {string} iframeId The id of the iframe hovered.
   */
  unhover: function(iframeId) {
    if (this.suggestions_[this.hoveredIndex_] &&
        this.suggestions_[this.hoveredIndex_].hasIframeId(iframeId)) {
      this.clearHover();
    }
  },

  /**
   * Clears the current hover.
   */
  clearHover: function() {
    this.hoveredIndex_ = -1;
    this.redrawHover_();
  },

  /**
   * Redraws the mouse hover background.
   * @private
   */
  redrawHover_: function() {
    var oldHover = this.container_.querySelector('.' + CLASSES.HOVERED);
    if (oldHover)
      oldHover.classList.remove(CLASSES.HOVERED);
    if (this.hoveredIndex_ != -1 && this.hoveredIndex_ != this.selectedIndex_)
      this.suggestions_[this.hoveredIndex_].hover(true);
  },

  /**
   * Marks the suggestions container as active.
   */
  activate: function() {
    this.container_.id = IDS.ACTIVE_SUGGESTIONS_CONTAINER;
  },

  /**
   * Marks the suggestions container as inactive.
   */
  deactivate: function() {
    this.container_.id = IDS.PENDING_SUGGESTIONS_CONTAINER;
    this.container_.innerHTML = '';
  },

  /**
   * The height of the suggestions container.
   * @type {number}
   */
  get height() {
    return this.container_.offsetHeight;
  },
};


/**
 * Clears the currently active suggestions and shows pending suggestions.
 */
function makePendingSuggestionsActive() {
  if (activeBox) {
    activeBox.deactivate();
    activeBox.destroy();
  } else {
    // Initially there will be no active suggestions, but we still want to use
    // #active-suggestions-container to load the next suggestions.
    $(IDS.ACTIVE_SUGGESTIONS_CONTAINER).id = IDS.PENDING_SUGGESTIONS_CONTAINER;
  }
  pendingBox.activate();
  activeBox = pendingBox;
  pendingBox = null;
  activeBox.repositionSuggestions();
  searchboxApiHandle.showOverlay(activeBox.height);
}


/**
 * Hides the active suggestions box.
 */
function hideActiveSuggestions() {
  searchboxApiHandle.showOverlay(0);
  if (activeBox) {
    $(IDS.ACTIVE_SUGGESTIONS_CONTAINER).innerHTML = '';
    activeBox.destroy();
  }
  activeBox = null;
}


/**
 * Updates suggestions in response to a onchange or onnativesuggestions call.
 */
function updateSuggestions() {
  updateSuggestionsWasDeferred = !suggestionIframesReady();
  // Suggestion iframes aren't loaded, so the page can't show suggestions yet.
  if (updateSuggestionsWasDeferred)
    return;
  if (pendingBox)
    pendingBox.destroy();
  pendingBox = null;
  var suggestions = searchboxApiHandle.nativeSuggestions;
  if (suggestions.length) {
    suggestions.sort(function(a, b) {
      return b.rankingData.relevance - a.rankingData.relevance;
    });
    var selectedIndex = -1;
    if (shouldSelectSuggestion(suggestions[0], searchboxApiHandle.verbatim))
      selectedIndex = 0;
    // Don't display a search-what-you-typed suggestion if it's the top match.
    if (suggestions[0].type == VERBATIM_SEARCH_TYPE)
      suggestions.shift();
  }
  var inputValue = searchboxApiHandle.value;

  // Hide the NTP if input has made it into the omnibox.
  if (inputValue && isNtpVisible())
    hideNtp();

  if (inputValue && suggestions.length) {
    pendingBox = new SuggestionsBox(inputValue,
        suggestions.slice(0, MAX_SUGGESTIONS_TO_SHOW), selectedIndex);
    searchboxApiHandle.hideBars();
    pendingBox.loadSuggestions();
  } else {
    hideActiveSuggestions();
  }

  lastInputValue = inputValue;
}


/**
 * Calls updateSuggestions() if it was deferred until suggestion iframes loaded
 * and they have now all loaded.
 */
function callDeferredUpdateSuggestions() {
  if (updateSuggestionsWasDeferred && suggestionIframesReady())
    updateSuggestions();
}


/**
 * Appends or replaces a style node for suggestion properties that depend on
 * searchboxApiHandle.
 */
function setSuggestionStyles() {
  assert(window.innerWidth > 0);
  var isRtl = searchboxApiHandle.rtl;
  var startMargin = searchboxApiHandle.startMargin;
  var style = $(IDS.SUGGESTION_STYLE) || document.createElement('style');
  style.type = 'text/css';
  style.id = IDS.SUGGESTION_STYLE;
  style.textContent =
      '.suggestion, ' +
      '.suggestion.search {' +
      '  background-position: ' +
          (isRtl ? '-webkit-calc(100% - 5px)' : '5px') + ' 4px;' +
      '  margin-' + (isRtl ? 'right' : 'left') + ': ' + startMargin + 'px;' +
      '  margin-' + (isRtl ? 'left' : 'right') + ': ' +
          (window.innerWidth - searchboxApiHandle.width - startMargin) + 'px;' +
      '  font: ' + searchboxApiHandle.fontSize + 'px "' +
          searchboxApiHandle.font + '";' +
      '}' +
      '.suggestion-contents {' +
      '  padding-' + (isRtl ? 'right' : 'left') + ': 26px;' +
      '}';
  $qs('head').appendChild(style);

  if (activeBox)
    activeBox.repositionSuggestions();

  // This is bound only for initialization. Afterwards this style should only
  // change onmarginchange.
  window.removeEventListener('resize', setSuggestionStyles);
}


/**
 * Handles key press events.
 * @param {Object} e The key being pressed.
 */
function handleKeyPress(e) {
  switch (e.keyCode) {
    case KEY_UP_ARROW:
      if (activeBox)
        activeBox.selectPrevious();
      break;
    case KEY_DOWN_ARROW:
      if (activeBox)
        activeBox.selectNext();
      break;
    case KEY_ESC:
      if (activeBox && activeBox.hasSelectedSuggestion())
        activeBox.clearSelection();
      else
        showNtp();
      break;
  }
}


/**
 * Handles postMessage calls from suggestion iframes.
 * @param {Object} message A notification that all iframes are done loading or
 *     that an iframe was clicked.
 */
function handleMessage(message) {
  if (message.origin != SUGGESTION_ORIGIN)
    return;

  if ('loaded' in message.data) {
    if (pendingBox && pendingBox.isResponseCurrent(message.data.loaded))
      makePendingSuggestionsActive();
  } else if ('click' in message.data) {
    if (activeBox) {
      var restrictedId = activeBox.getClickTarget(message.source);
      if (restrictedId != null) {
        hideActiveSuggestions();
        searchboxApiHandle.navigateContentWindow(restrictedId,
            getDispositionFromClickButton(message.data.click));
      }
    }
  }
}

// =============================================================================
//  Utils
// =============================================================================


/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}


/**
 * Shortcut for document.querySelector.
 * @param {string} selector A selector to query the desired element.
 * @return {HTMLElement} The first element to match |selector| or null.
 */
function $qs(selector) {
  return document.querySelector(selector);
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

// =============================================================================
//  Initialization
// =============================================================================


/**
 * Prepares the New Tab Page by adding listeners, rendering the current
 * theme, the most visited pages section, and Google-specific elements for a
 * Google-provided page.
 */
function init() {
  document.title = templateData.title;

  iframePool = new IframePool();
  iframePool.init();
  $(IDS.SUGGESTION_LOADER).onload = function() {
    $(IDS.SUGGESTION_LOADER).loaded = true;
    callDeferredUpdateSuggestions();
  };

  topMarginElement = $(IDS.TOP_MARGIN);
  tilesContainer = $(IDS.TILES);
  notification = $(IDS.NOTIFICATION);
  attribution = $(IDS.ATTRIBUTION);
  ntpContents = $(IDS.NTP_CONTENTS);

  for (var i = 0; i < NUM_ROWS; i++) {
    var row = document.createElement('div');
    row.classList.add(CLASSES.ROW);
    tilesContainer.appendChild(row);
  }

  if (isGooglePage) {
    document.body.classList.add(CLASSES.GOOGLE_PAGE);
    var logo = document.createElement('div');
    logo.id = IDS.LOGO;

    fakebox = document.createElement('div');
    fakebox.id = IDS.FAKEBOX;
    fakebox.innerHTML =
        '<input autocomplete="off" tabindex="-1" aria-hidden="true">' +
        '<div id=cursor></div>';

    ntpContents.insertBefore(fakebox, ntpContents.firstChild);
    ntpContents.insertBefore(logo, ntpContents.firstChild);
  }


  var notificationMessage = $(IDS.NOTIFICATION_MESSAGE);
  notificationMessage.textContent = templateData.thumbnailRemovedNotification;
  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  undoLink.textContent = templateData.undoThumbnailRemove;
  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  restoreAllLink.textContent = templateData.restoreThumbnailsShort;
  attribution.textContent = templateData.attributionIntro;

  var notificationCloseButton = $(IDS.NOTIFICATION_CLOSE_BUTTON);
  notificationCloseButton.addEventListener('click', hideNotification);

  window.addEventListener('resize', onResize);
  onResize();

  var topLevelHandle = getEmbeddedSearchApiHandle();

  ntpApiHandle = topLevelHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  onThemeChange();
  onMostVisitedChange();

  searchboxApiHandle = topLevelHandle.searchBox;
  searchboxApiHandle.onnativesuggestions = updateSuggestions;
  searchboxApiHandle.onchange = updateSuggestions;
  searchboxApiHandle.onkeypress = handleKeyPress;
  // TODO(jered): Re-enable this after http://crbug.com/236492 is fixed.
  // searchboxApiHandle.onmarginchange = setSuggestionStyles;
  if (window.innerWidth > 0) {
    setSuggestionStyles();
  } else {
    // Wait until the overlay is shown to initialize suggestion margins.
    window.addEventListener('resize', setSuggestionStyles);
  }
  searchboxApiHandle.onsubmit = function() {
    var value = searchboxApiHandle.value;
    if (!value) {
      // Hide the drop down right away when the user hit enter key on a URL.
      hideActiveSuggestions();
    }
  };

  if (fakebox) {
    // Listener for updating the key capture state.
    document.body.onclick = function(event) {
      if (isFakeboxClick(event))
        searchboxApiHandle.startCapturingKeyStrokes();
      else if (isFakeboxFocused())
        searchboxApiHandle.stopCapturingKeyStrokes();
    };
    searchboxApiHandle.onkeycapturechange = function() {
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
    };

    // Set the cursor alignment based on language directionality.
    $(IDS.CURSOR).style[searchboxApiHandle.rtl ? 'right' : 'left'] = '9px';
  }
}

document.addEventListener('DOMContentLoaded', init);
window.addEventListener('message', handleMessage, false);
window.addEventListener('blur', function() {
  if (activeBox)
    activeBox.clearHover();
}, false);
})();
