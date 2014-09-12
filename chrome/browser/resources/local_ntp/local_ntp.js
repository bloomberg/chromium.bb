// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview The local InstantExtended NTP.
 */


/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
<include src="../../../../ui/webui/resources/js/assert.js">
<include src="local_ntp_design.js">
<include src="local_ntp_util.js">
<include src="window_disposition_util.js">


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo', // Shows white logo if required by theme
  BLACKLIST: 'mv-blacklist', // triggers tile blacklist animation
  BLACKLIST_BUTTON: 'mv-x',
  BLACKLIST_BUTTON_INNER: 'mv-x-inner',
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  DOT: 'dot',
  FAKEBOX_DISABLE: 'fakebox-disable', // Makes fakebox non-interactive
  FAKEBOX_FOCUS: 'fakebox-focused', // Applies focus styles to the fakebox
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  FAVICON: 'mv-favicon',
  FAVICON_FALLBACK: 'mv-favicon-fallback',
  FOCUSED: 'mv-focused',
  HIDE_BLACKLIST_BUTTON: 'mv-x-hide', // hides blacklist button during animation
  HIDE_FAKEBOX_AND_LOGO: 'hide-fakebox-logo',
  HIDE_NOTIFICATION: 'mv-notice-hide',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  PAGE: 'mv-page', // page tiles
  PAGE_READY: 'mv-page-ready',  // page tile when ready
  RTL: 'rtl',  // Right-to-left language text.
  THUMBNAIL: 'mv-thumb',
  THUMBNAIL_FALLBACK: 'mv-thumb-fallback',
  THUMBNAIL_MASK: 'mv-mask',
  TILE: 'mv-tile',
  TILE_INNER: 'mv-tile-inner',
  TITLE: 'mv-title'
};


/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
var IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
  CUSTOM_THEME_STYLE: 'ct-style',
  FAKEBOX: 'fakebox',
  FAKEBOX_INPUT: 'fakebox-input',
  FAKEBOX_TEXT: 'fakebox-text',
  LOGO: 'logo',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  RESTORE_ALL_LINK: 'mv-restore',
  TILES: 'mv-tiles',
  UNDO_LINK: 'mv-undo'
};


/**
 * Enum for keycodes.
 * @enum {number}
 * @const
 */
var KEYCODE = {
  ENTER: 13
};


/**
 * Enum for the state of the NTP when it is disposed.
 * @enum {number}
 * @const
 */
var NTP_DISPOSE_STATE = {
  NONE: 0,  // Preserve the NTP appearance and functionality
  DISABLE_FAKEBOX: 1,
  HIDE_FAKEBOX_AND_LOGO: 2
};


/**
 * The JavaScript button event value for a middle click.
 * @type {number}
 * @const
 */
var MIDDLE_MOUSE_BUTTON = 1;


/**
 * Specifications for the NTP design.
 * @const {NtpDesign}
 */
var NTP_DESIGN = getNtpDesign(configData.ntpDesignName);


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
 * The container for NTP elements.
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
 * The iframe element which is currently keyboard focused, or null.
 * @type {?Element}
 */
var focusedIframe = null;


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
 * A flag to indicate Most Visited changed caused by user action. If true, then
 * in onMostVisitedChange() tiles remain visible so no flickering occurs.
 * @type {boolean}
 */
var userInitiatedMostVisitedChange = false;


/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/**
 * The browser embeddedSearch.searchBox object.
 * @type {Object}
 */
var searchboxApiHandle;


/**
 * The state of the NTP when a query is entered into the Omnibox.
 * @type {NTP_DISPOSE_STATE}
 */
var omniboxInputBehavior = NTP_DISPOSE_STATE.NONE;


/**
 * The state of the NTP when a query is entered into the Fakebox.
 * @type {NTP_DISPOSE_STATE}
 */
var fakeboxInputBehavior = NTP_DISPOSE_STATE.HIDE_FAKEBOX_AND_LOGO;


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
 * The color of the title in RRGGBBAA format.
 * @type {?string}
 */
var titleColor = null;


/**
 * Hide most visited tiles for at most this many milliseconds while painting.
 * @type {number}
 * @const
 */
