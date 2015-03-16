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
  // The suggestion is coming from the server. Unused here.
  NTP_SERVER_SIDE_SUGGESTION: 0,
  // The suggestion is coming from the client.
  NTP_CLIENT_SIDE_SUGGESTION: 1,
  // Indicates a tile was rendered, no matter if it's a thumbnail, a gray tile
  // or an external tile.
  NTP_TILE: 2,
  // The tile uses a local thumbnail image.
  NTP_THUMBNAIL_TILE: 3,
  // Used when no thumbnail is specified and a gray tile with the domain is used
  // as the main tile. Unused here.
  NTP_GRAY_TILE: 4,
  // The visuals of that tile are handled externally by the page itself.
  // Unused here.
  NTP_EXTERNAL_TILE: 5,
  // There was an error in loading both the thumbnail image and the fallback
  // (if it was provided), resulting in a gray tile.
  NTP_THUMBNAIL_ERROR: 6,
  // Used a gray tile with the domain as the fallback for a failed thumbnail.
  // Unused here.
  NTP_GRAY_TILE_FALLBACK: 7,
  // The visuals of that tile's fallback are handled externally. Unused here.
  NTP_EXTERNAL_TILE_FALLBACK: 8,
  // The user moused over an NTP tile.
  NTP_MOUSEOVER: 9
};


/**
 * Total number of tiles to show at any time. If the host page doesn't send
 * enough tiles, we fill them blank.
 * @const {number}
 */
var NUMBER_OF_TILES = 8;


/**
 * The origin of this request.
 * @const {string}
 */
var DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * Counter for DOM elements that we are waiting to finish loading.
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
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
var logEvent = function(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
};


/**
 * Down counts the DOM elements that we are waiting for the page to load.
 * When we get to 0, we send a message to the parent window.
 * This is usually used as an EventListener of onload/onerror.
 */
var countLoad = function() {
  loadedCounter -= 1;
  if (loadedCounter <= 0) {
    window.parent.postMessage({cmd: 'loaded'}, DOMAIN_ORIGIN);
    loadedCounter = 1;
  }
};


/**
 * Handles postMessages coming from the host page to the iframe.
 * We try to keep the logic here to a minimum and just dispatch to the relevant
 * functions.
 **/
var handlePostMessage = function(event) {
  var cmd = event.data.cmd;

  if (cmd == 'tile') {
    addTile(event.data);
  } else if (cmd == 'show') {
    showTiles();
    countLoad();
  } else {
    console.error('Unknown command: ' + event.data);
  }
};


/**
 * Called when the host page has finished sending us tile information and
 * we are ready to show the new tiles and drop the old ones.
 */
var showTiles = function() {
  // Store the tiles on the current closure.
  var cur = tiles;

  // Create empty tiles until we have NUMBER_OF_TILES.
  while (cur.childNodes.length < NUMBER_OF_TILES) {
    addTile({});
  }

  var parent = document.querySelector('#most-visited');

  // Mark old tile DIV for removal after the transition animation is done.
  var old = parent.querySelector('#mv-tiles');
  if (old) {
    old.id = 'mv-tiles-old';
    cur.addEventListener('webkitTransitionEnd', function(ev) {
      if (ev.target === cur) {
        parent.removeChild(old);
      }
    });
  }

  // Add new tileset.
  cur.id = 'mv-tiles';
  parent.appendChild(cur);
  // We want the CSS transition to trigger, so need to add to the DOM before
  // setting the style.
  setTimeout(function() {
    cur.style.opacity = 1.0;
  }, 0);

  // Make sure the tiles variable contain the next tileset we may use.
  tiles = document.createElement('div');
};


/**
 * Called when the host page wants to add a suggestion tile.
 * For Most Visited, it grabs the data from Chrome and pass on.
 * For host page generated it just passes the data.
 * @param {object} args Data for the tile to be rendered.
 */
