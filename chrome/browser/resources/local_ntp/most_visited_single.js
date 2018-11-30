/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Single iframe for NTP tiles.
(function() {
'use strict';


/**
 * Enum for key codes.
 * @enum {int}
 * @const
 */
const KEYCODES = {
  BACKSPACE: 8,
  DELETE: 46,
  DOWN: 40,
  ENTER: 13,
  ESC: 27,
  LEFT: 37,
  RIGHT: 39,
  SPACE: 32,
  TAB: 9,
  UP: 38,
};

/**
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  MOST_VISITED: 'most-visited',  // Container for all tilesets.
  MV_TILES: 'mv-tiles',          // Most Visited tiles container.
};


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  FAILED_FAVICON: 'failed-favicon',  // Applied when the favicon fails to load.
  REORDER: 'reorder',  // Applied to the tile being moved while reordering.
  REORDERING: 'reordering',  // Applied while we are reordering.
  // Material Design classes.
  MD_EMPTY_TILE: 'md-empty-tile',
  MD_FALLBACK_BACKGROUND: 'md-fallback-background',
  MD_FALLBACK_LETTER: 'md-fallback-letter',
  MD_FAVICON: 'md-favicon',
  MD_ICON: 'md-icon',
  MD_ADD_ICON: 'md-add-icon',
  MD_ADD_BACKGROUND: 'md-add-background',
  MD_MENU: 'md-menu',
  MD_EDIT_MENU: 'md-edit-menu',
  MD_TILE: 'md-tile',
  MD_TILE_CONTAINER: 'md-tile-container',
  MD_TILE_INNER: 'md-tile-inner',
  MD_TITLE: 'md-title',
  MD_TITLE_CONTAINER: 'md-title-container',
};


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

  // Shortcuts have been customized.
  NTP_SHORTCUT_CUSTOMIZED: 39,
  // The 'Add shortcut' link was clicked.
  NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED: 44,
  // The 'Edit shortcut' link was clicked.
  NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED: 45,
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
 * Timeout delay for the window.onresize event throttle. Set to 15 frame per
 * second.
 * @const {number}
 */
const RESIZE_TIMEOUT_DELAY = 66;


/**
 * Timeout delay in ms before starting the reorder flow.
 * @const {number}
 */
const REORDER_TIMEOUT_DELAY = 1000;


/**
 * Maximum number of tiles if custom links is enabled.
 * @const {number}
 */
const MD_MAX_NUM_CUSTOM_LINK_TILES = 10;


/**
 * Maximum number of tiles per row for Material Design.
 * @const {number}
 */
const MD_MAX_TILES_PER_ROW = 5;


/**
 * Width of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_WIDTH = 112;


/**
 * Number of tiles that will always be visible for Material Design. Calculated
 * by dividing minimum |--content-width| (see local_ntp.css) by |MD_TILE_WIDTH|
 * and multiplying by 2 rows.
 * @const {number}
 */
const MD_NUM_TILES_ALWAYS_VISIBLE = 6;


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
 * Maximum number of MostVisited tiles to show at any time. If the host page
 * doesn't send enough tiles and custom links is not enabled, we fill them blank
 * tiles. This can be changed depending on what feature is enabled. Set by the
 * host page, while 8 is default.
 * @type {number}
 */
let maxNumTiles = 8;


/**
 * List of parameters passed by query args.
 * @type {Object}
 */
var queryArgs = {};


/**
 * True if we are currently reordering the tiles.
 * @type {boolean}
 */
let reordering = false;


/**
 * The tile that is being moved during the reorder flow. Null if we are
 * currently not reordering.
 * @type {?Element}
 */
let elementToReorder = null;


/**
 * True if custom links is enabled.
 * @type {boolean}
 */
let isCustomLinksEnabled = false;


/**
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
var logEvent = function(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
};

/**
 * Log impression of an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < |maxNumTiles|.
 * @param {number} tileTitleSource The source of the tile's title as received
 *                 from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *                 getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
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
 * @param {number} tileIndex Position of the tile, >= 0 and < |maxNumTiles|.
 * @param {number} tileTitleSource The source of the tile's title as received
 *                 from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *                 getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
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
    let tilesAreCustomLinks =
        isCustomLinksEnabled && chrome.embeddedSearch.newTabPage.isCustomLinks;
    // Note that it's easiest to capture this when all custom links are loaded,
    // rather than when the impression for each link is logged.
    if (tilesAreCustomLinks) {
      chrome.embeddedSearch.newTabPage.logEvent(
          LOG_TYPE.NTP_SHORTCUT_CUSTOMIZED);
    }
    // Tell the parent page whether to show the restore default shortcuts option
    // in the menu.
    window.parent.postMessage(
        {cmd: 'loaded', showRestoreDefault: tilesAreCustomLinks},
        DOMAIN_ORIGIN);
    tilesAreCustomLinks = false;
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
  } else if (cmd === 'focusMenu') {
    focusTileMenu(data);
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
  document.body.classList.toggle('using-theme', info.isUsingTheme);

  // Reduce font weight on the default(white) background for Mac and CrOS.
  document.body.classList.toggle('mac-chromeos',
      !info.isThemeDark && !info.isUsingTheme &&
      (navigator.userAgent.indexOf('Mac') > -1 ||
      navigator.userAgent.indexOf('CrOS') > -1));
};


/**
 * Handler for 'focusMenu' message from the host page. Focuses the edited tile's
 * menu or the add shortcut tile after closing the custom link edit dialog
 * without saving.
 * @param {object} info Data received in the message.
 */
var focusTileMenu = function(info) {
  let tile = document.querySelector(`a.md-tile[data-tid="${info.tid}"]`);
  if (info.tid === -1 /* Add shortcut tile */)
    tile.focus();
  else
    tile.parentNode.childNodes[1].focus();
};


/**
 * Removes all old instances of |IDS.MV_TILES| that are pending for deletion.
 */
var removeAllOldTiles = function() {
  var parent = document.querySelector('#' + IDS.MOST_VISITED);
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

  // Add an "add new custom link" button if we haven't reached the maximum
  // number of tiles.
  if (isCustomLinksEnabled && cur.childNodes.length < maxNumTiles) {
    let data = {
      'tid': -1,
      'title': queryArgs['addLink'],
      'url': '',
      'isAddButton': true,
    };
    tiles.appendChild(renderMaterialDesignTile(data));
  }

  var parent = document.querySelector('#' + IDS.MOST_VISITED);

  // Only fade in the new tiles if there were tiles before.
  var fadeIn = false;
  var old = parent.querySelector('#' + IDS.MV_TILES);
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
  cur.id = IDS.MV_TILES;
  parent.appendChild(cur);

  // Re-balance the tiles if there are more than |MD_MAX_TILES_PER_ROW| in order
  // to make even rows.
  if (cur.childNodes.length > MD_MAX_TILES_PER_ROW) {
    cur.style.maxWidth = 'calc(var(--md-tile-width) * ' +
        Math.ceil(cur.childNodes.length / 2) + ')';
  }

  // Prevent keyboard navigation to tiles that are not visible.
  updateTileVisibility();

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
 * Explicitly hide tiles that are not visible in order to prevent keyboard
 * navigation.
 */
function updateTileVisibility() {
  const allTiles = document.querySelectorAll(
      '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE_CONTAINER);
  if (allTiles.length === 0)
    return;

  // Get the current number of tiles per row. Hide any tile after the first two
  // rows.
  const tilesPerRow = Math.trunc(document.body.offsetWidth / MD_TILE_WIDTH);
  for (let i = MD_NUM_TILES_ALWAYS_VISIBLE; i < allTiles.length; i++)
    allTiles[i].style.display = (i < tilesPerRow * 2) ? 'block' : 'none';
}


/**
 * Handler for the 'show' message from the host page, called when it wants to
 * add a suggestion tile.
 * It's also used to fill up our tiles to |maxNumTiles| if necessary.
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
  let tid = Number(tile.firstChild.getAttribute('data-tid'));

  if (isCustomLinksEnabled) {
    chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(tid);
  } else {
    tile.classList.add('blacklisted');
    tile.addEventListener('transitionend', function(ev) {
      if (ev.propertyName != 'width')
        return;
      window.parent.postMessage(
          {cmd: 'tileBlacklisted', tid: Number(tid)}, DOMAIN_ORIGIN);
    });
  }
};


/**
 * Starts edit custom link flow. Tells host page to show the edit custom link
 * dialog and pre-populate it with data obtained using the link's id.
 * @param {?number} tid Restricted id of the tile we want to edit.
 */
function editCustomLink(tid) {
  window.parent.postMessage({cmd: 'startEditLink', tid: tid}, DOMAIN_ORIGIN);
}


/**
 * Starts the reorder flow. Updates the visual style of the held tile to
 * indicate that it is being moved.
 * @param {!Element} tile Tile that is being moved.
 */
function startReorder(tile) {
  reordering = true;
  elementToReorder = tile;

  tile.classList.add(CLASSES.REORDER);
  // Disable other hover/active styling for all tiles.
  document.body.classList.add(CLASSES.REORDERING);

  document.addEventListener('dragend', () => {
    stopReorder(tile);
  }, {once: true});
}


/**
 * Stops the reorder flow. Resets the held tile's visual style and tells the
 * EmbeddedSearchAPI that a tile has been moved.
 * @param {!Element} tile Tile that has been moved.
 */
function stopReorder(tile) {
  reordering = false;
  elementToReorder = null;

  tile.classList.remove(CLASSES.REORDER);
  document.body.classList.remove(CLASSES.REORDERING);

  // Update |data-pos| for all tiles and notify EmbeddedSearchAPI that the tile
  // has been moved.
  const allTiles = document.querySelectorAll('#mv-tiles .' + CLASSES.MD_TILE);
  for (let i = 0; i < allTiles.length; i++)
    allTiles[i].setAttribute('data-pos', i);
  chrome.embeddedSearch.newTabPage.reorderCustomLink(
      Number(tile.firstChild.getAttribute('data-tid')),
      Number(tile.firstChild.getAttribute('data-pos')));
}


/**
 * Sets up event listeners necessary for tile reordering.
 * @param {!Element} tile Tile on which to set the event listeners.
 */
function setupReorder(tile) {
  // Starts the reorder flow after the user has held the mouse button down for
  // |REORDER_TIMEOUT_DELAY|.
  tile.addEventListener('mousedown', (event) => {
    // Do not reorder if the edit menu was clicked or if ctrl/shift/alt/meta is
    // also held down.
    if (event.button == 0 /* LEFT CLICK */ && !event.ctrlKey &&
        !event.shiftKey && !event.altKey && !event.metaKey &&
        !event.target.classList.contains(CLASSES.MD_MENU)) {
      let timeout = -1;

      // Cancel the timeout if the user drags the mouse off the tile and
      // releases or if the mouse if released.
      let dragend = document.addEventListener('dragend', () => {
        window.clearTimeout(timeout);
      }, {once: true});
      let mouseup = document.addEventListener('mouseup', () => {
        if (event.button == 0 /* LEFT CLICK */)
          window.clearTimeout(timeout);
      }, {once: true});

      // Wait for |REORDER_TIMEOUT_DELAY| before starting the reorder flow.
      timeout = window.setTimeout(() => {
        if (!reordering)
          startReorder(tile);
        document.removeEventListener('dragend', dragend);
        document.removeEventListener('mouseup', mouseup);
      }, REORDER_TIMEOUT_DELAY);
    }
  });

  tile.addEventListener('dragover', (event) => {
    // Only executed when the reorder flow is ongoing. Inserts the tile that is
    // being moved before/after this |tile| according to order in the list.
    if (reordering && elementToReorder && elementToReorder != tile) {
      // Determine which side to insert the element on:
      // - If the held tile comes after the current tile, insert behind the
      //   current tile.
      // - If the held tile comes before the current tile, insert in front of
      //   the current tile.
      let insertBefore;  // Element to insert the held tile behind.
      if (tile.compareDocumentPosition(elementToReorder) &
          Node.DOCUMENT_POSITION_FOLLOWING) {
        insertBefore = tile;
      } else {
        insertBefore = tile.nextSibling;
      }
      $('mv-tiles').insertBefore(elementToReorder, insertBefore);
    }
  });
}


/**
 * Renders a MostVisited tile to the DOM.
 * @param {object} data Object containing rid, url, title, favicon, thumbnail,
 *     and optionally isAddButton. isAddButton is true if you want to construct
 *     an add custom link button. data is null if you want to construct an
 *     empty tile. isAddButton can only be set if custom links is enabled.
 */
var renderTile = function(data) {
  return renderMaterialDesignTile(data);
};


/**
 * Renders a MostVisited tile with Material Design styles.
 * @param {object} data Object containing rid, url, title, favicon, and
 *     optionally isAddButton. isAddButton is if you want to construct an add
 *     custom link button. data is null if you want to construct an empty tile.
 * @return {Element}
 */
function renderMaterialDesignTile(data) {
  let mdTileContainer = document.createElement('div');
  mdTileContainer.role = 'none';

  if (data == null) {
    mdTileContainer.className = CLASSES.MD_EMPTY_TILE;
    return mdTileContainer;
  }
  mdTileContainer.className = CLASSES.MD_TILE_CONTAINER;

  // The tile will be appended to tiles.
  const position = tiles.children.length;
  // This is set in the load/error event for the favicon image.
  let tileType = TileVisualType.NONE;

  let mdTile = document.createElement('a');
  mdTile.className = CLASSES.MD_TILE;
  mdTile.tabIndex = 0;
  mdTile.setAttribute('data-tid', data.tid);
  mdTile.setAttribute('data-pos', position);
  if (utils.isSchemeAllowed(data.url)) {
    mdTile.href = data.url;
  }
  mdTile.setAttribute('aria-label', data.title);
  mdTile.title = data.title;

  mdTile.addEventListener('click', function(ev) {
    if (data.isAddButton) {
      editCustomLink();
      logEvent(LOG_TYPE.NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED);
    } else {
      logMostVisitedNavigation(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
    }
  });
  mdTile.addEventListener('keydown', function(event) {
    if ((event.keyCode === KEYCODES.DELETE ||
         event.keyCode === KEYCODES.BACKSPACE) &&
        !data.isAddButton) {
      event.preventDefault();
      event.stopPropagation();
      blacklistTile(mdTileContainer);
    } else if (
        event.keyCode === KEYCODES.ENTER || event.keyCode === KEYCODES.SPACE) {
      event.preventDefault();
      this.click();
    } else if (event.keyCode === KEYCODES.LEFT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.max(Number(this.getAttribute('data-pos')) - 1, 0)].focus();
    } else if (event.keyCode === KEYCODES.RIGHT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.min(
                Number(this.getAttribute('data-pos')) + 1, tiles.length - 1)]
          .focus();
    }
  });
  utils.disableOutlineOnMouseClick(mdTile);

  let mdTileInner = document.createElement('div');
  mdTileInner.className = CLASSES.MD_TILE_INNER;

  let mdIcon = document.createElement('div');
  mdIcon.className = CLASSES.MD_ICON;

  let mdFavicon = document.createElement('div');
  mdFavicon.className = CLASSES.MD_FAVICON;
  if (data.isAddButton) {
    let mdAdd = document.createElement('div');
    mdAdd.className = CLASSES.MD_ADD_ICON;
    let addBackground = document.createElement('div');
    addBackground.className = CLASSES.MD_ADD_BACKGROUND;

    addBackground.appendChild(mdAdd);
    mdFavicon.appendChild(addBackground);
  } else {
    let fi = document.createElement('img');
    // Set title and alt to empty so screen readers won't say the image name.
    fi.title = '';
    fi.alt = '';
    fi.src = 'chrome-search://ntpicon/size/24@' + window.devicePixelRatio +
        'x/' + data.url;
    loadedCounter += 1;
    fi.addEventListener('load', function(ev) {
      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_REAL;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });
    fi.addEventListener('error', function(ev) {
      let fallbackBackground = document.createElement('div');
      fallbackBackground.className = CLASSES.MD_FALLBACK_BACKGROUND;
      let fallbackLetter = document.createElement('div');
      fallbackLetter.className = CLASSES.MD_FALLBACK_LETTER;
      fallbackLetter.innerText = data.title.charAt(0).toUpperCase();
      if (navigator.userAgent.indexOf('Windows') > -1) {
        fallbackLetter.style.fontWeight = 600;
      }
      mdFavicon.classList.add(CLASSES.FAILED_FAVICON);

      fallbackBackground.appendChild(fallbackLetter);
      mdFavicon.removeChild(fi);
      mdFavicon.appendChild(fallbackBackground);

      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_DEFAULT;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });

    mdFavicon.appendChild(fi);
  }

  mdIcon.appendChild(mdFavicon);
  mdTileInner.appendChild(mdIcon);

  let mdTitleContainer = document.createElement('div');
  mdTitleContainer.className = CLASSES.MD_TITLE_CONTAINER;
  let mdTitle = document.createElement('div');
  mdTitle.className = CLASSES.MD_TITLE;
  let mdTitleTextwrap = document.createElement('span');
  mdTitleTextwrap.innerText = data.title;
  mdTitle.style.direction = data.direction || 'ltr';
  // Windows font family fallback to Segoe
  if (navigator.userAgent.indexOf('Windows') > -1) {
    mdTitle.style.fontFamily = 'Segoe UI';
  }
  mdTitleContainer.appendChild(mdTitle);
  mdTileInner.appendChild(mdTitleContainer);
  mdTile.appendChild(mdTileInner);
  mdTileContainer.appendChild(mdTile);
  mdTitle.appendChild(mdTitleTextwrap);

  if (!data.isAddButton) {
    let mdMenu = document.createElement('button');
    mdMenu.className = CLASSES.MD_MENU;
    if (isCustomLinksEnabled) {
      mdMenu.classList.add(CLASSES.MD_EDIT_MENU);
      mdMenu.title = queryArgs['editLinkTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label',
          (queryArgs['editLinkTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        editCustomLink(data.tid);
        ev.preventDefault();
        ev.stopPropagation();
        logEvent(LOG_TYPE.NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED);
      });
    } else {
      mdMenu.title = queryArgs['removeTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label', (queryArgs['removeTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        removeAllOldTiles();
        blacklistTile(mdTileContainer);
        ev.preventDefault();
        ev.stopPropagation();
      });
    }
    // Don't allow the event to bubble out to the containing tile, as that would
    // trigger navigation to the tile URL.
    mdMenu.addEventListener('keydown', function(ev) {
      event.stopPropagation();
    });
    utils.disableOutlineOnMouseClick(mdMenu);

    mdTileContainer.appendChild(mdMenu);
  }

  // Enable reordering.
  if (isCustomLinksEnabled && !data.isAddButton) {
    mdTileContainer.draggable = 'true';
    setupReorder(mdTileContainer);
  }

  return mdTileContainer;
}


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

  document.title = queryArgs['title'];

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    var html = document.querySelector('html');
    html.dir = 'rtl';
  }

  // Enable custom links.
  if (queryArgs['enableCustomLinks'] == '1') {
    isCustomLinksEnabled = true;
  }

  // Set the maximum number of tiles to show.
  if (isCustomLinksEnabled) {
    maxNumTiles = MD_MAX_NUM_CUSTOM_LINK_TILES;
  }

  // Throttle the resize event.
  let resizeTimeout;
  window.onresize = () => {
    if (resizeTimeout)
      return;
    resizeTimeout = window.setTimeout(() => {
      resizeTimeout = null;
      updateTileVisibility();
    }, RESIZE_TIMEOUT_DELAY);
  };

  window.addEventListener('message', handlePostMessage);
};


window.addEventListener('DOMContentLoaded', init);
})();