var MOST_VISITED_PAINT_TIMEOUT_MSEC = 500;


/**
 * A Tile is either a rendering of a Most Visited page or "filler" used to
 * pad out the section when not enough pages exist.
 *
 * @param {Element} elem The element for rendering the tile.
 * @param {Element=} opt_innerElem The element for contents of tile.
 * @param {Element=} opt_titleElem The element for rendering the title.
 * @param {Element=} opt_thumbnailElem The element for rendering the thumbnail.
 * @param {number=} opt_rid The RID for the corresponding Most Visited page.
 *     Should only be left unspecified when creating a filler tile.
 * @constructor
 */
function Tile(elem, opt_innerElem, opt_titleElem, opt_thumbnailElem, opt_rid) {
  /** @type {Element} */
  this.elem = elem;

  /** @type {Element|undefined} */
  this.innerElem = opt_innerElem;

  /** @type {Element|undefined} */
  this.titleElem = opt_titleElem;

  /** @type {Element|undefined} */
  this.thumbnailElem = opt_thumbnailElem;

  /** @type {number|undefined} */
  this.rid = opt_rid;
}


/**
 * Heuristic to determine whether a theme should be considered to be dark, so
 * the colors of various UI elements can be adjusted.
 * @param {ThemeBackgroundInfo|undefined} info Theme background information.
 * @return {boolean} Whether the theme is dark.
 * @private
 */
function getIsThemeDark(info) {
  if (!info)
    return false;
  // Heuristic: light text implies dark theme.
  var rgba = info.textColorRgba;
  var luminance = 0.3 * rgba[0] + 0.59 * rgba[1] + 0.11 * rgba[2];
  return luminance >= 128;
}


/**
 * Updates the NTP based on the current theme.
 * @private
 */
function renderTheme() {
  var fakeboxText = $(IDS.FAKEBOX_TEXT);
  if (fakeboxText) {
    fakeboxText.innerHTML = '';
    if (NTP_DESIGN.showFakeboxHint &&
        configData.translatedStrings.searchboxPlaceholder) {
      fakeboxText.textContent =
          configData.translatedStrings.searchboxPlaceholder;
    }
  }

  var info = ntpApiHandle.themeBackgroundInfo;
  var isThemeDark = getIsThemeDark(info);
  ntpContents.classList.toggle(CLASSES.DARK, isThemeDark);
  if (!info) {
    titleColor = NTP_DESIGN.titleColor;
    return;
  }

  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = convertToRRGGBBAAColor(info.textColorRgba);
  } else {
    titleColor = isThemeDark ?
        NTP_DESIGN.titleColorAgainstDark : NTP_DESIGN.titleColor;
  }

  var background = [convertToRGBAColor(info.backgroundColorRgba),
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();

  document.body.style.background = background;
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, info.alternateLogo);
  updateThemeAttribution(info.attributionUrl);
  setCustomThemeStyle(info);
}


/**
 * Updates the NTP based on the current theme, then rerenders all tiles.
 * @private
 */
function onThemeChange() {
  renderTheme();
  tilesContainer.innerHTML = '';
  renderAndShowTiles();
}


/**
 * Updates the NTP style according to theme.
 * @param {Object=} opt_themeInfo The information about the theme. If it is
 * omitted the style will be reverted to the default.
 * @private
 */
function setCustomThemeStyle(opt_themeInfo) {
  var customStyleElement = $(IDS.CUSTOM_THEME_STYLE);
  var head = document.head;
  if (opt_themeInfo && !opt_themeInfo.usingDefaultTheme) {
    ntpContents.classList.remove(CLASSES.DEFAULT_THEME);
    var themeStyle =
      '#attribution {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-msg {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorRgba) + ';' +
      '}' +
      '#mv-notice-links span {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-notice-x {' +
      '  -webkit-filter: drop-shadow(0 0 0 ' +
          convertToRGBAColor(opt_themeInfo.textColorRgba) + ');' +
      '}' +
      '.mv-page-ready .mv-mask {' +
      '  border: 1px solid ' +
          convertToRGBAColor(opt_themeInfo.sectionBorderColorRgba) + ';' +
      '}' +
      '.mv-page-ready:hover .mv-mask, .mv-page-ready .mv-focused ~ .mv-mask {' +
      '  border-color: ' +
          convertToRGBAColor(opt_themeInfo.headerColorRgba) + ';' +
      '}';

    if (customStyleElement) {
      customStyleElement.textContent = themeStyle;
    } else {
      customStyleElement = document.createElement('style');
      customStyleElement.type = 'text/css';
      customStyleElement.id = IDS.CUSTOM_THEME_STYLE;
      customStyleElement.textContent = themeStyle;
      head.appendChild(customStyleElement);
    }

  } else {
    ntpContents.classList.add(CLASSES.DEFAULT_THEME);
    if (customStyleElement)
      head.removeChild(customStyleElement);
  }
}


