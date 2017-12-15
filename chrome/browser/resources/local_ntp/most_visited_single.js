/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Single iframe for NTP tiles.
(function() {
'use strict';


/**
 * The different types of events that are logged from the NTP.  This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
var LOG_TYPE = {
  // All NTP tiles have finished loading (successfully or failing).
  NTP_ALL_TILES_LOADED: 11,
  // The data for all NTP tiles (title, URL, etc, but not the thumbnail image)
  // has been received. In contrast to NTP_ALL_TILES_LOADED, this is recorded
  // before the actual DOM elements have loaded (in particular the thumbnail
  // images).
  NTP_ALL_TILES_RECEIVED: 12,
};


/**
 * The different sources where an NTP tile's title can originate from.
 * Note: Keep in sync with components/ntp_tiles/tile_title_source.h
 * @enum {number}
 * @const
 */
var TileTitleSource = {
  UNKNOWN: 0,
  MANIFEST: 1,
  META_TAG: 2,
  TITLE: 3,
  INFERRED: 4
};


/**
 * The different sources that an NTP tile can have.
 * Note: Keep in sync with components/ntp_tiles/tile_source.h
 * @enum {number}
 * @const
 */
var TileSource = {
  TOP_SITES: 0,
  SUGGESTIONS_SERVICE: 1,
  POPULAR: 3,
  WHITELIST: 4,
};


/**
 * The different (visual) types that an NTP tile can have.
 * Note: Keep in sync with components/ntp_tiles/tile_visual_type.h
 * @enum {number}
 * @const
 */
var TileVisualType = {
  NONE: 0,
  ICON_REAL: 1,
  ICON_COLOR: 2,
  ICON_DEFAULT: 3,
  THUMBNAIL: 7,
  THUMBNAIL_FAILED: 8,
};


/**
 * Total number of tiles to show at any time. If the host page doesn't send
 * enough tiles, we fill them blank.
 * @const {number}
 */
var NUMBER_OF_TILES = 8;


/**
 * Number of lines to display in titles.
 * @type {number}
 */
var NUM_TITLE_LINES = 1;


/**
 * The origin of this request, i.e. 'https://www.google.TLD' for the remote NTP,
 * or 'chrome-search://local-ntp' for the local NTP.
 * @const {string}
 */
var DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * Counter for DOM elements that we are waiting to finish loading. Starts out
 * at 1 because initially we're waiting for the "show" message from the parent.
 * @type {number}
 */
var loadedCounter = 1;


/**
 * DOM element containing the tiles we are going to present next.
 * Works as a double-buffer that is shown when we receive a "show" postMessage.
 * @type {Element}
 */
var tiles = null;


/**
 * List of parameters passed by query args.
 * @type {Object}
 */
var queryArgs = {};


/**
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
var logEvent = function(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
};

/**
 * Log impression of an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < NUMBER_OF_TILES.
 * @param {number} tileTitleSource The title's source from TileTitleSource.
 * @param {number} tileSource The source from TileSource.
 * @param {number} tileType The type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *               produced by a ranking algorithm.
 */
function logMostVisitedImpression(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedImpression(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Log click on an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < NUMBER_OF_TILES.
 * @param {number} tileTitleSource The title's source from TileTitleSource.
 * @param {number} tileSource The source from TileSource.
 * @param {number} tileType The type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *               produced by a ranking algorithm.
 */
function logMostVisitedNavigation(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedNavigation(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Down counts the DOM elements that we are waiting for the page to load.
 * When we get to 0, we send a message to the parent window.
 * This is usually used as an EventListener of onload/onerror.
 */
var countLoad = function() {
  loadedCounter -= 1;
  if (loadedCounter <= 0) {
    swapInNewTiles();
    logEvent(LOG_TYPE.NTP_ALL_TILES_LOADED);
    window.parent.postMessage({cmd: 'loaded'}, DOMAIN_ORIGIN);
    // Reset to 1, so that any further 'show' message will cause us to swap in
    // fresh tiles.
    loadedCounter = 1;
  }
};


/**
 * Handles postMessages coming from the host page to the iframe.
 * Mostly, it dispatches every command to handleCommand.
 */
var handlePostMessage = function(event) {
  if (event.data instanceof Array) {
    for (var i = 0; i < event.data.length; ++i) {
      handleCommand(event.data[i]);
    }
  } else {
    handleCommand(event.data);
  }
};


/**
 * Handles a single command coming from the host page to the iframe.
 * We try to keep the logic here to a minimum and just dispatch to the relevant
 * functions.
 */
var handleCommand = function(data) {
  var cmd = data.cmd;

  if (cmd == 'tile') {
    addTile(data);
  } else if (cmd == 'show') {
    // TODO(treib): If this happens before we have finished loading the previous
    // tiles, we probably get into a bad state.
    showTiles(data);
  } else if (cmd == 'updateTheme') {
    updateTheme(data);
  } else {
    console.error('Unknown command: ' + JSON.stringify(data));
  }
};


/**
 * Handler for the 'show' message from the host page.
 * @param {object} info Data received in the message.
 */
var showTiles = function(info) {
  logEvent(LOG_TYPE.NTP_ALL_TILES_RECEIVED);
  countLoad();
};


/**
 * Handler for the 'updateTheme' message from the host page.
 * @param {object} info Data received in the message.
 */
var updateTheme = function(info) {
  document.body.style.setProperty('--tile-title-color', info.tileTitleColor);
  document.body.classList.toggle('dark-theme', info.isThemeDark);
};


/**
 * Removes all old instances of #mv-tiles that are pending for deletion.
 */
var removeAllOldTiles = function() {
  var parent = document.querySelector('#most-visited');
  var oldList = parent.querySelectorAll('.mv-tiles-old');
  for (var i = 0; i < oldList.length; ++i) {
    parent.removeChild(oldList[i]);
  }
};


/**
 * Called when all tiles have finished loading (successfully or not), including
 * their thumbnail images, and we are ready to show the new tiles and drop the
 * old ones.
 */
var swapInNewTiles = function() {
  // Store the tiles on the current closure.
  var cur = tiles;

  // Create empty tiles until we have NUMBER_OF_TILES.
  while (cur.childNodes.length < NUMBER_OF_TILES) {
    addTile({});
  }

  var parent = document.querySelector('#most-visited');

  // Only fade in the new tiles if there were tiles before.
  var fadeIn = false;
  var old = parent.querySelector('#mv-tiles');
  if (old) {
    fadeIn = true;
    // Mark old tile DIV for removal after the transition animation is done.
    old.removeAttribute('id');
    old.classList.add('mv-tiles-old');
    old.style.opacity = 0.0;
    cur.addEventListener('transitionend', function(ev) {
      if (ev.target === cur) {
        removeAllOldTiles();
      }
    });
  }

  // Add new tileset.
  cur.id = 'mv-tiles';
  parent.appendChild(cur);
  // getComputedStyle causes the initial style (opacity 0) to be applied, so
  // that when we then set it to 1, that triggers the CSS transition.
  if (fadeIn) {
    window.getComputedStyle(cur).opacity;
  }
  cur.style.opacity = 1.0;

  // Make sure the tiles variable contain the next tileset we'll use if the host
  // page sends us an updated set of tiles.
  tiles = document.createElement('div');
};


/**
 * Handler for the 'show' message from the host page, called when it wants to
 * add a suggestion tile.
 * It's also used to fill up our tiles to NUMBER_OF_TILES if necessary.
 * @param {object} args Data for the tile to be rendered.
 */
var addTile = function(args) {
  if (isFinite(args.rid)) {
    // An actual suggestion. Grab the data from the embeddedSearch API.
    var data =
        chrome.embeddedSearch.newTabPage.getMostVisitedItemData(args.rid);
    if (!data)
      return;

    data.tid = data.rid;
    if (!data.faviconUrl) {
      data.faviconUrl = 'chrome-search://favicon/size/16@' +
          window.devicePixelRatio + 'x/' + data.renderViewId + '/' + data.tid;
    }
    tiles.appendChild(renderTile(data));
  } else {
    // An empty tile
    tiles.appendChild(renderTile(null));
  }
};

/**
 * Called when the user decided to add a tile to the blacklist.
 * It sets off the animation for the blacklist and sends the blacklisted id
 * to the host page.
 * @param {Element} tile DOM node of the tile we want to remove.
 */
var blacklistTile = function(tile) {
  tile.classList.add('blacklisted');
  tile.addEventListener('transitionend', function(ev) {
    if (ev.propertyName != 'width')
      return;

    window.parent.postMessage(
        {cmd: 'tileBlacklisted', tid: Number(tile.getAttribute('data-tid'))},
        DOMAIN_ORIGIN);
  });
};


/**
 * Returns whether the given URL has a known, safe scheme.
 * @param {string} url URL to check.
 */
var isSchemeAllowed = function(url) {
  return url.startsWith('http://') || url.startsWith('https://') ||
      url.startsWith('ftp://') || url.startsWith('chrome-extension://');
};


/**
 * Renders a MostVisited tile to the DOM.
 * @param {object} data Object containing rid, url, title, favicon, thumbnail.
 *     data is null if you want to construct an empty tile.
 */
var renderTile = function(data) {
  var tile = document.createElement('a');

  if (data == null) {
    tile.className = 'mv-empty-tile';
    return tile;
  }

  // The tile will be appended to tiles.
  var position = tiles.children.length;

  // This is set in the load/error event for the thumbnail image.
  var tileType = TileVisualType.NONE;

  tile.className = 'mv-tile';
  tile.setAttribute('data-tid', data.tid);

  if (isSchemeAllowed(data.url)) {
    tile.href = data.url;
  }
  tile.setAttribute('aria-label', data.title);
  tile.title = data.title;

  tile.addEventListener('click', function(ev) {
    logMostVisitedNavigation(
        position, data.tileTitleSource, data.tileSource, tileType,
        data.dataGenerationTime);
  });

  tile.addEventListener('keydown', function(event) {
    if (event.keyCode == 46 /* DELETE */ ||
        event.keyCode == 8 /* BACKSPACE */) {
      event.preventDefault();
      event.stopPropagation();
      blacklistTile(this);
    } else if (
        event.keyCode == 13 /* ENTER */ || event.keyCode == 32 /* SPACE */) {
      event.preventDefault();
      this.click();
    } else if (event.keyCode >= 37 && event.keyCode <= 40 /* ARROWS */) {
      // specify the direction of movement
      var inArrowDirection = function(origin, target) {
        return (event.keyCode == 37 /* LEFT */ &&
                origin.offsetTop == target.offsetTop &&
                origin.offsetLeft > target.offsetLeft) ||
            (event.keyCode == 38 /* UP */ &&
             origin.offsetTop > target.offsetTop &&
             origin.offsetLeft == target.offsetLeft) ||
            (event.keyCode == 39 /* RIGHT */ &&
             origin.offsetTop == target.offsetTop &&
             origin.offsetLeft < target.offsetLeft) ||
            (event.keyCode == 40 /* DOWN */ &&
             origin.offsetTop < target.offsetTop &&
             origin.offsetLeft == target.offsetLeft);
      };

      var nonEmptyTiles = document.querySelectorAll('#mv-tiles .mv-tile');
      var nextTile = null;
      // Find the closest tile in the appropriate direction.
      for (var i = 0; i < nonEmptyTiles.length; i++) {
        if (inArrowDirection(this, nonEmptyTiles[i]) &&
            (!nextTile || inArrowDirection(nonEmptyTiles[i], nextTile))) {
          nextTile = nonEmptyTiles[i];
        }
      }
      if (nextTile) {
        nextTile.focus();
      }
    }
  });

  var favicon = document.createElement('div');
  favicon.className = 'mv-favicon';
  var fi = document.createElement('img');
  fi.src = data.faviconUrl;
  // Set title and alt to empty so screen readers won't say the image name.
  fi.title = '';
  fi.alt = '';
  loadedCounter += 1;
  fi.addEventListener('load', countLoad);
  fi.addEventListener('error', countLoad);
  fi.addEventListener('error', function(ev) {
    favicon.classList.add('failed-favicon');
  });
  favicon.appendChild(fi);
  tile.appendChild(favicon);

  var title = document.createElement('div');
  title.className = 'mv-title';
  title.innerText = data.title;
  title.style.direction = data.direction || 'ltr';
  if (NUM_TITLE_LINES > 1) {
    title.classList.add('multiline');
  }
  tile.appendChild(title);

  var thumb = document.createElement('div');
  thumb.className = 'mv-thumb';
  var img = document.createElement('img');
  img.title = data.title;
  img.src = data.thumbnailUrl;
  loadedCounter += 1;
  img.addEventListener('load', function(ev) {
    // Store the type for a potential later navigation.
    tileType = TileVisualType.THUMBNAIL;
    logMostVisitedImpression(
        position, data.tileTitleSource, data.tileSource, tileType,
        data.dataGenerationTime);
    // Note: It's important to call countLoad last, because that might emit the
    // NTP_ALL_TILES_LOADED event, which must happen after the impression log.
    countLoad();
  });
  img.addEventListener('error', function(ev) {
    thumb.classList.add('failed-img');
    thumb.removeChild(img);
    // Store the type for a potential later navigation.
    tileType = TileVisualType.THUMBNAIL_FAILED;
    logMostVisitedImpression(
        position, data.tileTitleSource, data.tileSource, tileType,
        data.dataGenerationTime);
    // Note: It's important to call countLoad last, because that might emit the
    // NTP_ALL_TILES_LOADED event, which must happen after the impression log.
    countLoad();
  });
  thumb.appendChild(img);
  tile.appendChild(thumb);

  var mvx = document.createElement('button');
  mvx.className = 'mv-x';
  mvx.title = queryArgs['removeTooltip'] || '';
  mvx.addEventListener('click', function(ev) {
    removeAllOldTiles();
    blacklistTile(tile);
    ev.preventDefault();
    ev.stopPropagation();
  });
  // Don't allow the event to bubble out to the containing tile, as that would
  // trigger navigation to the tile URL.
  mvx.addEventListener('keydown', function(event) {
    event.stopPropagation();
  });
  tile.appendChild(mvx);

  return tile;
};


/**
 * Does some initialization and parses the query arguments passed to the iframe.
 */
var init = function() {
  // Create a new DOM element to hold the tiles. The tiles will be added
  // one-by-one via addTile, and the whole thing will be inserted into the page
  // in swapInNewTiles, after the parent has sent us the 'show' message, and all
  // thumbnails and favicons have loaded.
  tiles = document.createElement('div');

  // Parse query arguments.
  var query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (var i = 0; i < query.length; ++i) {
    var val = query[i].split('=');
    if (val[0] == '')
      continue;
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  if ('ntl' in queryArgs) {
    var ntl = parseInt(queryArgs['ntl'], 10);
    if (isFinite(ntl))
      NUM_TITLE_LINES = ntl;
  }

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    var html = document.querySelector('html');
    html.dir = 'rtl';
  }

  window.addEventListener('message', handlePostMessage);
};


window.addEventListener('DOMContentLoaded', init);
})();