var addTile = function(args) {
  if (args.rid) {
    var data = chrome.embeddedSearch.searchBox.getMostVisitedItemData(args.rid);
    tiles.appendChild(renderTile(data));
    logEvent(LOG_TYPE.NTP_CLIENT_SIDE_SUGGESTION);
  } else {
    tiles.appendChild(renderTile(null));
  }
};


/**
 * Called when the user decided to add a tile to the blacklist.
 * It sets of the animation for the blacklist and sends the blacklisted id
 * to the host page.
 * @param {Element} tile DOM node of the tile we want to remove.
 */
var blacklistTile = function(tile) {
  tile.classList.add('blacklisted');
  var sent = false;
  tile.addEventListener('webkitTransitionEnd', function() {
    if (sent) return;
    sent = true;
    window.parent.postMessage({cmd: 'tileBlacklisted',
                               rid: Number(tile.getAttribute('data-rid'))},
                              DOMAIN_ORIGIN);
  });
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

  logEvent(LOG_TYPE.NTP_TILE);

  tile.className = 'mv-tile';
  tile.setAttribute('data-rid', data.rid);
  tile.innerHTML = '<div class="mv-favicon"></div>' +
    '<div class="mv-title"></div><div class="mv-thumb"></div>' +
    '<div title="' + configData['removeThumbnailTooltip'] +
    '" class="mv-x"></div>';

  tile.href = data.url;
  tile.title = data.title;
  tile.addEventListener('keypress', function(ev) {
    if (ev.keyCode == 127) { // DELETE
      blacklistTile(tile);
      ev.stopPropagation();
      return false;
    }
  });
  // TODO(fserb): remove this or at least change to mouseenter.
  tile.addEventListener('mouseover', function() {
    logEvent(LOG_TYPE.NTP_MOUSEOVER);
  });

  var title = tile.querySelector('.mv-title');
  title.innerText = data.title;
  title.style.direction = data.direction || 'ltr';

  var thumb = tile.querySelector('.mv-thumb');
  if (data.thumbnailUrl) {
    var img = document.createElement('img');
    img.title = data.title;
    img.src = data.thumbnailUrl;
    loadedCounter += 1;
    img.addEventListener('load', countLoad);
    img.addEventListener('error', countLoad);
    img.addEventListener('error', function(ev) {
      thumb.classList.add('failed-img');
      thumb.removeChild(img);
      logEvent(LOG_TYPE.NTP_THUMBNAIL_ERROR);
    });
    thumb.appendChild(img);
    logEvent(LOG_TYPE.NTP_THUMBNAIL_TILE);
  } else {
    thumb.classList.add('failed-img');
  }

  var favicon = tile.querySelector('.mv-favicon');
  if (data.faviconUrl) {
    var fi = document.createElement('img');
    fi.src = '../' + data.faviconUrl;
    // Set the title to empty so screen readers won't say the image name.
    fi.title = '';
    loadedCounter += 1;
    fi.addEventListener('load', countLoad);
    fi.addEventListener('error', countLoad);
    fi.addEventListener('error', function(ev) {
      favicon.classList.add('failed-favicon');
    });
    favicon.appendChild(fi);
  } else {
    favicon.classList.add('failed-favicon');
  }

  var mvx = tile.querySelector('.mv-x');
  mvx.addEventListener('click', function(ev) {
    blacklistTile(tile);
    ev.stopPropagation();
    return false;
  });

  return tile;
};


/**
 * Do some initialization and parses the query arguments passed to the iframe.
 */
var init = function() {
  // Creates a new DOM element to hold the tiles.
  tiles = document.createElement('div');

  // Parse query arguments.
  var query = window.location.search.substring(1).split('&');
  var args = {};
  for (var i = 0; i < query.length; ++i) {
    var val = query[i].split('=');
    if (val[0] == '') continue;
    args[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  // Enable RTL.
  if (args['rtl'] == '1') {
    var html = document.querySelector('html');
    html.dir = 'rtl';
  }

  window.addEventListener('message', handlePostMessage);
};


window.addEventListener('DOMContentLoaded', init);
})();