/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string} url The URL of the attribution image, if any.
 * @private
 */
function updateThemeAttribution(url) {
  if (!url) {
    setAttributionVisibility_(false);
    return;
  }

  var attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;
  setAttributionVisibility_(true);
}


/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 * @private
 */
function setAttributionVisibility_(show) {
  if (attribution) {
    attribution.style.display = show ? '' : 'none';
  }
}


 /**
 * Converts an Array of color components into RRGGBBAA format.
 * @param {Array.<number>} color Array of rgba color components.
 * @return {string} Color string in RRGGBBAA format.
 * @private
 */
function convertToRRGGBBAAColor(color) {
  return color.map(function(t) {
    return ('0' + t.toString(16)).slice(-2);  // To 2-digit, 0-padded hex.
  }).join('');
}


 /**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array.<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 * @private
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
                    color[3] / 255 + ')';
}


/**
 * Handles a new set of Most Visited page data.
 */
function onMostVisitedChange() {
  if (isBlacklisting) {
    // Trigger the blacklist animation, which then triggers reloadAllTiles().
    var lastBlacklistedTileElem = lastBlacklistedTile.elem;
    lastBlacklistedTileElem.addEventListener(
        'webkitTransitionEnd', blacklistAnimationDone);
    lastBlacklistedTileElem.classList.add(CLASSES.BLACKLIST);
  } else {
    reloadAllTiles();
  }
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
  reloadAllTiles();
}


/**
 * Fetches new data, creates, and renders tiles.
 */
function reloadAllTiles() {
  var pages = ntpApiHandle.mostVisited;

  tiles = [];
  for (var i = 0; i < MAX_NUM_TILES_TO_SHOW; ++i)
    tiles.push(createTile(pages[i], i));

  tilesContainer.innerHTML = '';
  renderAndShowTiles();
}


/**
 * Binds onload events for a tile's internal <iframe> elements.
 * @param {Tile} tile The main tile to bind events to.
 * @param {Barrier} tileVisibilityBarrier A barrier to make all tiles visible
 *   the moment all tiles are loaded.
 */
function bindTileOnloadEvents(tile, tileVisibilityBarrier) {
  if (tile.titleElem) {
    tileVisibilityBarrier.add();
    tile.titleElem.onload = function() {
      tileVisibilityBarrier.remove();
    };
  }
  if (tile.thumbnailElem) {
    tileVisibilityBarrier.add();
    tile.thumbnailElem.onload = function() {
      tile.elem.classList.add(CLASSES.PAGE_READY);
      tileVisibilityBarrier.remove();
    };
  }
}


/**
 * Renders the current list of visible tiles to DOM, and hides tiles that are
 * already in the DOM but should not be seen.
 */
