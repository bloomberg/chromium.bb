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
  NTP_MOUSEOVER: 9,
  // A NTP Tile has finished loading (successfully or failing).
  NTP_TILE_LOADED: 10,
};


/**
 * Total number of tiles to show at any time. If the host page doesn't send
 * enough tiles, we fill them blank.
 * @const {number}
 */
var NUMBER_OF_TILES = 8;


/**
 * Whether to use icons instead of thumbnails.
 * @type {boolean}
 */
var USE_ICONS = false;


/**
 * Number of lines to display in titles.
 * @type {number}
 */
var NUM_TITLE_LINES = 1;


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
 * List of parameters passed by query args.
 * @type {Object}
 */
var queryArgs = {};

/**
 * Url to ping when suggestions have been shown.
 */
var impressionUrl = null;


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
    showTiles();
    logEvent(LOG_TYPE.NTP_TILE_LOADED);
    window.parent.postMessage({cmd: 'loaded'}, DOMAIN_ORIGIN);
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
    countLoad();
    hideOverflowTiles(data);
  } else if (cmd == 'updateTheme') {
    updateTheme(data);
  } else if (cmd == 'tilesVisible') {
    hideOverflowTiles(data);
  } else {
    console.error('Unknown command: ' + JSON.stringify(data));
  }
};


var updateTheme = function(info) {
  var themeStyle = [];

  if (info.tileBorderColor) {
    themeStyle.push('.thumb-ntp .mv-tile {' +
        'border: 1px solid ' + info.tileBorderColor + '; }');
  }
  if (info.tileHoverBorderColor) {
    themeStyle.push('.thumb-ntp .mv-tile:hover {' +
        'border-color: ' + info.tileHoverBorderColor + '; }');
  }
  if (info.isThemeDark) {
    themeStyle.push('.thumb-ntp .mv-tile, .thumb-ntp .mv-empty-tile { ' +
        'background: rgb(51,51,51); }');
    themeStyle.push('.thumb-ntp .mv-thumb.failed-img { ' +
        'background-color: #555; }');
    themeStyle.push('.thumb-ntp .mv-thumb.failed-img::after { ' +
        'border-color: #333; }');
    themeStyle.push('.thumb-ntp .mv-x { ' +
        'background: linear-gradient(to left, ' +
        'rgb(51,51,51) 60%, transparent); }');
    themeStyle.push('html[dir=rtl] .thumb-ntp .mv-x { ' +
        'background: linear-gradient(to right, ' +
        'rgb(51,51,51) 60%, transparent); }');
    themeStyle.push('.thumb-ntp .mv-x::after { ' +
        'background-color: rgba(255,255,255,0.7); }');
    themeStyle.push('.thumb-ntp .mv-x:hover::after { ' +
        'background-color: #fff; }');
    themeStyle.push('.thumb-ntp .mv-x:active::after { ' +
        'background-color: rgba(255,255,255,0.5); }');
    themeStyle.push('.icon-ntp .mv-tile:focus { ' +
        'background: rgba(255,255,255,0.2); }');
  }
  if (info.tileTitleColor) {
    themeStyle.push('body { color: ' + info.tileTitleColor + '; }');
  }

  document.querySelector('#custom-theme').textContent = themeStyle.join('\n');
};


/**
 * Hides extra tiles that don't fit on screen.
 */
