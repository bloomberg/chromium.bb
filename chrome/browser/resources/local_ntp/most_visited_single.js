/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Single iframe for NTP tiles.

/**
 * Controls rendering the Most Visited iframe.
 * @return {Object} A limited interface for testing the iframe.
 */
function MostVisited() {
'use strict';


/**
 * Enum for key codes.
 * @enum {number}
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
  GRID_TILE: 'grid-tile',
  GRID_TILE_CONTAINER: 'grid-tile-container',
  REORDER: 'reorder',  // Applied to the tile being moved while reordering.
  REORDERING: 'reordering',      // Applied while we are reordering.
  MAC_CHROMEOS: 'mac-chromeos',  // Reduces font weight for MacOS and ChromeOS.
  // Material Design classes.
  MD_EMPTY_TILE: 'md-empty-tile',
  MD_ICON_BACKGROUND: 'md-icon-background',
  MD_FALLBACK_LETTER: 'md-fallback-letter',
  MD_FAVICON: 'md-favicon',
  MD_ICON: 'md-icon',
  MD_ADD_ICON: 'md-add-icon',
  MD_MENU: 'md-menu',
  MD_EDIT_MENU: 'md-edit-menu',
  MD_TILE: 'md-tile',
  MD_TILE_CONTAINER: 'md-tile-container',
  MD_TILE_INNER: 'md-tile-inner',
  MD_TITLE: 'md-title',
  MD_TITLE_CONTAINER: 'md-title-container',
  NO_INITIAL_FADE: 'no-initial-fade',
};


/**
 * The different types of events that are logged from the NTP.  This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
const LOG_TYPE = {
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
const TileVisualType = {
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
 * Height of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_HEIGHT = 128;


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
const DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * Counter for DOM elements that we are waiting to finish loading. Starts out
 * at 1 because initially we're waiting for the "show" message from the parent.
 * @type {number}
 */
let loadedCounter = 1;


/**
 * DOM element containing the tiles we are going to present next.
 * Works as a double-buffer that is shown when we receive a "show" postMessage.
 * @type {Element}
 */
let tiles = null;


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
let queryArgs = {};


/**
 * True if custom links is enabled.
 * @type {boolean}
 */
let isCustomLinksEnabled = false;


/**
 * The current grid of tiles.
 * @type {?Grid}
 */
let currGrid = null;


/**
 * Class that handles layouts and animations for the tile grid. This includes
 * animations for adding, deleting, and reordering.
 */
class Grid {
  constructor() {
    /** @private {number} */
    this.tileHeight_ = 0;
    /** @private {number} */
    this.tileWidth_ = 0;
    /** @private {number} */
    this.tilesAlwaysVisible_ = 0;
    /** @private {number} */
    this.maxTilesPerRow_ = 0;
    /** @private {number} */
    this.maxTiles_ = 0;

    /** @private {number} */
    this.maxTilesPerRowWindow_ = 0;

    /** @private {?Element} */
    this.container_ = null;
    /** @private {?HTMLCollection} */
    this.tiles_ = null;

    /**
     * Matrix that stores the (x,y) positions of the tile layout.
     * @private {?Array<!Array<number>>}
     */
    this.position_ = null;
  }


  /**
   * Sets up the grid for the new tileset in |container|. The old tileset is
   * discarded.
   * @param {!Element} container The grid container element.
   * @param {Object=} params Customizable parameters for the grid. Used in
   *     testing.
   */
  init(container, params = {}) {
    this.container_ = container;

    this.tileHeight_ = params.tileHeight || MD_TILE_HEIGHT;
    this.tileWidth_ = params.tileWidth || MD_TILE_WIDTH;
    this.tilesAlwaysVisible_ =
        params.tilesAlwaysVisible || MD_NUM_TILES_ALWAYS_VISIBLE;
    this.maxTilesPerRow_ = params.maxTilesPerRow || MD_MAX_TILES_PER_ROW;
    this.maxTiles_ = params.maxTiles || maxNumTiles;

    this.maxTilesPerRowWindow_ = this.getMaxTilesPerRow_();

    this.tiles_ =
        this.container_.getElementsByClassName(CLASSES.GRID_TILE_CONTAINER);
    if (this.tiles_.length > this.maxTiles_) {
      throw new Error(
          'The number of tiles (' + this.tiles_.length +
          ') exceeds the maximum (' + this.maxTiles_ + ').');
    }
    this.position_ = new Array(this.maxTiles_);
    for (let i = 0; i < this.maxTiles_; i++) {
      this.position_[i] = Array(2).fill(0);
    }

    this.updateLayout();
  }


  /**
   * Returns a grid tile wrapper that contains |tile|.
   * @param {!Element} tile The tile element.
   * @return {!Element} A grid tile wrapper.
   */
  createGridTile(tile) {
    const gridTileContainer = document.createElement('div');
    gridTileContainer.className = CLASSES.GRID_TILE_CONTAINER;
    const gridTile = document.createElement('div');
    gridTile.className = CLASSES.GRID_TILE;
    gridTile.appendChild(tile);
    gridTileContainer.appendChild(gridTile);
    return gridTileContainer;
  }


  /**
   * Updates the layout of the tiles. This is called for new tilesets and when
   * the window is resized or zoomed. Translates each tile's
   * |CLASSES.GRID_TILE_CONTAINER| to the correct position.
   */
  updateLayout() {
    const tilesPerRow = this.getTilesPerRow_();

    this.container_.style.width = (tilesPerRow * this.tileWidth_) + 'px';

    const maxVisibleTiles = tilesPerRow * 2;
    let x = 0;
    let y = 0;
    for (let i = 0; i < this.tiles_.length; i++) {
      const tile = this.tiles_[i];
      // Reset the offset for row 2.
      if (i === tilesPerRow) {
        x = this.getRow2Offset_(tilesPerRow);
        y = this.tileHeight_;
      }
      // Update the tile's position.
      this.translate_(tile, x, y);
      this.position_[i][0] = x;
      this.position_[i][1] = y;
      x += this.tileWidth_;  // Increment for the next tile.

      // Update visibility for tiles that may be hidden by the iframe border in
      // order to prevent keyboard navigation from reaching them. Ignores tiles
      // that will always be visible, since changing 'display' prevents
      // transitions from working.
      if (i >= this.tilesAlwaysVisible_) {
        const isVisible = i < maxVisibleTiles;
        tile.style.display = isVisible ? 'block' : 'none';
      }
    }
  }


  /**
   * Called when the window is resized/zoomed. Recalculates maximums for the new
   * window size and calls |updateLayout| if necessary.
   */
  onResize() {
    // Update the layout if the max number of tiles per row changes due to the
    // new window size.
    const maxPerRowWindow = this.getMaxTilesPerRow_();
    if (maxPerRowWindow !== this.maxTilesPerRowWindow_) {
      this.maxTilesPerRowWindow_ = maxPerRowWindow;
      this.updateLayout();
    }
  }


  /**
   * Returns the number of tiles per row. This may be balanced if there are more
   * than |this.maxTilesPerRow_| in order to make even rows.
   * @return {number} The number of tiles per row.
   * @private
   */
  getTilesPerRow_() {
    const tilesPerRow = (this.tiles_.length > this.maxTilesPerRow_) ?
        Math.ceil(this.tiles_.length / 2) :
        this.tiles_.length;
    // The number of tiles cannot exceed the max allowed by the window size.
    return Math.min(tilesPerRow, this.maxTilesPerRowWindow_);
  }


  /**
   * Returns the maximum number of tiles per row allowed by the window size.
   * @return {number} The maximum number of tiles per row.
   * @private
   */
  getMaxTilesPerRow_() {
    return Math.floor(window.innerWidth / this.tileWidth_);
  }


  /**
   * Returns row 2's x offset from row 1 in px. This will either be 0 or half a
   * tile length.
   * @param {number} tilesPerRow The number of tiles per row.
   * @return {number} The offset for row 2.
   * @private
   */
  getRow2Offset_(tilesPerRow) {
    // An odd number of tiles requires a half tile offset in the second row,
    // unless both rows are full (i.e. for smaller window widths).
    if (this.tiles_.length % 2 === 1 && this.tiles_.length / tilesPerRow < 2) {
      return Math.round(this.tileWidth_ / 2);
    }
    return 0;
  }


  /**
   * Returns true if the browser is in RTL.
   * @return {boolean}
   * @private
   */
  isRtl_() {
    return document.documentElement.dir === 'rtl';
  }


  /**
   * Translates the |element| by (x, y).
   * @param {!Element} element The element to apply the transform to.
   * @param {number} x The x value.
   * @param {number} y The y value.
   * @private
   */
  translate_(element, x, y) {
    const rtlX = x * (this.isRtl_() ? -1 : 1);
    element.style.transform = 'translate(' + rtlX + 'px, ' + y + 'px)';
  }
}


/**
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
function logEvent(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
}

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
function countLoad() {
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
}


/**
 * Handles postMessages coming from the host page to the iframe.
 * Mostly, it dispatches every command to handleCommand.
 */
function handlePostMessage(event) {
  if (event.data instanceof Array) {
    for (let i = 0; i < event.data.length; ++i) {
      handleCommand(event.data[i]);
    }
  } else {
    handleCommand(event.data);
  }
}


/**
 * Handles a single command coming from the host page to the iframe.
 * We try to keep the logic here to a minimum and just dispatch to the relevant
 * functions.
 */
function handleCommand(data) {
  const cmd = data.cmd;

  if (cmd == 'tile') {
    addTile(data);
  } else if (cmd == 'show') {
    // TODO(crbug.com/946225): If this happens before we have finished loading
    // the previous tiles, we probably get into a bad state. If/when the iframe
    // is removed this might no longer be a concern.
    showTiles(data);
  } else if (cmd == 'updateTheme') {
    updateTheme(data);
  } else if (cmd === 'focusMenu') {
    focusTileMenu(data);
  } else {
    console.error('Unknown command: ' + JSON.stringify(data));
  }
}


/**
 * Handler for the 'show' message from the host page.
 * @param {!Object} info Data received in the message.
 */
function showTiles(info) {
  logEvent(LOG_TYPE.NTP_ALL_TILES_RECEIVED);
  utils.setPlatformClass(document.body);
  countLoad();
}


/**
 * Handler for the 'updateTheme' message from the host page.
 * @param {!Object} info Data received in the message.
 */
function updateTheme(info) {
  document.body.style.setProperty('--tile-title-color', info.tileTitleColor);
  document.body.classList.toggle('dark-theme', info.isThemeDark);
  document.body.classList.toggle('using-theme', info.isUsingTheme);
  document.documentElement.setAttribute('darkmode', info.isDarkMode);

  // Reduce font weight on the default(white) background for Mac and CrOS.
  document.body.classList.toggle(
      CLASSES.MAC_CHROMEOS,
      !info.isThemeDark && !info.isUsingTheme &&
          (navigator.userAgent.indexOf('Mac') > -1 ||
           navigator.userAgent.indexOf('CrOS') > -1));
}


/**
 * Handler for 'focusMenu' message from the host page. Focuses the edited tile's
 * menu or the add shortcut tile after closing the custom link edit dialog
 * without saving.
 * @param {!Object} info Data received in the message.
 */
function focusTileMenu(info) {
  const tile = document.querySelector(`a.md-tile[data-tid="${info.tid}"]`);
  if (info.tid === -1 /* Add shortcut tile */) {
    tile.focus();
  } else {
    tile.parentNode.childNodes[1].focus();
  }
}


/**
 * Removes all old instances of |IDS.MV_TILES| that are pending for deletion.
 */
function removeAllOldTiles() {
  const parent = document.querySelector('#' + IDS.MOST_VISITED);
  const oldList = parent.querySelectorAll('.mv-tiles-old');
  for (let i = 0; i < oldList.length; ++i) {
    parent.removeChild(oldList[i]);
  }
}


/**
 * Called when all tiles have finished loading (successfully or not), including
 * their thumbnail images, and we are ready to show the new tiles and drop the
 * old ones.
 */
function swapInNewTiles() {
  // Store the tiles on the current closure.
  const cur = tiles;

  // Add an "add new custom link" button if we haven't reached the maximum
  // number of tiles.
  if (isCustomLinksEnabled && cur.childNodes.length < maxNumTiles) {
    const data = {
      'tid': -1,
      'title': queryArgs['addLink'],
      'url': '',
      'isAddButton': true,
      'dataGenerationTime': new Date(),
      'tileSource': -1,
      'tileTitleSource': -1
    };
    tiles.appendChild(renderMaterialDesignTile(data));
  }

  const parent = document.querySelector('#' + IDS.MOST_VISITED);

  // Only fade in the new tiles if there were tiles before.
  let fadeIn = false;
  const old = parent.querySelector('#' + IDS.MV_TILES);
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

  // Initialize the new tileset before modifying opacity. This will prevent the
  // transform transition from applying after the tiles fade in.
  currGrid.init(cur);

  const flushOpacity = () => window.getComputedStyle(cur).opacity;

  // getComputedStyle causes the initial style (opacity 0) to be applied, so
  // that when we then set it to 1, that triggers the CSS transition.
  if (fadeIn) {
    flushOpacity();
  }
  cur.style.opacity = 1.0;

  if (document.documentElement.classList.contains(CLASSES.NO_INITIAL_FADE)) {
    flushOpacity();
    document.documentElement.classList.remove(CLASSES.NO_INITIAL_FADE);
  }

  // Make sure the tiles variable contain the next tileset we'll use if the host
  // page sends us an updated set of tiles.
  tiles = document.createElement('div');
}


/**
 * Handler for the 'show' message from the host page, called when it wants to
 * add a suggestion tile.
 * It's also used to fill up our tiles to |maxNumTiles| if necessary.
 * @param {?MostVisitedData} args Data for the tile to be rendered.
 */
function addTile(args) {
  if (isFinite(args.rid)) {
    // An actual suggestion. Grab the data from the embeddedSearch API.
    const data =
        chrome.embeddedSearch.newTabPage.getMostVisitedItemData(args.rid);
    if (!data) {
      return;
    }

    data.tid = data.rid;
    // Use a dark icon if dark mode is enabled.
    data.dark = args.darkMode;
    if (!data.faviconUrl) {
      data.faviconUrl = 'chrome-search://favicon/size/16@' +
          window.devicePixelRatio + 'x/' + data.renderViewId + '/' + data.tid;
    }
    tiles.appendChild(renderTile(data));
  } else {
    // An empty tile
    tiles.appendChild(renderTile(null));
  }
}

/**
 * Called when the user decided to add a tile to the blacklist.
 * It sets off the animation for the blacklist and sends the blacklisted id
 * to the host page.
 * @param {Element} tile DOM node of the tile we want to remove.
 */
function blacklistTile(tile) {
  const tid = Number(tile.firstChild.getAttribute('data-tid'));

  if (isCustomLinksEnabled) {
    chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(tid);
  } else {
    tile.classList.add('blacklisted');
    tile.addEventListener('transitionend', function(ev) {
      if (ev.propertyName != 'width') {
        return;
      }
      window.parent.postMessage(
          {cmd: 'tileBlacklisted', tid: Number(tid)}, DOMAIN_ORIGIN);
    });
  }
}


/**
 * Starts edit custom link flow. Tells host page to show the edit custom link
 * dialog and pre-populate it with data obtained using the link's id.
 * @param {?number} tid Restricted id of the tile we want to edit.
 */
function editCustomLink(tid) {
  window.parent.postMessage({cmd: 'startEditLink', tid: tid}, DOMAIN_ORIGIN);
}


/**
 * Renders a MostVisited tile to the DOM.
 * @param {?MostVisitedData} data Object containing rid, url, title, favicon,
 *     thumbnail, and optionally isAddButton. isAddButton is true if you want to
 *     construct an add custom link button. data is null if you want to
 *     construct an empty tile. isAddButton can only be set if custom links is
 *     enabled.
 */
function renderTile(data) {
  return renderMaterialDesignTile(data);
}


/**
 * Renders a MostVisited tile with Material Design styles.
 * @param {?MostVisitedData} data Object containing rid, url, title, favicon,
 *     and optionally isAddButton. isAddButton is if you want to construct an
 *     add custom link button. data is null if you want to construct an empty
 *     tile.
 * @return {Element}
 */
function renderMaterialDesignTile(data) {
  const mdTileContainer = document.createElement('div');
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

  const mdTile = document.createElement('a');
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
      editCustomLink(null);
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

  const mdTileInner = document.createElement('div');
  mdTileInner.className = CLASSES.MD_TILE_INNER;

  const mdIcon = document.createElement('div');
  mdIcon.className = CLASSES.MD_ICON;

  if (data.isAddButton) {
    const mdAdd = document.createElement('div');
    mdAdd.className = CLASSES.MD_ADD_ICON;
    const addBackground = document.createElement('div');
    addBackground.className = CLASSES.MD_ICON_BACKGROUND;

    addBackground.appendChild(mdAdd);
    mdIcon.appendChild(addBackground);
  } else {
    const fi = document.createElement('img');
    // Set title and alt to empty so screen readers won't say the image name.
    fi.title = '';
    fi.alt = '';
    const url = new URL('chrome-search://ntpicon/');
    url.searchParams.set('dark', data.dark);
    url.searchParams.set('size', '24@' + window.devicePixelRatio + 'x');
    url.searchParams.set('url', data.url);
    fi.src = url.toString();
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
      const fallbackBackground = document.createElement('div');
      fallbackBackground.className = CLASSES.MD_ICON_BACKGROUND;
      const fallbackLetter = document.createElement('div');
      fallbackLetter.className = CLASSES.MD_FALLBACK_LETTER;
      fallbackLetter.textContent = data.title.charAt(0).toUpperCase();
      mdIcon.classList.add(CLASSES.FAILED_FAVICON);

      fallbackBackground.appendChild(fallbackLetter);
      mdIcon.removeChild(fi);
      mdIcon.appendChild(fallbackBackground);

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

    mdIcon.appendChild(fi);
  }

  mdTileInner.appendChild(mdIcon);

  const mdTitleContainer = document.createElement('div');
  mdTitleContainer.className = CLASSES.MD_TITLE_CONTAINER;
  const mdTitle = document.createElement('div');
  mdTitle.className = CLASSES.MD_TITLE;
  const mdTitleTextwrap = document.createElement('span');
  mdTitleTextwrap.innerText = data.title;
  mdTitle.style.direction = data.direction || 'ltr';
  mdTitleContainer.appendChild(mdTitle);
  mdTileInner.appendChild(mdTitleContainer);
  mdTile.appendChild(mdTileInner);
  mdTileContainer.appendChild(mdTile);
  mdTitle.appendChild(mdTitleTextwrap);

  if (!data.isAddButton) {
    const mdMenu = document.createElement('button');
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
      ev.stopPropagation();
    });
    utils.disableOutlineOnMouseClick(mdMenu);

    mdTileContainer.appendChild(mdMenu);
  }

  return currGrid.createGridTile(mdTileContainer);
}