function renderAndShowTiles() {
  var numExisting = tilesContainer.querySelectorAll('.' + CLASSES.TILE).length;
  // Only add visible tiles to the DOM, to avoid creating invisible tiles that
  // produce meaningless impression metrics. However, if a tile becomes
  // invisible then we leave it in DOM to prevent reload if it's shown again.
  var numDesired = Math.min(tiles.length, numColumnsShown * NUM_ROWS);

  // If we need to render new tiles, manage the visibility to hide intermediate
  // load states of the <iframe>s.
  if (numExisting < numDesired) {
    var showAll = function() {
      for (var i = 0; i < numDesired; ++i) {
        if (tiles[i].titleElem || tiles[i].thumbnailElem)
          tiles[i].elem.classList.add(CLASSES.PAGE_READY);
      }
    };
    var tileVisibilityBarrier = new Barrier(showAll);

    if (!userInitiatedMostVisitedChange) {
      // Make titleContainer invisible, but still taking up space.
      // titleContainer becomes visible again (1) on timeout, or (2) when all
      // tiles finish loading (using tileVisibilityBarrier).
      window.setTimeout(function() {
        tileVisibilityBarrier.cancel();
        showAll();
      }, MOST_VISITED_PAINT_TIMEOUT_MSEC);
    }
    userInitiatedMostVisitedChange = false;

    for (var i = numExisting; i < numDesired; ++i) {
      bindTileOnloadEvents(tiles[i], tileVisibilityBarrier);
      tilesContainer.appendChild(tiles[i].elem);
    }
  }

  // Show only the desired tiles. Note that .hidden does not work for
  // inline-block elements like tiles[i].elem.
  for (var i = 0; i < numDesired; ++i)
    tiles[i].elem.style.display = 'inline-block';
  // If |numDesired| < |numExisting| then hide extra tiles (e.g., this occurs
  // when window is downsized).
  for (; i < numExisting; ++i)
    tiles[i].elem.style.display = 'none';
}


/**
 * Builds a URL to display a most visited tile title in an iframe.
 * @param {number} rid The restricted ID.
 * @param {number} position The position of the iframe in the UI.
 * @return {string} An URL to display the most visited title in an iframe.
 */
function getMostVisitedTitleIframeUrl(rid, position) {
  var url = 'chrome-search://most-visited/' +
      encodeURIComponent(MOST_VISITED_TITLE_IFRAME);
  var params = [
      'rid=' + encodeURIComponent(rid),
      'f=' + encodeURIComponent(NTP_DESIGN.fontFamily),
      'fs=' + encodeURIComponent(NTP_DESIGN.fontSize),
      'c=' + encodeURIComponent(titleColor),
      'pos=' + encodeURIComponent(position)];
  if (NTP_DESIGN.titleTextAlign)
    params.push('ta=' + encodeURIComponent(NTP_DESIGN.titleTextAlign));
  if (NTP_DESIGN.titleTextFade)
    params.push('tf=' + encodeURIComponent(NTP_DESIGN.titleTextFade));
  return url + '?' + params.join('&');
}


/**
 * Builds a URL to display a most visited tile thumbnail in an iframe.
 * @param {number} rid The restricted ID.
 * @param {number} position The position of the iframe in the UI.
 * @return {string} An URL to display the most visited thumbnail in an iframe.
 */
function getMostVisitedThumbnailIframeUrl(rid, position) {
  var url = 'chrome-search://most-visited/' +
      encodeURIComponent(MOST_VISITED_THUMBNAIL_IFRAME);
  var params = [
      'rid=' + encodeURIComponent(rid),
      'f=' + encodeURIComponent(NTP_DESIGN.fontFamily),
      'fs=' + encodeURIComponent(NTP_DESIGN.fontSize),
      'c=' + encodeURIComponent(NTP_DESIGN.thumbnailTextColor),
      'pos=' + encodeURIComponent(position)];
  if (NTP_DESIGN.thumbnailFallback)
    params.push('etfb=1');
  return url + '?' + params.join('&');
}


/**
 * Creates a Tile with the specified page data. If no data is provided, a
 * filler Tile is created.
 * @param {Object} page The page data.
 * @param {number} position The position of the tile.
 * @return {Tile} The new Tile.
 */