var hideOverflowTiles = function(data) {
  var tileAndEmptyTileList = document.querySelectorAll(
      '#mv-tiles .mv-tile,#mv-tiles .mv-empty-tile');
  for (var i = 0; i < tileAndEmptyTileList.length; ++i) {
    tileAndEmptyTileList[i].classList.toggle('hidden', i >= data.maxVisible);
  }
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
    old.removeAttribute('id');
    old.classList.add('mv-tiles-old');
    old.style.opacity = 0.0;
    cur.addEventListener('webkitTransitionEnd', function(ev) {
      if (ev.target === cur) {
        removeAllOldTiles();
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

  if (impressionUrl) {
    if (navigator.sendBeacon) {
      navigator.sendBeacon(impressionUrl);
    } else {
      // if sendBeacon is not enabled, we fallback to "a ping".
      var a = document.createElement('a');
      a.href = '#';
      a.ping = impressionUrl;
      a.click();
    }
    impressionUrl = null;
  }
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
    data.tid = data.rid;
    if (!data.faviconUrl) {
      data.faviconUrl = 'chrome-search://favicon/size/16@' +
          window.devicePixelRatio + 'x/' + data.renderViewId + '/' + data.tid;
    }
    tiles.appendChild(renderTile(data));
  } else if (args.id) {
    tiles.appendChild(renderTile(args));
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
  tile.addEventListener('webkitTransitionEnd', function(ev) {
    if (ev.propertyName != 'width') return;

    window.parent.postMessage({cmd: 'tileBlacklisted',
                               tid: Number(tile.getAttribute('data-tid'))},
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
  tile.setAttribute('data-tid', data.tid);
  var tooltip = queryArgs['removeTooltip'] || '';
  var html = [];
  if (!USE_ICONS) {
    html.push('<div class="mv-favicon"></div>');
  }
  html.push('<div class="mv-title"></div><div class="mv-thumb"></div>');
  html.push('<div title="' + tooltip + '" class="mv-x"></div>');
  tile.innerHTML = html.join('');

  tile.href = data.url;
  tile.title = data.title;
  if (data.impressionUrl) {
    impressionUrl = data.impressionUrl;
  }
  if (data.pingUrl) {
    tile.addEventListener('click', function(ev) {
      if (navigator.sendBeacon) {
        navigator.sendBeacon(data.pingUrl);
      } else {
        // if sendBeacon is not enabled, we fallback to "a ping".
        var a = document.createElement('a');
        a.href = '#';
        a.ping = data.pingUrl;
        a.click();
      }
    });
  }
  // For local suggestions, we use navigateContentWindow instead of the default
  // action, since it includes support for file:// urls.
  if (data.rid) {
    tile.addEventListener('click', function(ev) {
      ev.preventDefault();
      var disp = chrome.embeddedSearch.newTabPage.getDispositionFromClick(
        ev.button == 1,  // MIDDLE BUTTON
        ev.altKey, ev.ctrlKey, ev.metaKey, ev.shiftKey);

      window.chrome.embeddedSearch.newTabPage.navigateContentWindow(this.href,
                                                                    disp);
    });
  }

  tile.addEventListener('keydown', function(event) {
    if (event.keyCode == 46 /* DELETE */ ||
        event.keyCode == 8 /* BACKSPACE */) {
      event.preventDefault();
      event.stopPropagation();
      blacklistTile(this);
    } else if (event.keyCode == 13 /* ENTER */ ||
               event.keyCode == 32 /* SPACE */) {
      event.preventDefault();
      this.click();
    } else if (event.keyCode >= 37 && event.keyCode <= 40 /* ARROWS */) {
      var tiles = document.querySelectorAll('#mv-tiles .mv-tile');
      var nextTile = null;
      // Use the location of the tile to find the next one in the
      // appropriate direction.
      // For LEFT and UP we keep iterating until we find the last element
      // that fulfills the conditions.
      // For RIGHT and DOWN we accept the first element that works.
      if (event.keyCode == 37 /* LEFT */) {
        for (var i = 0; i < tiles.length; i++) {
          var tile = tiles[i];
          if (tile.offsetTop == this.offsetTop &&
              tile.offsetLeft < this.offsetLeft) {
            if (!nextTile || tile.offsetLeft > nextTile.offsetLeft) {
              nextTile = tile;
            }
          }
        }
      }
      if (event.keyCode == 38 /* UP */) {
        for (var i = 0; i < tiles.length; i++) {
          var tile = tiles[i];
          if (tile.offsetTop < this.offsetTop &&
              tile.offsetLeft == this.offsetLeft) {
            if (!nextTile || tile.offsetTop > nextTile.offsetTop) {
              nextTile = tile;
            }
          }
        }
      }
      if (event.keyCode == 39 /* RIGHT */) {
        for (var i = 0; i < tiles.length; i++) {
          var tile = tiles[i];
          if (tile.offsetTop == this.offsetTop &&
              tile.offsetLeft > this.offsetLeft) {
            if (!nextTile || tile.offsetLeft < nextTile.offsetLeft) {
              nextTile = tile;
            }
          }
        }
      }
      if (event.keyCode == 40 /* DOWN */) {
        for (var i = 0; i < tiles.length; i++) {
          var tile = tiles[i];
          if (tile.offsetTop > this.offsetTop &&
              tile.offsetLeft == this.offsetLeft) {
            if (!nextTile || tile.offsetTop < nextTile.offsetTop) {
              nextTile = tile;
            }
          }
        }
      }

      if (nextTile) {
        nextTile.focus();
      }
    }
  });
  // TODO(fserb): remove this or at least change to mouseenter.
  tile.addEventListener('mouseover', function() {
    logEvent(LOG_TYPE.NTP_MOUSEOVER);
  });

  var title = tile.querySelector('.mv-title');
  title.innerText = data.title;
  title.style.direction = data.direction || 'ltr';
  if (NUM_TITLE_LINES > 1) {
    title.classList.add('multiline');
  }

  if (USE_ICONS) {
    var thumb = tile.querySelector('.mv-thumb');
    if (data.largeIconUrl) {
      var img = document.createElement('img');
      img.title = data.title;
      img.src = data.largeIconUrl;
      img.classList.add('large-icon');
      loadedCounter += 1;
      img.addEventListener('load', countLoad);
      img.addEventListener('load', function(ev) {
        thumb.classList.add('large-icon-outer');
      });
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
  } else { // THUMBNAILS
    // We keep track of the outcome of loading possible thumbnails for this
    // tile. Possible values:
    //   - null: waiting for load/error
    //   - false: error
    //   - a string: URL that loaded correctly.
    // This is populated by acceptImage/rejectImage and loadBestImage
    // decides the best one to load.
    var results = [];
    var thumb = tile.querySelector('.mv-thumb');
    var img = document.createElement('img');
    var loaded = false;

    var loadBestImage = function() {
      if (loaded) {
        return;
      }
      for (var i = 0; i < results.length; ++i) {
        if (results[i] === null) {
          return;
        }
        if (results[i] != false) {
          img.src = results[i];
          loaded = true;
          return;
        }
      }
      thumb.classList.add('failed-img');
      thumb.removeChild(img);
      logEvent(LOG_TYPE.NTP_THUMBNAIL_ERROR);
      countLoad();
    };

    var acceptImage = function(idx, url) {
      return function(ev) {
        results[idx] = url;
        loadBestImage();
      };
    };

    var rejectImage = function(idx) {
      return function(ev) {
        results[idx] = false;
        loadBestImage();
      };
    };

    img.title = data.title;
    img.classList.add('thumbnail');
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

    if (data.thumbnailUrl) {
      img.src = data.thumbnailUrl;
    } else {
      // Get all thumbnailUrls for the tile.
      // They are ordered from best one to be used to worst.
      for (var i = 0; i < data.thumbnailUrls.length; ++i) {
        results.push(null);
      }
      for (var i = 0; i < data.thumbnailUrls.length; ++i) {
        if (data.thumbnailUrls[i]) {
          var image = new Image();
          image.src = data.thumbnailUrls[i];
          image.onload = acceptImage(i, data.thumbnailUrls[i]);
          image.onerror = rejectImage(i);
        } else {
          rejectImage(i)(null);
        }
      }
    }

    var favicon = tile.querySelector('.mv-favicon');
    if (data.faviconUrl) {
      var fi = document.createElement('img');
      fi.src = data.faviconUrl;
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
  }

  var mvx = tile.querySelector('.mv-x');
  mvx.addEventListener('click', function(ev) {
    removeAllOldTiles();
    blacklistTile(tile);
    ev.preventDefault();
    ev.stopPropagation();
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
  queryArgs = {};
  for (var i = 0; i < query.length; ++i) {
    var val = query[i].split('=');
    if (val[0] == '') continue;
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  // Apply class for icon NTP, if specified.
  USE_ICONS = queryArgs['icons'] == '1';
  if ('ntl' in queryArgs) {
    var ntl = parseInt(queryArgs['ntl'], 10);
    if (isFinite(ntl))
      NUM_TITLE_LINES = ntl;
  }

  // Duplicating NTP_DESIGN.mainClass.
  document.querySelector('#most-visited').classList.add(
      USE_ICONS ? 'icon-ntp' : 'thumb-ntp');

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    var html = document.querySelector('html');
    html.dir = 'rtl';
  }

  window.addEventListener('message', handlePostMessage);
};


window.addEventListener('DOMContentLoaded', init);
})();