/**
 * Does some initialization and parses the query arguments passed to the iframe.
 */
function init() {
  // Create a new DOM element to hold the tiles. The tiles will be added
  // one-by-one via addTile, and the whole thing will be inserted into the page
  // in swapInNewTiles, after the parent has sent us the 'show' message, and all
  // thumbnails and favicons have loaded.
  tiles = document.createElement('div');

  // Parse query arguments.
  const query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (let i = 0; i < query.length; ++i) {
    const val = query[i].split('=');
    if (val[0] == '') {
      continue;
    }
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  document.title = queryArgs['title'];

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    document.documentElement.dir = 'rtl';
  }

  // Enable custom links.
  if (queryArgs['enableCustomLinks'] == '1') {
    isCustomLinksEnabled = true;
  }

  // Set the maximum number of tiles to show.
  if (isCustomLinksEnabled) {
    maxNumTiles = MD_MAX_NUM_CUSTOM_LINK_TILES;
  }

  currGrid = new Grid();
  // Set up layout updates on window resize. Throttled according to
  // |RESIZE_TIMEOUT_DELAY|.
  let resizeTimeout;
  window.onresize = () => {
    if (resizeTimeout) {
      window.clearTimeout(resizeTimeout);
    }
    resizeTimeout = window.setTimeout(() => {
      resizeTimeout = null;
      currGrid.onResize();
    }, RESIZE_TIMEOUT_DELAY);
  };

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}


return {
  Grid: Grid,      // Exposed for testing.
  init: init,      // Exposed for testing.
  listen: listen,
};
}

if (!window.mostVisitedUnitTest) {
  MostVisited().listen();
}