function createTile(page, position) {
  var tileElem = document.createElement('div');
  tileElem.classList.add(CLASSES.TILE);
  // Prevent tile from being selected (and highlighted) when areas outside the
  // <iframe>s are clicked.
  tileElem.addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
  var innerElem = createAndAppendElement(tileElem, 'div', CLASSES.TILE_INNER);

  if (page) {
    var rid = page.rid;
    tileElem.classList.add(CLASSES.PAGE);

    var navigateFunction = function(e) {
      e.preventDefault();
      ntpApiHandle.navigateContentWindow(rid, getDispositionFromEvent(e));
    };

    // The click handler for navigating to the page identified by the RID.
    tileElem.addEventListener('click', navigateFunction);

    // The iframe which renders the page title.
    var titleElem = document.createElement('iframe');
    // Enable tab navigation on the iframe, which will move the selection to the
    // link element (which also has a tabindex).
    titleElem.tabIndex = '0';

    // Why iframes have IDs:
    //
    // On navigating back to the NTP we see several onmostvisitedchange() events
    // in series with incrementing RIDs. After the first event, a set of iframes
    // begins loading RIDs n, n+1, ..., n+k-1; after the second event, these get
    // destroyed and a new set begins loading RIDs n+k, n+k+1, ..., n+2k-1.
    // Now due to crbug.com/68841, Chrome incorrectly loads the content for the
    // first set of iframes into the most recent set of iframes.
    //
    // Giving iframes distinct ids seems to cause some invalidation and prevent
    // associating the incorrect data.
    //
    // TODO(jered): Find and fix the root (probably Blink) bug.

    // Keep this ID here. See comment above.
    titleElem.id = 'title-' + rid;
    titleElem.className = CLASSES.TITLE;
    titleElem.src = getMostVisitedTitleIframeUrl(rid, position);
    innerElem.appendChild(titleElem);

    // A fallback element for missing thumbnails.
    if (NTP_DESIGN.thumbnailFallback) {
      var fallbackElem = createAndAppendElement(
          innerElem, 'div', CLASSES.THUMBNAIL_FALLBACK);
      if (NTP_DESIGN.thumbnailFallback === THUMBNAIL_FALLBACK.DOT)
        createAndAppendElement(fallbackElem, 'div', CLASSES.DOT);
    }

    // The iframe which renders either a thumbnail or domain element.
    var thumbnailElem = document.createElement('iframe');
    thumbnailElem.tabIndex = '-1';
    thumbnailElem.setAttribute('aria-hidden', 'true');
    // Keep this ID here. See comment above.
    thumbnailElem.id = 'thumb-' + rid;
    thumbnailElem.className = CLASSES.THUMBNAIL;
    thumbnailElem.src = getMostVisitedThumbnailIframeUrl(rid, position);
    innerElem.appendChild(thumbnailElem);

    // The button used to blacklist this page.
    var blacklistButton = createAndAppendElement(
        innerElem, 'div', CLASSES.BLACKLIST_BUTTON);
    createAndAppendElement(
        blacklistButton, 'div', CLASSES.BLACKLIST_BUTTON_INNER);
    var blacklistFunction = generateBlacklistFunction(rid);
    blacklistButton.addEventListener('click', blacklistFunction);
    blacklistButton.title = configData.translatedStrings.removeThumbnailTooltip;

    // A helper mask on top of the tile that is used to create hover border
    // and/or to darken the thumbnail on focus.
    var maskElement = createAndAppendElement(
        innerElem, 'div', CLASSES.THUMBNAIL_MASK);

    // The page favicon, or a fallback.
    var favicon = createAndAppendElement(innerElem, 'div', CLASSES.FAVICON);
    if (page.faviconUrl) {
      favicon.style.backgroundImage = 'url(' + page.faviconUrl + ')';
    } else {
      favicon.classList.add(CLASSES.FAVICON_FALLBACK);
    }
    return new Tile(tileElem, innerElem, titleElem, thumbnailElem, rid);
  } else {
    return new Tile(tileElem);
  }
}


/**
 * Generates a function to be called when the page with the corresponding RID
 * is blacklisted.
 * @param {number} rid The RID of the page being blacklisted.
 * @return {function(Event=)} A function which handles the blacklisting of the
 *     page by updating state variables and notifying Chrome.
 */
function generateBlacklistFunction(rid) {
  return function(e) {
    // Prevent navigation when the page is being blacklisted.
    if (e)
      e.stopPropagation();

    userInitiatedMostVisitedChange = true;
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
  notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
}


/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  userInitiatedMostVisitedChange = true;
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
  userInitiatedMostVisitedChange = true;
  hideNotification();
  ntpApiHandle.undoAllMostVisitedDeletions();
}


/**
 * Recomputes the number of tile columns, and width of various contents based
 * on the width of the window.
 * @return {boolean} Whether the number of tile columns has changed.
 */
function updateContentWidth() {
  var tileRequiredWidth = NTP_DESIGN.tileWidth + NTP_DESIGN.tileMargin;
  // If innerWidth is zero, then use the maximum snap size.
  var maxSnapSize = MAX_NUM_COLUMNS * tileRequiredWidth -
      NTP_DESIGN.tileMargin + MIN_TOTAL_HORIZONTAL_PADDING;
  var innerWidth = window.innerWidth || maxSnapSize;
  // Each tile has left and right margins that sum to NTP_DESIGN.tileMargin.
  var availableWidth = innerWidth + NTP_DESIGN.tileMargin -
      MIN_TOTAL_HORIZONTAL_PADDING;
  var newNumColumns = Math.floor(availableWidth / tileRequiredWidth);
  if (newNumColumns < MIN_NUM_COLUMNS)
    newNumColumns = MIN_NUM_COLUMNS;
  else if (newNumColumns > MAX_NUM_COLUMNS)
    newNumColumns = MAX_NUM_COLUMNS;

  if (numColumnsShown === newNumColumns)
    return false;

  numColumnsShown = newNumColumns;
  var tilesContainerWidth = numColumnsShown * tileRequiredWidth;
  tilesContainer.style.width = tilesContainerWidth + 'px';
  if (fakebox) {
    // -2 to account for border.
    var fakeboxWidth = (tilesContainerWidth - NTP_DESIGN.tileMargin - 2);
    fakebox.style.width = fakeboxWidth + 'px';
  }
  return true;
}


/**
 * Resizes elements because the number of tile columns may need to change in
 * response to resizing. Also shows or hides extra tiles tiles according to the
 * new width of the page.
 */
function onResize() {
  if (updateContentWidth()) {
    // Render without clearing tiles.
    renderAndShowTiles();
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
 * Handles new input by disposing the NTP, according to where the input was
 * entered.
 */
function onInputStart() {
  if (fakebox && isFakeboxFocused()) {
    setFakeboxFocus(false);
    setFakeboxDragFocus(false);
    disposeNtp(true);
  } else if (!isFakeboxFocused()) {
    disposeNtp(false);
  }
}


/**
 * Disposes the NTP, according to where the input was entered.
 * @param {boolean} wasFakeboxInput True if the input was in the fakebox.
 */
function disposeNtp(wasFakeboxInput) {
  var behavior = wasFakeboxInput ? fakeboxInputBehavior : omniboxInputBehavior;
  if (behavior == NTP_DISPOSE_STATE.DISABLE_FAKEBOX)
    setFakeboxActive(false);
  else if (behavior == NTP_DISPOSE_STATE.HIDE_FAKEBOX_AND_LOGO)
    setFakeboxAndLogoVisibility(false);
}


/**
 * Restores the NTP (re-enables the fakebox and unhides the logo.)
 */
function restoreNtp() {
  setFakeboxActive(true);
  setFakeboxAndLogoVisibility(true);
}


/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} focus True to show a dragging focus to the fakebox.
 */
function setFakeboxDragFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DRAG_FOCUS, focus);
}

/**
 * @return {boolean} True if the fakebox has focus.
 */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS) ||
      document.body.classList.contains(CLASSES.FAKEBOX_DRAG_FOCUS);
}


/**
 * @param {boolean} enable True to enable the fakebox.
 */
function setFakeboxActive(enable) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DISABLE, !enable);
}


/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return fakebox.contains(event.target) &&
      !document.body.classList.contains(CLASSES.FAKEBOX_DISABLE);
}


/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxAndLogoVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX_AND_LOGO, !show);
}


/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
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
 * @param {!Element} element The element to register the handler for.
 * @param {number} keycode The keycode of the key to register.
 * @param {!Function} handler The key handler to register.
 */
function registerKeyHandler(element, keycode, handler) {
  element.addEventListener('keydown', function(event) {
    if (event.keyCode == keycode)
      handler(event);
  });
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
 * Event handler for the focus changed and blacklist messages on link elements.
 * Used to toggle visual treatment on the tiles (depending on the message).
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  if (event.origin !== 'chrome-search://most-visited')
    return;

  if (event.data === 'linkFocused') {
    var activeElement = document.activeElement;
    if (activeElement.classList.contains(CLASSES.TITLE)) {
      activeElement.classList.add(CLASSES.FOCUSED);
      focusedIframe = activeElement;
    }
  } else if (event.data === 'linkBlurred') {
    if (focusedIframe)
      focusedIframe.classList.remove(CLASSES.FOCUSED);
    focusedIframe = null;
  } else if (event.data.indexOf('tileBlacklisted') === 0) {
    var tilePosition = event.data.split(',')[1];
    if (tilePosition)
      generateBlacklistFunction(tiles[parseInt(tilePosition, 10)].rid)();
  }
}


/**
 * Prepares the New Tab Page by adding listeners, rendering the current
 * theme, the most visited pages section, and Google-specific elements for a
 * Google-provided page.
 */
function init() {
  tilesContainer = $(IDS.TILES);
  notification = $(IDS.NOTIFICATION);
  attribution = $(IDS.ATTRIBUTION);
  ntpContents = $(IDS.NTP_CONTENTS);

  if (configData.isGooglePage) {
    var logo = document.createElement('div');
    logo.id = IDS.LOGO;

    fakebox = document.createElement('div');
    fakebox.id = IDS.FAKEBOX;
    var fakeboxHtml = [];
    fakeboxHtml.push('<input id="' + IDS.FAKEBOX_INPUT +
        '" autocomplete="off" tabindex="-1" aria-hidden="true">');
    fakeboxHtml.push('<div id="' + IDS.FAKEBOX_TEXT + '"></div>');
    fakeboxHtml.push('<div id="cursor"></div>');
    fakebox.innerHTML = fakeboxHtml.join('');

    ntpContents.insertBefore(fakebox, ntpContents.firstChild);
    ntpContents.insertBefore(logo, ntpContents.firstChild);
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  // Hide notifications after fade out, so we can't focus on links via keyboard.
  notification.addEventListener('webkitTransitionEnd', hideNotification);

  var notificationMessage = $(IDS.NOTIFICATION_MESSAGE);
  notificationMessage.textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, KEYCODE.ENTER, onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.ENTER, onUndo);
  restoreAllLink.textContent =
      configData.translatedStrings.restoreThumbnailsShort;

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  var notificationCloseButton = $(IDS.NOTIFICATION_CLOSE_BUTTON);
  createAndAppendElement(
      notificationCloseButton, 'div', CLASSES.BLACKLIST_BUTTON_INNER);
  notificationCloseButton.addEventListener('click', hideNotification);

  window.addEventListener('resize', onResize);
  updateContentWidth();

  var topLevelHandle = getEmbeddedSearchApiHandle();

  ntpApiHandle = topLevelHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  ntpApiHandle.oninputstart = onInputStart;
  ntpApiHandle.oninputcancel = restoreNtp;

  if (ntpApiHandle.isInputInProgress)
    onInputStart();

  renderTheme();
  onMostVisitedChange();

  searchboxApiHandle = topLevelHandle.searchBox;

  if (fakebox) {
    // Listener for updating the key capture state.
    document.body.onmousedown = function(event) {
      if (isFakeboxClick(event))
        searchboxApiHandle.startCapturingKeyStrokes();
      else if (isFakeboxFocused())
        searchboxApiHandle.stopCapturingKeyStrokes();
    };
    searchboxApiHandle.onkeycapturechange = function() {
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
    };
    var inputbox = $(IDS.FAKEBOX_INPUT);
    if (inputbox) {
      inputbox.onpaste = function(event) {
        event.preventDefault();
        searchboxApiHandle.paste();
      };
      inputbox.ondrop = function(event) {
        event.preventDefault();
        var text = event.dataTransfer.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
      };
      inputbox.ondragenter = function() {
        setFakeboxDragFocus(true);
      };
      inputbox.ondragleave = function() {
        setFakeboxDragFocus(false);
      };
    }

    // Update the fakebox style to match the current key capturing state.
    setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
    document.body.setAttribute('dir', 'rtl');
    // Add class for setting alignments based on language directionality.
    document.body.classList.add(CLASSES.RTL);
    $(IDS.TILES).dir = 'rtl';
  }

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}

return {
  init: init,
  listen: listen
};
}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
