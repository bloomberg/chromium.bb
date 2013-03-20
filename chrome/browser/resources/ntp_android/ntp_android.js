// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File Description:
//     Contains all the necessary functions for rendering the NTP on mobile
//     devices.

/**
 * The event type used to determine when a touch starts.
 * @type {string}
 */
var PRESS_START_EVT = 'touchstart';

/**
 * The event type used to determine when a touch finishes.
 * @type {string}
 */
var PRESS_STOP_EVT = 'touchend';

/**
 * The event type used to determine when a touch moves.
 * @type {string}
 */
var PRESS_MOVE_EVT = 'touchmove';

cr.define('ntp', function() {
  /**
   * Constant for the localStorage key used to specify the default bookmark
   * folder to be selected when navigating to the bookmark tab for the first
   * time of a new NTP instance.
   * @type {string}
   */
  var DEFAULT_BOOKMARK_FOLDER_KEY = 'defaultBookmarkFolder';

  /**
   * Constant for the localStorage key used to store whether or not sync was
   * enabled on the last call to syncEnabled().
   * @type {string}
   */
  var SYNC_ENABLED_KEY = 'syncEnabled';

  /**
   * The time before and item gets marked as active (in milliseconds).  This
   * prevents an item from being marked as active when the user is scrolling
   * the page.
   * @type {number}
   */
  var ACTIVE_ITEM_DELAY_MS = 100;

  /**
   * The CSS class identifier for grid layouts.
   * @type {string}
   */
  var GRID_CSS_CLASS = 'icon-grid';

  /**
   * The element to center when centering a GRID_CSS_CLASS.
   */
  var GRID_CENTER_CSS_CLASS = 'center-icon-grid';

  /**
   * Attribute used to specify the number of columns to use in a grid.  If
   * left unspecified, the grid will fill the container.
   */
  var GRID_COLUMNS = 'grid-columns';

  /**
   * Attribute used to specify whether the top margin should be set to match
   * the left margin of the grid.
   */
  var GRID_SET_TOP_MARGIN_CLASS = 'grid-set-top-margin';

  /**
   * Attribute used to specify whether the margins of individual items within
   * the grid should be adjusted to better fill the space.
   */
  var GRID_SET_ITEM_MARGINS = 'grid-set-item-margins';

  /**
   * The CSS class identifier for centered empty section containers.
   */
  var CENTER_EMPTY_CONTAINER_CSS_CLASS = 'center-empty-container';

  /**
   * The CSS class identifier for marking list items as active.
   * @type {string}
   */
  var ACTIVE_LIST_ITEM_CSS_CLASS = 'list-item-active';

  /**
   * Attributes set on elements representing data in a section, specifying
   * which section that element belongs to. Used for context menus.
   * @type {string}
   */
  var SECTION_KEY = 'sectionType';

  /**
   * Attribute set on an element that has a context menu. Specifies the URL for
   * which the context menu action should apply.
   * @type {string}
   */
  var CONTEXT_MENU_URL_KEY = 'url';

  /**
   * The list of main section panes added.
   * @type {Array.<Element>}
   */
  var panes = [];

  /**
   * The list of section prefixes, which are used to append to the hash of the
   * page to allow the native toolbar to see url changes when the pane is
   * switched.
   */
  var sectionPrefixes = [];

  /**
   * The next available index for new favicons.  Users must increment this
   * value once assigning this index to a favicon.
   * @type {number}
   */
  var faviconIndex = 0;

  /**
   * The currently selected pane DOM element.
   * @type {Element}
   */
  var currentPane = null;

  /**
   * The index of the currently selected top level pane.  The index corresponds
   * to the elements defined in {@see #panes}.
   * @type {number}
   */
  var currentPaneIndex;

  /**
   * The ID of the bookmark folder currently selected.
   * @type {string|number}
   */
  var bookmarkFolderId = null;

  /**
   * The current element active item.
   * @type {?Element}
   */
  var activeItem;

  /**
   * The element to be marked as active if no actions cancel it.
   * @type {?Element}
   */
  var pendingActiveItem;

  /**
   * The timer ID to mark an element as active.
   * @type {number}
   */
  var activeItemDelayTimerId;

  /**
   * Enum for the different load states based on the initialization of the NTP.
   * @enum {number}
   */
  var LoadStatusType = {
    LOAD_NOT_DONE: 0,
    LOAD_IMAGES_COMPLETE: 1,
    LOAD_BOOKMARKS_FINISHED: 2,
    LOAD_COMPLETE: 3  // An OR'd combination of all necessary states.
  };

  /**
   * The current loading status for the NTP.
   * @type {LoadStatusType}
   */
  var loadStatus_ = LoadStatusType.LOAD_NOT_DONE;

  /**
   * Whether the loading complete notification has been sent.
   * @type {boolean}
   */
  var finishedLoadingNotificationSent_ = false;

  /**
   * Whether the NTP is in incognito mode or not.
   * @type {boolean}
   */
  var isIncognito = false;

  /**
   * Whether the initial history state has been replaced.  The state will be
   * replaced once the bookmark data has loaded to ensure the proper folder
   * id is persisted.
   * @type {boolean}
   */
  var replacedInitialState = false;

  /**
   * Stores number of most visited pages.
   * @type {number}
   */
  var numberOfMostVisitedPages = 0;

  /**
   * Whether there are any recently closed tabs.
   * @type {boolean}
   */
  var hasRecentlyClosedTabs = false;

  /**
   * Whether promo is not allowed or not (external to NTP).
   * @type {boolean}
   */
  var promoIsAllowed = false;

  /**
   * Whether promo should be shown on Most Visited page (externally set).
   * @type {boolean}
   */
  var promoIsAllowedOnMostVisited = false;

  /**
   * Whether promo should be shown on Open Tabs page (externally set).
   * @type {boolean}
   */
  var promoIsAllowedOnOpenTabs = false;

  /**
   * Whether promo should show a virtual computer on Open Tabs (externally set).
   * @type {boolean}
   */
  var promoIsAllowedAsVirtualComputer = false;

  /**
   * Promo-injected title of a virtual computer on an open tabs pane.
   * @type {string}
   */
  var promoInjectedComputerTitleText = '';

  /**
   * Promo-injected last synced text of a virtual computer on an open tabs pane.
   * @type {string}
   */
  var promoInjectedComputerLastSyncedText = '';

  /**
   * The different sections that are displayed.
   * @enum {number}
   */
  var SectionType = {
    BOOKMARKS: 'bookmarks',
    FOREIGN_SESSION: 'foreign_session',
    FOREIGN_SESSION_HEADER: 'foreign_session_header',
    MOST_VISITED: 'most_visited',
    PROMO_VC_SESSION_HEADER: 'promo_vc_session_header',
    RECENTLY_CLOSED: 'recently_closed',
    SNAPSHOTS: 'snapshots',
    UNKNOWN: 'unknown',
  };

  /**
   * The different ids used of our custom context menu. Sent to the ChromeView
   * and sent back when a menu is selected.
   * @enum {number}
   */
  var ContextMenuItemIds = {
    BOOKMARK_EDIT: 0,
    BOOKMARK_DELETE: 1,
    BOOKMARK_OPEN_IN_NEW_TAB: 2,
    BOOKMARK_OPEN_IN_INCOGNITO_TAB: 3,
    BOOKMARK_SHORTCUT: 4,

    MOST_VISITED_OPEN_IN_NEW_TAB: 10,
    MOST_VISITED_OPEN_IN_INCOGNITO_TAB: 11,
    MOST_VISITED_REMOVE: 12,

    RECENTLY_CLOSED_OPEN_IN_NEW_TAB: 20,
    RECENTLY_CLOSED_OPEN_IN_INCOGNITO_TAB: 21,
    RECENTLY_CLOSED_REMOVE: 22,

    FOREIGN_SESSIONS_REMOVE: 30,

    PROMO_VC_SESSION_REMOVE: 40,
  };

  /**
   * The URL of the element for the context menu.
   * @type {string}
   */
  var contextMenuUrl = null;

  var contextMenuItem = null;

  var currentSnapshots = null;

  var currentSessions = null;

  /**
   * The possible states of the sync section
   * @enum {number}
   */
  var SyncState = {
    INITIAL: 0,
    WAITING_FOR_DATA: 1,
    DISPLAYING_LOADING: 2,
    DISPLAYED_LOADING: 3,
    LOADED: 4,
  };

  /**
   * The current state of the sync section.
   */
  var syncState = SyncState.INITIAL;

  /**
   * Whether or not sync is enabled. It will be undefined until
   * setSyncEnabled() is called.
   * @type {?boolean}
   */
  var syncEnabled = undefined;

  /**
   * The current most visited data being displayed.
   * @type {Array.<Object>}
   */
  var mostVisitedData_ = [];

  /**
   * The current bookmark data being displayed. Keep a reference to this data
   * in case the sync enabled state changes.  In this case, the bookmark data
   * will need to be refiltered.
   * @type {?Object}
   */
  var bookmarkData;

  /**
   * Keep track of any outstanding timers related to updating the sync section.
   */
  var syncTimerId = -1;

  /**
   * The minimum amount of time that 'Loading...' can be displayed. This is to
   * prevent flashing.
   */
  var SYNC_LOADING_TIMEOUT = 1000;

  /**
   * How long to wait for sync data to load before displaying the 'Loading...'
   * text to the user.
   */
  var SYNC_INITIAL_LOAD_TIMEOUT = 1000;

  /**
   * An array of images that are currently in loading state. Once an image
   * loads it is removed from this array.
   */
  var imagesBeingLoaded = new Array();

  /**
   * Flag indicating if we are on bookmark shortcut mode.
   * In this mode, only the bookmark section is available and selecting
   * a non-folder bookmark adds it to the home screen.
   * Context menu is disabled.
   */
  var bookmarkShortcutMode = false;

  function setIncognitoMode(incognito) {
    isIncognito = incognito;
  }

  /**
   * Flag set to true when the page is loading its initial set of images. This
   * is set to false after all the initial images have loaded.
   */
  function onInitialImageLoaded(event) {
    var url = event.target.src;
    for (var i = 0; i < imagesBeingLoaded.length; ++i) {
      if (imagesBeingLoaded[i].src == url) {
        imagesBeingLoaded.splice(i, 1);
        if (imagesBeingLoaded.length == 0) {
          // To send out the NTP loading complete notification.
          loadStatus_ |= LoadStatusType.LOAD_IMAGES_COMPLETE;
          sendNTPNotification();
        }
      }
    }
  }

  /**
   * Marks the given image as currently being loaded. Once all such images load
   * we inform the browser via a hash change.
   */
  function trackImageLoad(url) {
    if (finishedLoadingNotificationSent_)
      return;

    for (var i = 0; i < imagesBeingLoaded.length; ++i) {
      if (imagesBeingLoaded[i].src == url)
        return;
    }

    loadStatus_ &= (~LoadStatusType.LOAD_IMAGES_COMPLETE);

    var image = new Image();
    image.onload = onInitialImageLoaded;
    image.onerror = onInitialImageLoaded;
    image.src = url;
    imagesBeingLoaded.push(image);
  }

  /**
   * Initializes all the UI once the page has loaded.
   */
  function init() {
    // Special case to handle NTP caching.
    if (window.location.hash == '#cached_ntp')
      document.location.hash = '#most_visited';
    // Special case to show a specific bookmarks folder.
    // Used to show the mobile bookmarks folder after importing.
    var bookmarkIdMatch = window.location.hash.match(/#bookmarks:(\d+)/);
    if (bookmarkIdMatch && bookmarkIdMatch.length == 2) {
      localStorage.setItem(DEFAULT_BOOKMARK_FOLDER_KEY, bookmarkIdMatch[1]);
      document.location.hash = '#bookmarks';
    }
    // Special case to choose a bookmark for adding a shortcut.
    // See the doc of bookmarkShortcutMode for details.
    if (window.location.hash == '#bookmark_shortcut')
      bookmarkShortcutMode = true;
    // Make sure a valid section is always displayed.  Both normal and
    // incognito NTPs have a bookmarks section.
    if (getPaneIndexFromHash() < 0)
      document.location.hash = '#bookmarks';

    // Initialize common widgets.
    var titleScrollers =
        document.getElementsByClassName('section-title-wrapper');
    for (var i = 0, len = titleScrollers.length; i < len; i++)
      initializeTitleScroller(titleScrollers[i]);

    // Initialize virtual computers for the sync promo.
    createPromoVirtualComputers();

    chrome.send('getMostVisited');
    chrome.send('getRecentlyClosedTabs');
    chrome.send('getForeignSessions');
    chrome.send('getPromotions');

    setCurrentBookmarkFolderData(
        localStorage.getItem(DEFAULT_BOOKMARK_FOLDER_KEY));

    addMainSection('incognito');
    addMainSection('most_visited');
    addMainSection('bookmarks');
    addMainSection('open_tabs');

    computeDynamicLayout();

    scrollToPane(getPaneIndexFromHash());
    updateSyncEmptyState();

    window.onpopstate = onPopStateHandler;
    window.addEventListener('hashchange', updatePaneOnHash);
    window.addEventListener('resize', windowResizeHandler);

    if (!bookmarkShortcutMode)
      window.addEventListener('contextmenu', contextMenuHandler);
  }

  /**
   * Notifies the chrome process of the status of the NTP.
   */
  function sendNTPNotification() {
    if (loadStatus_ != LoadStatusType.LOAD_COMPLETE)
      return;

    if (!finishedLoadingNotificationSent_) {
      finishedLoadingNotificationSent_ = true;
      chrome.send('notifyNTPReady');
    } else {
      // Navigating after the loading complete notification has been sent
      // might break tests.
      chrome.send('NTPUnexpectedNavigation');
    }
  }

  /**
   * The default click handler for created item shortcuts.
   *
   * @param {Object} item The item specification.
   * @param {function} evt The browser click event triggered.
   */
  function itemShortcutClickHandler(item, evt) {
    // Handle the touch callback
    if (item['folder']) {
      browseToBookmarkFolder(item.id);
    } else {
      if (bookmarkShortcutMode) {
        chrome.send('createHomeScreenBookmarkShortcut', [item.id]);
      } else if (!!item.url) {
        window.location = item.url;
      }
    }
  }

  /**
   * Opens a recently closed tab.
   *
   * @param {Object} item An object containing the necessary information to
   *     reopen a tab.
   */
  function openRecentlyClosedTab(item, evt) {
    chrome.send('reopenTab', [item.sessionId]);
  }

  /**
   * Creates a 'div' DOM element.
   *
   * @param {string} className The CSS class name for the DIV.
   * @param {string=} opt_backgroundUrl The background URL to be applied to the
   *     DIV if required.
   * @return {Element} The newly created DIV element.
   */
  function createDiv(className, opt_backgroundUrl) {
    var div = document.createElement('div');
    div.className = className;
    if (opt_backgroundUrl)
      div.style.backgroundImage = 'url(' + opt_backgroundUrl + ')';
    return div;
  }

  /**
   * Helper for creating new DOM elements.
   *
   * @param {string} type The type of Element to be created (i.e. 'div',
   *     'span').
   * @param {Object} params A mapping of element attribute key and values that
   *     should be applied to the new element.
   * @return {Element} The newly created DOM element.
   */
  function createElement(type, params) {
    var el = document.createElement(type);
    if (typeof params === 'string') {
      el.className = params;
    } else {
      for (attr in params) {
        el[attr] = params[attr];
      }
    }
    return el;
  }

  /**
   * Adds a click listener to a specified element with the ability to override
   * the default value of itemShortcutClickHandler.
   *
   * @param {Element} el The element the click listener should be added to.
   * @param {Object} item The item data represented by the element.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection.
   */
  function wrapClickHandler(el, item, opt_clickCallback) {
    el.addEventListener('click', function(evt) {
      var clickCallback =
          opt_clickCallback ? opt_clickCallback : itemShortcutClickHandler;
      clickCallback(item, evt);
    });
  }

  /**
   * Create a DOM element to contain a recently closed item for a tablet
   * device.
   *
   * @param {Object} item The data of the item used to generate the shortcut.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection (if not provided it will
   *     use the default -- itemShortcutClickHandler).
   * @return {Element} The shortcut element created.
   */
  function makeRecentlyClosedTabletItem(item, opt_clickCallback) {
    var cell = createDiv('cell');

    cell.setAttribute(CONTEXT_MENU_URL_KEY, item.url);

    var iconUrl = item.icon;
    if (!iconUrl) {
      iconUrl = 'chrome://touch-icon/size/16@' + window.devicePixelRatio +
          'x/' + item.url;
    }
    var icon = createDiv('icon', iconUrl);
    trackImageLoad(iconUrl);
    cell.appendChild(icon);

    var title = createDiv('title');
    title.textContent = item.title;
    cell.appendChild(title);

    wrapClickHandler(cell, item, opt_clickCallback);

    return cell;
  }

  /**
   * Creates a shortcut DOM element based on the item specified item
   * configuration using the thumbnail layout used for most visited.  Other
   * data types should not use this as they won't have a thumbnail.
   *
   * @param {Object} item The data of the item used to generate the shortcut.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection (if not provided it will
   *     use the default -- itemShortcutClickHandler).
   * @return {Element} The shortcut element created.
   */
  function makeMostVisitedItem(item, opt_clickCallback) {
    // thumbnail-cell          -- main outer container
    //   thumbnail-container   -- container for the thumbnail
    //     thumbnail           -- the actual thumbnail image; outer border
    //     inner-border        -- inner border
    //   title                 -- container for the title
    //     img                 -- hack align title text baseline with bottom
    //     title text          -- the actual text of the title
    var thumbnailCell = createDiv('thumbnail-cell');
    var thumbnailContainer = createDiv('thumbnail-container');
    var backgroundUrl = item.thumbnailUrl || 'chrome://thumb/' + item.url;
    if (backgroundUrl == 'chrome://thumb/chrome://welcome/') {
      // Ideally, it would be nice to use the URL as is.  However, as of now
      // theme support has been removed from Chrome.  Instead, load the image
      // URL from a style and use it.  Don't just use the style because
      // trackImageLoad(...) must be called with the background URL.
      var welcomeStyle = findCssRule('.welcome-to-chrome').style;
      var backgroundImage = welcomeStyle.backgroundImage;
      // trim the "url(" prefix and ")" suffix
      backgroundUrl = backgroundImage.substring(4, backgroundImage.length - 1);
    }
    trackImageLoad(backgroundUrl);
    var thumbnail = createDiv('thumbnail');
    // Use an Image object to ensure the thumbnail image actually exists.  If
    // not, this will allow the default to show instead.
    var thumbnailImg = new Image();
    thumbnailImg.onload = function() {
      thumbnail.style.backgroundImage = 'url(' + backgroundUrl + ')';
    };
    thumbnailImg.src = backgroundUrl;

    thumbnailContainer.appendChild(thumbnail);
    var innerBorder = createDiv('inner-border');
    thumbnailContainer.appendChild(innerBorder);
    thumbnailCell.appendChild(thumbnailContainer);
    var title = createDiv('title');
    title.textContent = item.title;
    var spacerImg = createElement('img', 'title-spacer');
    spacerImg.alt = '';
    title.insertBefore(spacerImg, title.firstChild);
    thumbnailCell.appendChild(title);

    var shade = createDiv('thumbnail-cell-shade');
    thumbnailContainer.appendChild(shade);
    addActiveTouchListener(shade, 'thumbnail-cell-shade-active');

    wrapClickHandler(thumbnailCell, item, opt_clickCallback);

    thumbnailCell.setAttribute(CONTEXT_MENU_URL_KEY, item.url);
    thumbnailCell.contextMenuItem = item;
    return thumbnailCell;
  }

  /**
   * Creates a shortcut DOM element based on the item specified item
   * configuration using the favicon layout used for bookmarks.
   *
   * @param {Object} item The data of the item used to generate the shortcut.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection (if not provided it will
   *     use the default -- itemShortcutClickHandler).
   * @return {Element} The shortcut element created.
   */
  function makeBookmarkItem(item, opt_clickCallback) {
    var holder = createDiv('favicon-cell');
    addActiveTouchListener(holder, 'favicon-cell-active');

    holder.setAttribute(CONTEXT_MENU_URL_KEY, item.url);
    holder.contextMenuItem = item;
    var faviconBox = createDiv('favicon-box');
    if (item.folder) {
      faviconBox.classList.add('folder');
    } else {
      var iconUrl = item.icon || 'chrome://touch-icon/largest/' + item.url;
      var faviconIcon = createDiv('favicon-icon');
      faviconIcon.style.backgroundImage = 'url(' + iconUrl + ')';
      trackImageLoad(iconUrl);

      var image = new Image();
      image.src = iconUrl;
      image.onload = function() {
        var w = image.width;
        var h = image.height;
        var wDip = w / window.devicePixelRatio;
        var hDip = h / window.devicePixelRatio;
        if (Math.floor(wDip) <= 16 || Math.floor(hDip) <= 16) {
          // it's a standard favicon (or at least it's small).
          faviconBox.classList.add('document');

          faviconBox.appendChild(
              createDiv('color-strip colorstrip-' + faviconIndex));
          faviconBox.appendChild(createDiv('bookmark-border'));
          var foldDiv = createDiv('fold');
          foldDiv.id = 'fold_' + faviconIndex;
          foldDiv.style['background'] =
              '-webkit-canvas(fold_' + faviconIndex + ')';

          // Use a container so that the fold it self can be zoomed without
          // changing the positioning of the fold.
          var foldContainer = createDiv('fold-container');
          foldContainer.appendChild(foldDiv);
          faviconBox.appendChild(foldContainer);

          // FaviconWebUIHandler::HandleGetFaviconDominantColor expects
          // an URL that starts with chrome://favicon/size/.
          // The handler always loads 16x16 1x favicon and assumes that
          // the dominant color for all scale factors is the same.
          chrome.send('getFaviconDominantColor',
              [('chrome://favicon/size/16@1x/' + item.url), '' + faviconIndex]);
          faviconIndex++;
        } else if ((w == 57 && h == 57) || (w == 114 && h == 114)) {
          // it's a touch icon for 1x or 2x.
          faviconIcon.classList.add('touch-icon');
        } else {
          // It's an html5 icon (or at least it's larger).
          // Rescale it to be no bigger than 64x64 dip.
          var maxDip = 64; // DIP
          if (wDip > maxDip || hDip > maxDip) {
            var scale = (wDip > hDip) ? (maxDip / wDip) : (maxDip / hDip);
            wDip *= scale;
            hDip *= scale;
          }
          faviconIcon.style.backgroundSize = wDip + 'px ' + hDip + 'px';
        }
      };
      faviconBox.appendChild(faviconIcon);
    }
    holder.appendChild(faviconBox);

    var title = createDiv('title');
    title.textContent = item.title;
    holder.appendChild(title);

    wrapClickHandler(holder, item, opt_clickCallback);

    return holder;
  }

  /**
   * Adds touch listeners to the specified element to apply a class when it is
   * selected (removing the class when no longer pressed).
   *
   * @param {Element} el The element to apply the class to when touched.
   * @param {string} activeClass The CSS class name to be applied when active.
   */
  function addActiveTouchListener(el, activeClass) {
    if (!window.touchCancelListener) {
      window.touchCancelListener = function(evt) {
        if (activeItemDelayTimerId) {
          clearTimeout(activeItemDelayTimerId);
          activeItemDelayTimerId = undefined;
        }
        if (!activeItem) {
          return;
        }
        activeItem.classList.remove(activeItem.dataset.activeClass);
        activeItem = null;
      };
      document.addEventListener('touchcancel', window.touchCancelListener);
    }
    el.dataset.activeClass = activeClass;
    el.addEventListener(PRESS_START_EVT, function(evt) {
      if (activeItemDelayTimerId) {
        clearTimeout(activeItemDelayTimerId);
        activeItemDelayTimerId = undefined;
      }
      activeItemDelayTimerId = setTimeout(function() {
        el.classList.add(activeClass);
        activeItem = el;
      }, ACTIVE_ITEM_DELAY_MS);
    });
    el.addEventListener(PRESS_STOP_EVT, function(evt) {
      if (activeItemDelayTimerId) {
        clearTimeout(activeItemDelayTimerId);
        activeItemDelayTimerId = undefined;
      }
      // Add the active class to ensure the pressed state is visible when
      // quickly tapping, which can happen if the start and stop events are
      // received before the active item delay timer has been executed.
      el.classList.add(activeClass);
      el.classList.add('no-active-delay');
      setTimeout(function() {
        el.classList.remove(activeClass);
        el.classList.remove('no-active-delay');
      }, 0);
      activeItem = null;
    });
  }

  /**
   * Creates a shortcut DOM element based on the item specified in the list
   * format.
   *
   * @param {Object} item The data of the item used to generate the shortcut.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection (if not provided it will
   *     use the default -- itemShortcutClickHandler).
   * @return {Element} The shortcut element created.
   */
  function makeListEntryItem(item, opt_clickCallback) {
    var listItem = createDiv('list-item');
    addActiveTouchListener(listItem, ACTIVE_LIST_ITEM_CSS_CLASS);
    listItem.setAttribute(CONTEXT_MENU_URL_KEY, item.url);
    var iconSize = item.iconSize || 64;
    var iconUrl = item.icon ||
        'chrome://touch-icon/size/' + iconSize + '@1x/' + item.url;
    listItem.appendChild(createDiv('icon', iconUrl));
    trackImageLoad(iconUrl);
    var title = createElement('span', {
      textContent: item.title,
      className: 'title'
    });
    listItem.appendChild(title);
    listItem.addEventListener('click', function(evt) {
      var clickCallback =
          opt_clickCallback ? opt_clickCallback : itemShortcutClickHandler;
      clickCallback(item, evt);
    });
    if (item.divider == 'section') {
      // Add a child div because the section divider has a gradient and
      // webkit doesn't seem to currently support borders with gradients.
      listItem.appendChild(createDiv('section-divider'));
    } else {
      listItem.classList.add('standard-divider');
    }
    return listItem;
  }

  /**
   * Creates a DOM list entry for a remote session or tab.
   *
   * @param {Object} item The data of the item used to generate the shortcut.
   * @param {function(Object, string, BrowserEvent)=} opt_clickCallback The
   *     click callback to be triggered upon selection (if not provided it will
   *     use the default -- itemShortcutClickHandler).
   * @return {Element} The shortcut element created.
   */
  function makeForeignSessionListEntry(item, opt_clickCallback) {
    // Session item
    var sessionOuterDiv = createDiv('list-item standard-divider');
    addActiveTouchListener(sessionOuterDiv, ACTIVE_LIST_ITEM_CSS_CLASS);
    sessionOuterDiv.contextMenuItem = item;

    var icon = createDiv('session-icon ' + item.iconStyle);
    sessionOuterDiv.appendChild(icon);

    var titleContainer = createElement('span', 'title');
    sessionOuterDiv.appendChild(titleContainer);

    // Extra container to allow title & last-sync time to stack vertically.
    var sessionInnerDiv = createDiv(null);
    titleContainer.appendChild(sessionInnerDiv);

    var title = createDiv('session-name');
    title.textContent = item.title;
    title.id = item.titleId || '';
    sessionInnerDiv.appendChild(title);

    var lastSynced = createDiv('session-last-synced');
    lastSynced.textContent =
        templateData.opentabslastsynced + ': ' + item.userVisibleTimestamp;
    lastSynced.id = item.userVisibleTimestampId || '';
    sessionInnerDiv.appendChild(lastSynced);

    sessionOuterDiv.addEventListener('click', function(evt) {
      var clickCallback =
          opt_clickCallback ? opt_clickCallback : itemShortcutClickHandler;
      clickCallback(item, evt);
    });
    return sessionOuterDiv;
  }

  /**
   * Saves the number of most visited pages and updates promo visibility.
   * @param {number} n Number of most visited pages.
   */
  function setNumberOfMostVisitedPages(n) {
    numberOfMostVisitedPages = n;
    updatePromoVisibility();
  }

  /**
   * Saves the recently closed tabs flag and updates promo visibility.
   * @param {boolean} anyTabs Whether there are any recently closed tabs.
   */
  function setHasRecentlyClosedTabs(anyTabs) {
    hasRecentlyClosedTabs = anyTabs;
    updatePromoVisibility();
  }

  /**
   * Updates the most visited pages.
   *
   * @param {Array.<Object>} List of data for displaying the list of most
   *     visited pages (see C++ handler for model description).
   * @param {boolean} hasBlacklistedUrls Whether any blacklisted URLs are
   *     present.
   */
  function setMostVisitedPages(data, hasBlacklistedUrls) {
    setNumberOfMostVisitedPages(data.length);
    // limit the number of most visited items to display
    if (isPhone() && data.length > 6) {
      data.splice(6, data.length - 6);
    } else if (isTablet() && data.length > 8) {
      data.splice(8, data.length - 8);
    }

    if (equals(data, mostVisitedData_))
      return;

    var clickFunction = function(item) {
      chrome.send('metricsHandler:recordAction', ['MobileNTPMostVisited']);
      window.location = item.url;
    };
    populateData(findList('most_visited'), SectionType.MOST_VISITED, data,
        makeMostVisitedItem, clickFunction);
    computeDynamicLayout();

    mostVisitedData_ = data;
  }

  /**
   * Updates the recently closed tabs.
   *
   * @param {Array.<Object>} List of data for displaying the list of recently
   *     closed tabs (see C++ handler for model description).
   */
  function setRecentlyClosedTabs(data) {
    var container = $('recently_closed_container');
    if (!data || data.length == 0) {
      // hide the recently closed section if it is empty.
      container.style.display = 'none';
      setHasRecentlyClosedTabs(false);
    } else {
      container.style.display = 'block';
      setHasRecentlyClosedTabs(true);
      var decoratorFunc = isPhone() ? makeListEntryItem :
          makeRecentlyClosedTabletItem;
      populateData(findList('recently_closed'), SectionType.RECENTLY_CLOSED,
          data, decoratorFunc, openRecentlyClosedTab);
    }
    computeDynamicLayout();
  }

  /**
   * Updates the bookmarks.
   *
   * @param {Array.<Object>} List of data for displaying the bookmarks (see
   *     C++ handler for model description).
   */
  function bookmarks(data) {
    bookmarkFolderId = data.id;
    if (!replacedInitialState) {
      history.replaceState(
          {folderId: bookmarkFolderId, selectedPaneIndex: currentPaneIndex},
          null, null);
      replacedInitialState = true;
    }
    if (syncEnabled == undefined) {
      // Wait till we know whether or not sync is enabled before displaying any
      // bookmarks (since they may need to be filtered below)
      bookmarkData = data;
      return;
    }

    var titleWrapper = $('bookmarks_title_wrapper');
    setBookmarkTitleHierarchy(
        titleWrapper, data, data['hierarchy']);

    var filteredBookmarks = data.bookmarks;
    if (!syncEnabled) {
      filteredBookmarks = filteredBookmarks.filter(function(val) {
        return (val.type != 'BOOKMARK_BAR' && val.type != 'OTHER_NODE');
      });
    }
    if (bookmarkShortcutMode) {
      populateData(findList('bookmarks'), SectionType.BOOKMARKS,
          filteredBookmarks, makeBookmarkItem);
    } else {
      var clickFunction = function(item) {
        if (item['folder']) {
          browseToBookmarkFolder(item.id);
        } else if (!!item.url) {
          chrome.send('metricsHandler:recordAction', ['MobileNTPBookmark']);
          window.location = item.url;
        }
      };
      populateData(findList('bookmarks'), SectionType.BOOKMARKS,
          filteredBookmarks, makeBookmarkItem, clickFunction);
    }

    var bookmarkContainer = $('bookmarks_container');

    // update the shadows on the  breadcrumb bar
    computeDynamicLayout();

    if ((loadStatus_ & LoadStatusType.LOAD_BOOKMARKS_FINISHED) !=
        LoadStatusType.LOAD_BOOKMARKS_FINISHED) {
      loadStatus_ |= LoadStatusType.LOAD_BOOKMARKS_FINISHED;
      sendNTPNotification();
    }
  }

  /**
   * Checks if promo is allowed and MostVisited requirements are satisfied.
   * @return {boolean} Whether the promo should be shown on most_visited.
   */
  function shouldPromoBeShownOnMostVisited() {
    return promoIsAllowed && promoIsAllowedOnMostVisited &&
        numberOfMostVisitedPages >= 2 && !hasRecentlyClosedTabs;
  }

  /**
   * Checks if promo is allowed and OpenTabs requirements are satisfied.
   * @return {boolean} Whether the promo should be shown on open_tabs.
   */
  function shouldPromoBeShownOnOpenTabs() {
    var snapshotsCount =
        currentSnapshots == null ? 0 : currentSnapshots.length;
    var sessionsCount = currentSessions == null ? 0 : currentSessions.length;
    return promoIsAllowed && promoIsAllowedOnOpenTabs &&
        (snapshotsCount + sessionsCount != 0);
  }

  /**
   * Checks if promo is allowed and SyncPromo requirements are satisfied.
   * @return {boolean} Whether the promo should be shown on sync_promo.
   */
  function shouldPromoBeShownOnSync() {
    var snapshotsCount =
        currentSnapshots == null ? 0 : currentSnapshots.length;
    var sessionsCount = currentSessions == null ? 0 : currentSessions.length;
    return promoIsAllowed && promoIsAllowedOnOpenTabs &&
        (snapshotsCount + sessionsCount == 0);
  }

  /**
   * Records a promo impression on a given section if necessary.
   * @param {string} section Active section name to check.
   */
  function promoUpdateImpressions(section) {
    if (section == 'most_visited' && shouldPromoBeShownOnMostVisited())
      chrome.send('recordImpression', ['most_visited']);
    else if (section == 'open_tabs' && shouldPromoBeShownOnOpenTabs())
      chrome.send('recordImpression', ['open_tabs']);
    else if (section == 'open_tabs' && shouldPromoBeShownOnSync())
      chrome.send('recordImpression', ['sync_promo']);
  }

  /**
   * Updates the visibility on all promo-related items as necessary.
   */
  function updatePromoVisibility() {
    var mostVisitedEl = $('promo_message_on_most_visited');
    var openTabsVCEl = $('promo_vc_list');
    var syncPromoLegacyEl = $('promo_message_on_sync_promo_legacy');
    var syncPromoReceivedEl = $('promo_message_on_sync_promo_received');
    mostVisitedEl.style.display =
        shouldPromoBeShownOnMostVisited() ? 'block' : 'none';
    syncPromoReceivedEl.style.display =
        shouldPromoBeShownOnSync() ? 'block' : 'none';
    syncPromoLegacyEl.style.display =
        shouldPromoBeShownOnSync() ? 'none' : 'block';
    openTabsVCEl.style.display =
        (shouldPromoBeShownOnOpenTabs() && promoIsAllowedAsVirtualComputer) ?
            'block' : 'none';
  }

  /**
   * Called from native.
   * Clears the promotion.
   */
  function clearPromotions() {
    setPromotions({});
  }

  /**
   * Set the element to a parsed and sanitized promotion HTML string.
   * @param {Element} el The element to set the promotion string to.
   * @param {string} html The promotion HTML string.
   * @throws {Error} In case of non supported markup.
   */
  function setPromotionHtml(el, html) {
    if (!el) return;
    el.innerHTML = '';
    if (!html) return;
    var tags = ['BR', 'DIV', 'BUTTON', 'SPAN'];
    var attrs = {
      class: function(node, value) { return true; },
      style: function(node, value) { return true; },
    };
    try {
      var fragment = parseHtmlSubset(html, tags, attrs);
      el.appendChild(fragment);
    } catch (err) {
      console.error(err.toString());
      // Ignore all errors while parsing or setting the element.
    }
  }

  /**
   * Called from native.
   * Sets the text for all promo-related items, updates
   * promo-send-email-target items to send email on click and
   * updates the visibility of items.
   * @param {Object} promotions Dictionary used to fill-in the text.
   */
  function setPromotions(promotions) {
    var mostVisitedEl = $('promo_message_on_most_visited');
    var openTabsEl = $('promo_message_on_open_tabs');
    var syncPromoReceivedEl = $('promo_message_on_sync_promo_received');

    promoIsAllowed = !!promotions.promoIsAllowed;
    promoIsAllowedOnMostVisited = !!promotions.promoIsAllowedOnMostVisited;
    promoIsAllowedOnOpenTabs = !!promotions.promoIsAllowedOnOpenTabs;
    promoIsAllowedAsVirtualComputer = !!promotions.promoIsAllowedAsVC;

    setPromotionHtml(mostVisitedEl, promotions.promoMessage);
    setPromotionHtml(openTabsEl, promotions.promoMessage);
    setPromotionHtml(syncPromoReceivedEl, promotions.promoMessageLong);

    promoInjectedComputerTitleText = promotions.promoVCTitle || '';
    promoInjectedComputerLastSyncedText = promotions.promoVCLastSynced || '';
    var openTabsVCTitleEl = $('promo_vc_title');
    if (openTabsVCTitleEl)
      openTabsVCTitleEl.textContent = promoInjectedComputerTitleText;
    var openTabsVCLastSyncEl = $('promo_vc_lastsync');
    if (openTabsVCLastSyncEl)
      openTabsVCLastSyncEl.textContent = promoInjectedComputerLastSyncedText;

    if (promoIsAllowed) {
      var promoButtonEls =
          document.getElementsByClassName('promo-button');
      for (var i = 0, len = promoButtonEls.length; i < len; i++) {
        promoButtonEls[i].onclick = executePromoAction;
        addActiveTouchListener(promoButtonEls[i], 'promo-button-active');
      }
    }
    updatePromoVisibility();
  }

  /**
   * On-click handler for promo email targets.
   * Performs the promo action "send email".
   * @param {Object} evt User interface event that triggered the action.
   */
  function executePromoAction(evt) {
    if (evt.preventDefault)
      evt.preventDefault();
    evt.returnValue = false;
    chrome.send('promoActionTriggered');
  }

  /**
   * Called by the browser when a context menu has been selected.
   *
   * @param {number} itemId The id of the item that was selected, as specified
   *     when chrome.send('showContextMenu') was called.
   */
  function onCustomMenuSelected(itemId) {
    switch (itemId) {
      case ContextMenuItemIds.BOOKMARK_OPEN_IN_NEW_TAB:
      case ContextMenuItemIds.MOST_VISITED_OPEN_IN_NEW_TAB:
      case ContextMenuItemIds.RECENTLY_CLOSED_OPEN_IN_NEW_TAB:
        if (contextMenuUrl != null)
          chrome.send('openInNewTab', [contextMenuUrl]);
        break;

      case ContextMenuItemIds.BOOKMARK_OPEN_IN_INCOGNITO_TAB:
      case ContextMenuItemIds.MOST_VISITED_OPEN_IN_INCOGNITO_TAB:
      case ContextMenuItemIds.RECENTLY_CLOSED_OPEN_IN_INCOGNITO_TAB:
        if (contextMenuUrl != null)
          chrome.send('openInIncognitoTab', [contextMenuUrl]);
        break;

      case ContextMenuItemIds.BOOKMARK_EDIT:
        if (contextMenuItem != null)
          chrome.send('editBookmark', [contextMenuItem.id]);
        break;

      case ContextMenuItemIds.BOOKMARK_DELETE:
        if (contextMenuUrl != null)
          chrome.send('deleteBookmark', [contextMenuItem.id]);
        break;

      case ContextMenuItemIds.MOST_VISITED_REMOVE:
        if (contextMenuUrl != null)
          chrome.send('blacklistURLFromMostVisited', [contextMenuUrl]);
        break;

      case ContextMenuItemIds.BOOKMARK_SHORTCUT:
        if (contextMenuUrl != null)
          chrome.send('createHomeScreenBookmarkShortcut', [contextMenuItem.id]);
        break;

      case ContextMenuItemIds.RECENTLY_CLOSED_REMOVE:
        chrome.send('clearRecentlyClosed');
        break;

      case ContextMenuItemIds.FOREIGN_SESSIONS_REMOVE:
        if (contextMenuItem != null) {
          chrome.send(
              'deleteForeignSession', [contextMenuItem.sessionTag]);
          chrome.send('getForeignSessions');
        }
        break;

      case ContextMenuItemIds.PROMO_VC_SESSION_REMOVE:
        chrome.send('promoDisabled');
        break;

      default:
        log.error('Unknown context menu selected id=' + itemId);
        break;
    }
  }

  /**
   * Generates the full bookmark folder hierarchy and populates the scrollable
   * title element.
   *
   * @param {Element} wrapperEl The wrapper element containing the scrollable
   *     title.
   * @param {string} data The current bookmark folder node.
   * @param {Array.<Object>=} opt_ancestry The folder ancestry of the current
   *     bookmark folder.  The list is ordered in order of closest descendant
   *     (the root will always be the last node).  The definition of each
   *     element is:
   *     - id {number}: Unique ID of the folder (N/A for root node).
   *     - name {string}: Name of the folder (N/A for root node).
   *     - root {boolean}: Whether this is the root node.
   */
  function setBookmarkTitleHierarchy(wrapperEl, data, opt_ancestry) {
    var title = wrapperEl.getElementsByClassName('section-title')[0];
    title.innerHTML = '';
    if (opt_ancestry) {
      for (var i = opt_ancestry.length - 1; i >= 0; i--) {
        var titleCrumb = createBookmarkTitleCrumb_(opt_ancestry[i]);
        title.appendChild(titleCrumb);
        title.appendChild(createDiv('bookmark-separator'));
      }
    }
    var titleCrumb = createBookmarkTitleCrumb_(data);
    titleCrumb.classList.add('title-crumb-active');
    title.appendChild(titleCrumb);

    // Ensure the last crumb is as visible as possible.
    var windowWidth =
        wrapperEl.getElementsByClassName('section-title-mask')[0].offsetWidth;
    var crumbWidth = titleCrumb.offsetWidth;
    var leftOffset = titleCrumb.offsetLeft;

    var shiftLeft = windowWidth - crumbWidth - leftOffset;
    if (shiftLeft < 0) {
      if (crumbWidth > windowWidth)
        shifLeft = -leftOffset;

      // Queue up the scrolling initially to allow for the mask element to
      // be placed into the dom and it's size correctly calculated.
      setTimeout(function() {
        handleTitleScroll(wrapperEl, shiftLeft);
      }, 0);
    } else {
      handleTitleScroll(wrapperEl, 0);
    }
  }

  /**
   * Creates a clickable bookmark title crumb.
   * @param {Object} data The crumb data (see setBookmarkTitleHierarchy for
   *     definition of the data object).
   * @return {Element} The clickable title crumb element.
   * @private
   */
  function createBookmarkTitleCrumb_(data) {
    var titleCrumb = createDiv('title-crumb');
    if (data.root) {
      titleCrumb.innerText = templateData.bookmarkstitle;
    } else {
      titleCrumb.innerText = data.title;
    }
    titleCrumb.addEventListener('click', function(evt) {
      browseToBookmarkFolder(data.root ? '0' : data.id);
    });
    return titleCrumb;
  }

  /**
   * Handles scrolling a title element.
   * @param {Element} wrapperEl The wrapper element containing the scrollable
   *     title.
   * @param {number} scrollPosition The position to be scrolled to.
   */
  function handleTitleScroll(wrapperEl, scrollPosition) {
    var overflowLeftMask =
        wrapperEl.getElementsByClassName('overflow-left-mask')[0];
    var overflowRightMask =
        wrapperEl.getElementsByClassName('overflow-right-mask')[0];
    var title = wrapperEl.getElementsByClassName('section-title')[0];
    var titleMask = wrapperEl.getElementsByClassName('section-title-mask')[0];
    var titleWidth = title.scrollWidth;
    var containerWidth = titleMask.offsetWidth;

    var maxRightScroll = containerWidth - titleWidth;
    var boundedScrollPosition =
        Math.max(maxRightScroll, Math.min(scrollPosition, 0));

    overflowLeftMask.style.opacity =
        Math.min(
            1,
            (Math.max(0, -boundedScrollPosition)) + 10 / 30);

    overflowRightMask.style.opacity =
        Math.min(
            1,
            (Math.max(0, boundedScrollPosition - maxRightScroll) + 10) / 30);

    // Set the position of the title.
    if (titleWidth < containerWidth) {
      title.style.left = '0px';
    } else {
      title.style.left = boundedScrollPosition + 'px';
    }
  }

  /**
   * Initializes a scrolling title element.
   * @param {Element} wrapperEl The wrapper element of the scrolling title.
   */
  function initializeTitleScroller(wrapperEl) {
    var title = wrapperEl.getElementsByClassName('section-title')[0];

    var inTitleScroll = false;
    var startingScrollPosition;
    var startingOffset;
    wrapperEl.addEventListener(PRESS_START_EVT, function(evt) {
      inTitleScroll = true;
      startingScrollPosition = getTouchEventX(evt);
      startingOffset = title.offsetLeft;
    });
    document.body.addEventListener(PRESS_STOP_EVT, function(evt) {
      if (!inTitleScroll)
        return;
      inTitleScroll = false;
    });
    document.body.addEventListener(PRESS_MOVE_EVT, function(evt) {
      if (!inTitleScroll)
        return;
      handleTitleScroll(
          wrapperEl,
          startingOffset - (startingScrollPosition - getTouchEventX(evt)));
      evt.stopPropagation();
    });
  }

  /**
   * Handles updates from the underlying bookmark model (calls originate
   * in the WebUI handler for bookmarks).
   *
   * @param {Object} status Describes the type of change that occurred.  Can
   *     contain the following fields:
   *     - parent_id {string}: Unique id of the parent that was affected by
   *                           the change.  If the parent is the bookmark
   *                           bar, then the ID will be 'root'.
   *     - node_id {string}: The unique ID of the node that was affected.
   */
  function bookmarkChanged(status) {
    if (status) {
      var affectedParentNode = status['parent_id'];
      var affectedNodeId = status['node_id'];
      var shouldUpdate = (bookmarkFolderId == affectedParentNode ||
          bookmarkFolderId == affectedNodeId);
      if (shouldUpdate)
        setCurrentBookmarkFolderData(bookmarkFolderId);
    } else {
      // This typically happens when extensive changes could have happened to
      // the model, such as initial load, import and sync.
      setCurrentBookmarkFolderData(bookmarkFolderId);
    }
  }

  /**
   * Loads the bookarks data for a given folder.
   *
   * @param {string|number} folderId The ID of the folder to load (or null if
   *     it should load the root folder).
   */
  function setCurrentBookmarkFolderData(folderId) {
    if (folderId != null) {
      chrome.send('getBookmarks', [folderId]);
    } else {
      chrome.send('getBookmarks');
    }
    try {
      if (folderId == null) {
        localStorage.removeItem(DEFAULT_BOOKMARK_FOLDER_KEY);
      } else {
        localStorage.setItem(DEFAULT_BOOKMARK_FOLDER_KEY, folderId);
      }
    } catch (e) {}
  }

  /**
   * Navigates to the specified folder and handles loading the required data.
   * Ensures the current folder can be navigated back to using the browser
   * controls.
   *
   * @param {string|number} folderId The ID of the folder to navigate to.
   */
  function browseToBookmarkFolder(folderId) {
    history.pushState(
        {folderId: folderId, selectedPaneIndex: currentPaneIndex},
        null, null);
    setCurrentBookmarkFolderData(folderId);
  }

  /**
   * Called to inform the page of the current sync status. If the state has
   * changed from disabled to enabled, it changes the current and default
   * bookmark section to the root directory.  This makes desktop bookmarks are
   * visible.
   */
  function setSyncEnabled(enabled) {
    try {
      if (syncEnabled != undefined && syncEnabled == enabled) {
        // The value didn't change
        return;
      }
      syncEnabled = enabled;

      if (enabled) {
        if (!localStorage.getItem(SYNC_ENABLED_KEY)) {
          localStorage.setItem(SYNC_ENABLED_KEY, 'true');
          setCurrentBookmarkFolderData('0');
        }
      } else {
        localStorage.removeItem(SYNC_ENABLED_KEY);
      }
      updatePromoVisibility();

      if (bookmarkData) {
        // Bookmark data can now be displayed (or needs to be refiltered)
        bookmarks(bookmarkData);
      }

      updateSyncEmptyState();
    } catch (e) {}
  }

  /**
   * Handles adding or removing the 'nothing to see here' text from the session
   * list depending on the state of snapshots and sessions.
   *
   * @param {boolean} Whether the call is occuring because of a schedule
   *     timeout.
   */
  function updateSyncEmptyState(timeout) {
    if (syncState == SyncState.DISPLAYING_LOADING && !timeout) {
      // Make sure 'Loading...' is displayed long enough
      return;
    }

    var openTabsList = findList('open_tabs');
    var snapshotsList = findList('snapshots');
    var syncPromo = $('sync_promo');
    var syncLoading = $('sync_loading');
    var syncEnableSync = $('sync_enable_sync');

    if (syncEnabled == undefined ||
        currentSnapshots == null ||
        currentSessions == null) {
      if (syncState == SyncState.INITIAL) {
        // Wait one second for sync data to come in before displaying loading
        // text.
        syncState = SyncState.WAITING_FOR_DATA;
        syncTimerId = setTimeout(function() { updateSyncEmptyState(true); },
            SYNC_INITIAL_LOAD_TIMEOUT);
      } else if (syncState == SyncState.WAITING_FOR_DATA && timeout) {
        // We've waited for the initial info timeout to pass and still don't
        // have data.  So, display loading text so the user knows something is
        // happening.
        syncState = SyncState.DISPLAYING_LOADING;
        syncLoading.style.display = '-webkit-box';
        centerEmptySections(syncLoading);
        syncTimerId = setTimeout(function() { updateSyncEmptyState(true); },
            SYNC_LOADING_TIMEOUT);
      } else if (syncState == SyncState.DISPLAYING_LOADING) {
        // Allow the Loading... text to go away once data comes in
        syncState = SyncState.DISPLAYED_LOADING;
      }
      return;
    }

    if (syncTimerId != -1) {
      clearTimeout(syncTimerId);
      syncTimerId = -1;
    }
    syncState = SyncState.LOADED;

    // Hide everything by default, display selectively below
    syncEnableSync.style.display = 'none';
    syncLoading.style.display = 'none';
    syncPromo.style.display = 'none';

    var snapshotsCount =
        currentSnapshots == null ? 0 : currentSnapshots.length;
    var sessionsCount = currentSessions == null ? 0 : currentSessions.length;

    if (!syncEnabled) {
      syncEnableSync.style.display = '-webkit-box';
      centerEmptySections(syncEnableSync);
    } else if (sessionsCount + snapshotsCount == 0) {
      syncPromo.style.display = '-webkit-box';
      centerEmptySections(syncPromo);
    } else {
      openTabsList.style.display = sessionsCount == 0 ? 'none' : 'block';
      snapshotsList.style.display = snapshotsCount == 0 ? 'none' : 'block';
    }
    updatePromoVisibility();
  }

  /**
   * Called externally when updated snapshot data is available.
   *
   * @param {Object} data The snapshot data
   */
  function snapshots(data) {
    var list = findList('snapshots');
    list.innerHTML = '';

    currentSnapshots = data;
    updateSyncEmptyState();

    if (!data || data.length == 0)
      return;

    data.sort(function(a, b) {
      return b.createTime - a.createTime;
    });

    // Create the main container
    var snapshotsEl = createElement('div');
    list.appendChild(snapshotsEl);

    // Create the header container
    var headerEl = createDiv('session-header');
    snapshotsEl.appendChild(headerEl);

    // Create the documents container
    var docsEl = createDiv('session-children-container');
    snapshotsEl.appendChild(docsEl);

    // Create the container for the title & icon
    var headerInnerEl = createDiv('list-item standard-divider');
    addActiveTouchListener(headerInnerEl, ACTIVE_LIST_ITEM_CSS_CLASS);
    headerEl.appendChild(headerInnerEl);

    // Create the header icon
    headerInnerEl.appendChild(createDiv('session-icon documents'));

    // Create the header title
    var titleContainer = createElement('span', 'title');
    headerInnerEl.appendChild(titleContainer);
    var title = createDiv('session-name');
    title.textContent = templateData.receivedDocuments;
    titleContainer.appendChild(title);

    // Add support for expanding and collapsing the children
    var expando = createDiv();
    var expandoFunction = createExpandoFunction(expando, docsEl);
    headerInnerEl.addEventListener('click', expandoFunction);
    headerEl.appendChild(expando);

    // Support for actually opening the document
    var snapshotClickCallback = function(item) {
      if (!item)
        return;
      if (item.snapshotId) {
        window.location = 'chrome://snapshot/' + item.snapshotId;
      } else if (item.printJobId) {
        window.location = 'chrome://printjob/' + item.printJobId;
      } else {
        window.location = item.url;
      }
    }

    // Finally, add the list of documents
    populateData(docsEl, SectionType.SNAPSHOTS, data,
        makeListEntryItem, snapshotClickCallback);
  }

  /**
   * Create a function to handle expanding and collapsing a section
   *
   * @param {Element} expando The expando div
   * @param {Element} element The element to expand and collapse
   * @return {function()} A callback function that should be invoked when the
   *     expando is clicked
   */
  function createExpandoFunction(expando, element) {
    expando.className = 'expando open';
    return function() {
      if (element.style.height != '0px') {
        // It seems that '-webkit-transition' only works when explicit pixel
        // values are used.
        setTimeout(function() {
          // If this is the first time to collapse the list, store off the
          // expanded height and also set the height explicitly on the style.
          if (!element.expandedHeight) {
            element.expandedHeight =
                element.clientHeight + 'px';
            element.style.height = element.expandedHeight;
          }
          // Now set the height to 0.  Note, this is also done in a callback to
          // give the layout engine a chance to run after possibly setting the
          // height above.
          setTimeout(function() {
            element.style.height = '0px';
          }, 0);
        }, 0);
        expando.className = 'expando closed';
      } else {
        element.style.height = element.expandedHeight;
        expando.className = 'expando open';
      }
    }
  }

  /**
   * Initializes the promo_vc_list div to look like a foreign session
   * with a desktop.
   */
  function createPromoVirtualComputers() {
    var list = findList('promo_vc');
    list.innerHTML = '';

    // Set up the container and the "virtual computer" session header.
    var sessionEl = createDiv();
    list.appendChild(sessionEl);
    var sessionHeader = createDiv('session-header');
    sessionEl.appendChild(sessionHeader);

    // Set up the session children container and the promo as a child.
    var sessionChildren = createDiv('session-children-container');
    var promoMessage = createDiv('promo-message');
    promoMessage.id = 'promo_message_on_open_tabs';
    sessionChildren.appendChild(promoMessage);
    sessionEl.appendChild(sessionChildren);

    // Add support for expanding and collapsing the children.
    var expando = createDiv();
    var expandoFunction = createExpandoFunction(expando, sessionChildren);

    // Fill-in the contents of the "virtual computer" session header.
    var headerList = [{
      'title': promoInjectedComputerTitleText,
      'titleId': 'promo_vc_title',
      'userVisibleTimestamp': promoInjectedComputerLastSyncedText,
      'userVisibleTimestampId': 'promo_vc_lastsync',
      'iconStyle': 'laptop'
    }];

    populateData(sessionHeader, SectionType.PROMO_VC_SESSION_HEADER, headerList,
        makeForeignSessionListEntry, expandoFunction);
    sessionHeader.appendChild(expando);
  }

  /**
   * Called externally when updated synced sessions data is available.
   *
   * @param {Object} data The snapshot data
   */
  function setForeignSessions(data, tabSyncEnabled) {
    var list = findList('open_tabs');
    list.innerHTML = '';

    currentSessions = data;
    updateSyncEmptyState();

    // Sort the windows within each client such that more recently
    // modified windows appear first.
    data.forEach(function(client) {
      if (client.windows != null) {
        client.windows.sort(function(a, b) {
          if (b.timestamp == null) {
            return -1;
          } else if (a.timestamp == null) {
            return 1;
          } else {
            return b.timestamp - a.timestamp;
          }
        });
      }
    });

    // Sort so more recently modified clients appear first.
    data.sort(function(aClient, bClient) {
      var aWindows = aClient.windows;
      var bWindows = bClient.windows;
      if (bWindows == null || bWindows.length == 0 ||
          bWindows[0].timestamp == null) {
        return -1;
      } else if (aWindows == null || aWindows.length == 0 ||
          aWindows[0].timestamp == null) {
        return 1;
      } else {
        return bWindows[0].timestamp - aWindows[0].timestamp;
      }
    });

    data.forEach(function(client, clientNum) {

      var windows = client.windows;
      if (windows == null || windows.length == 0)
        return;

      // Set up the container for the session header
      var sessionEl = createElement('div');
      list.appendChild(sessionEl);
      var sessionHeader = createDiv('session-header');
      sessionEl.appendChild(sessionHeader);

      // Set up the container for the session children
      var sessionChildren = createDiv('session-children-container');
      sessionEl.appendChild(sessionChildren);

      var clientName = 'Client ' + clientNum;
      if (client.name)
        clientName = client.name;

      var iconStyle;
      var deviceType = client.deviceType;
      if (deviceType == 'win' ||
          deviceType == 'macosx' ||
          deviceType == 'linux' ||
          deviceType == 'chromeos' ||
          deviceType == 'other') {
        iconStyle = 'laptop';
      } else if (deviceType == 'phone') {
        iconStyle = 'phone';
      } else if (deviceType == 'tablet') {
        iconStyle = 'tablet';
      } else {
        console.error('Unknown sync device type found: ', deviceType);
        iconStyle = 'laptop';
      }
      var headerList = [{
        'title': clientName,
        'userVisibleTimestamp': windows[0].userVisibleTimestamp,
        'iconStyle': iconStyle,
        'sessionTag': client.tag,
      }];

      var expando = createDiv();
      var expandoFunction = createExpandoFunction(expando, sessionChildren);
      populateData(sessionHeader, SectionType.FOREIGN_SESSION_HEADER,
          headerList, makeForeignSessionListEntry, expandoFunction);
      sessionHeader.appendChild(expando);

      // Populate the session children container
      var openTabsList = new Array();
      for (var winNum = 0; winNum < windows.length; winNum++) {
        win = windows[winNum];
        var tabs = win.tabs;
        for (var tabNum = 0; tabNum < tabs.length; tabNum++) {
          var tab = tabs[tabNum];
          // If this is the last tab in the window and there are more windows,
          // use a section divider.
          var needSectionDivider =
              (tabNum + 1 == tabs.length) && (winNum + 1 < windows.length);
          openTabsList.push({
            timestamp: tab.timestamp,
            title: tab.title,
            url: tab.url,
            sessionTag: client.tag,
            winNum: winNum,
            sessionId: tab.sessionId,
            icon: tab.icon,
            iconSize: 32,
            divider: needSectionDivider ? 'section' : 'standard',
          });
        }
      }
      var tabCallback = function(item, evt) {
        var buttonIndex = 0;
        var altKeyPressed = false;
        var ctrlKeyPressed = false;
        var metaKeyPressed = false;
        var shiftKeyPressed = false;
        if (evt instanceof MouseEvent) {
          buttonIndex = evt.button;
          altKeyPressed = evt.altKey;
          ctrlKeyPressed = evt.ctrlKey;
          metaKeyPressed = evt.metaKey;
          shiftKeyPressed = evt.shiftKey;
        }
        chrome.send('metricsHandler:recordAction', ['MobileNTPForeignSession']);
        chrome.send('openForeignSession', [String(item.sessionTag),
            String(item.winNum), String(item.sessionId), buttonIndex,
            altKeyPressed, ctrlKeyPressed, metaKeyPressed, shiftKeyPressed]);
      };
      populateData(sessionChildren, SectionType.FOREIGN_SESSION, openTabsList,
          makeListEntryItem, tabCallback);
    });
  }

  /**
   * Updates the dominant favicon color for a given index.
   *
   * @param {number} index The index of the favicon whose dominant color is
   *     being specified.
   * @param {string} color The string encoded color.
   */
  function setFaviconDominantColor(index, color) {
    var colorstrips = document.getElementsByClassName('colorstrip-' + index);
    for (var i = 0; i < colorstrips.length; i++)
      colorstrips[i].style.background = color;

    var id = 'fold_' + index;
    var fold = $(id);
    if (!fold)
      return;
    var zoom = window.getComputedStyle(fold).zoom;
    var scale = 1 / window.getComputedStyle(fold).zoom;

    // The width/height of the canvas.  Set to 24 so it looks good across all
    // resolutions.
    var cw = 24;
    var ch = 24;

    // Get the fold canvas and create a path for the fold shape
    var ctx = document.getCSSCanvasContext(
        '2d', 'fold_' + index, cw * scale, ch * scale);
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(0, ch * 0.75 * scale);
    ctx.quadraticCurveTo(
        0, ch * scale,
        cw * .25 * scale, ch * scale);
    ctx.lineTo(cw * scale, ch * scale);
    ctx.closePath();

    // Create a gradient for the fold and fill it
    var gradient = ctx.createLinearGradient(cw * scale, 0, 0, ch * scale);
    if (color.indexOf('#') == 0) {
      var r = parseInt(color.substring(1, 3), 16);
      var g = parseInt(color.substring(3, 5), 16);
      var b = parseInt(color.substring(5, 7), 16);
      gradient.addColorStop(0, 'rgba(' + r + ', ' + g + ', ' + b + ', 0.6)');
    } else {
      // assume the color is in the 'rgb(#, #, #)' format
      var rgbBase = color.substring(4, color.length - 1);
      gradient.addColorStop(0, 'rgba(' + rgbBase + ', 0.6)');
    }
    gradient.addColorStop(1, color);
    ctx.fillStyle = gradient;
    ctx.fill();

    // Stroke the fold
    ctx.lineWidth = Math.floor(scale);
    ctx.strokeStyle = color;
    ctx.stroke();
    ctx.strokeStyle = 'rgba(0, 0, 0, 0.1)';
    ctx.stroke();

  }

  /**
   * Finds the list element corresponding to the given name.
   * @param {string} name The name prefix of the DOM element (<prefix>_list).
   * @return {Element} The list element corresponding with the name.
   */
  function findList(name) {
    return $(name + '_list');
  }

  /**
   * Render the given data into the given list, and hide or show the entire
   * container based on whether there are any elements.  The decorator function
   * is used to create the element to be inserted based on the given data
   * object.
   *
   * @param {holder} The dom element that the generated list items will be put
   *     into.
   * @param {SectionType} section The section that data is for.
   * @param {Object} data The data to be populated.
   * @param {function(Object, boolean)} decorator The function that will
   *     handle decorating each item in the data.
   * @param {function(Object, Object)} opt_clickCallback The function that is
   *     called when the item is clicked.
   */
  function populateData(holder, section, data, decorator,
      opt_clickCallback) {
    // Empty other items in the list, if present.
    holder.innerHTML = '';
    var fragment = document.createDocumentFragment();
    if (!data || data.length == 0) {
      fragment.innerHTML = '';
    } else {
      data.forEach(function(item) {
        var el = decorator(item, opt_clickCallback);
        el.setAttribute(SECTION_KEY, section);
        el.id = section + fragment.childNodes.length;
        fragment.appendChild(el);
      });
    }
    holder.appendChild(fragment);
    if (holder.classList.contains(GRID_CSS_CLASS))
      centerGrid(holder);
    centerEmptySections(holder);
  }

  /**
   * Given an element containing a list of child nodes arranged in
   * a grid, this will center the grid in the window based on the
   * remaining space.
   * @param {Element} el Container holding the grid cell items.
   */
  function centerGrid(el) {
    var childEl = el.firstChild;
    if (!childEl)
      return;

    // Find the element to actually set the margins on.
    var toCenter = el;
    var curEl = toCenter;
    while (curEl && curEl.classList) {
      if (curEl.classList.contains(GRID_CENTER_CSS_CLASS)) {
        toCenter = curEl;
        break;
      }
      curEl = curEl.parentNode;
    }
    var setItemMargins = el.classList.contains(GRID_SET_ITEM_MARGINS);
    var itemWidth = getItemWidth(childEl, setItemMargins);
    var windowWidth = document.documentElement.offsetWidth;
    if (itemWidth >= windowWidth) {
      toCenter.style.paddingLeft = '0';
      toCenter.style.paddingRight = '0';
    } else {
      var numColumns = el.getAttribute(GRID_COLUMNS);
      if (numColumns) {
        numColumns = parseInt(numColumns);
      } else {
        numColumns = Math.floor(windowWidth / itemWidth);
      }

      if (setItemMargins) {
        // In this case, try to size each item to fill as much space as
        // possible.
        var gutterSize =
            (windowWidth - itemWidth * numColumns) / (numColumns + 1);
        var childLeftMargin = Math.round(gutterSize / 2);
        var childRightMargin = Math.floor(gutterSize - childLeftMargin);
        var children = el.childNodes;
        for (var i = 0; i < children.length; i++) {
          children[i].style.marginLeft = childLeftMargin + 'px';
          children[i].style.marginRight = childRightMargin + 'px';
        }
        itemWidth += childLeftMargin + childRightMargin;
      }

      var remainder = windowWidth - itemWidth * numColumns;
      var leftPadding = Math.round(remainder / 2);
      var rightPadding = Math.floor(remainder - leftPadding);
      toCenter.style.paddingLeft = leftPadding + 'px';
      toCenter.style.paddingRight = rightPadding + 'px';

      if (toCenter.classList.contains(GRID_SET_TOP_MARGIN_CLASS)) {
        var childStyle = window.getComputedStyle(childEl);
        var childLeftPadding = parseInt(
            childStyle.getPropertyValue('padding-left'));
        toCenter.style.paddingTop =
            (childLeftMargin + childLeftPadding + leftPadding) + 'px';
      }
    }
  }

  /**
   * Finds and centers all child grid elements for a given node (the grids
   * do not need to be direct descendants and can reside anywhere in the node
   * hierarchy).
   * @param {Element} el The node containing the grid child nodes.
   */
  function centerChildGrids(el) {
    var grids = el.getElementsByClassName(GRID_CSS_CLASS);
    for (var i = 0; i < grids.length; i++)
      centerGrid(grids[i]);
  }

  /**
   * Finds and vertically centers all 'empty' elements for a given node (the
   * 'empty' elements do not need to be direct descendants and can reside
   * anywhere in the node hierarchy).
   * @param {Element} el The node containing the 'empty' child nodes.
   */
  function centerEmptySections(el) {
    if (el.classList &&
        el.classList.contains(CENTER_EMPTY_CONTAINER_CSS_CLASS)) {
      centerEmptySection(el);
    }
    var empties = el.getElementsByClassName(CENTER_EMPTY_CONTAINER_CSS_CLASS);
    for (var i = 0; i < empties.length; i++) {
      centerEmptySection(empties[i]);
    }
  }

  /**
   * Set the top of the given element to the top of the parent and set the
   * height to (bottom of document - top).
   *
   * @param {Element} el Container holding the centered content.
   */
  function centerEmptySection(el) {
    var parent = el.parentNode;
    var top = parent.offsetTop;
    var bottom = (
        document.documentElement.offsetHeight - getButtonBarPadding());
    el.style.height = (bottom - top) + 'px';
    el.style.top = top + 'px';
  }

  /**
   * Finds the index of the panel specified by its prefix.
   * @param {string} The string prefix for the panel.
   * @return {number} The index of the panel.
   */
  function getPaneIndex(panePrefix) {
    var pane = $(panePrefix + '_container');

    if (pane != null) {
      var index = panes.indexOf(pane);

      if (index >= 0)
        return index;
    }
    return 0;
  }

  /**
   * Finds the index of the panel specified by location hash.
   * @return {number} The index of the panel.
   */
  function getPaneIndexFromHash() {
    var paneIndex;
    if (window.location.hash == '#bookmarks') {
      paneIndex = getPaneIndex('bookmarks');
    } else if (window.location.hash == '#bookmark_shortcut') {
      paneIndex = getPaneIndex('bookmarks');
    } else if (window.location.hash == '#most_visited') {
      paneIndex = getPaneIndex('most_visited');
    } else if (window.location.hash == '#open_tabs') {
      paneIndex = getPaneIndex('open_tabs');
    } else if (window.location.hash == '#incognito') {
      paneIndex = getPaneIndex('incognito');
    } else {
      // Couldn't find a good section
      paneIndex = -1;
    }
    return paneIndex;
  }

  /**
   * Selects a pane from the top level list (Most Visited, Bookmarks, etc...).
   * @param {number} paneIndex The index of the pane to be selected.
   * @return {boolean} Whether the selected pane has changed.
   */
  function scrollToPane(paneIndex) {
    var pane = panes[paneIndex];

    if (pane == currentPane)
      return false;

    var newHash = '#' + sectionPrefixes[paneIndex];
    // If updated hash matches the current one in the URL, we need to call
    // updatePaneOnHash directly as updating the hash to the same value will
    // not trigger the 'hashchange' event.
    if (bookmarkShortcutMode || newHash == document.location.hash)
      updatePaneOnHash();
    computeDynamicLayout();
    promoUpdateImpressions(sectionPrefixes[paneIndex]);
    return true;
  }

  /**
   * Updates the pane based on the current hash.
   */
  function updatePaneOnHash() {
    var paneIndex = getPaneIndexFromHash();
    var pane = panes[paneIndex];

    if (currentPane)
      currentPane.classList.remove('selected');
    pane.classList.add('selected');
    currentPane = pane;
    currentPaneIndex = paneIndex;

    document.body.scrollTop = 0;

    // TODO (dtrainor): Could potentially add logic to reset the bookmark state
    // if they are moving to that pane.  This logic was in there before, but
    // was removed due to the fact that we have to go to this pane as part of
    // the history navigation.
  }

  /**
   * Adds a top level section to the NTP.
   * @param {string} panelPrefix The prefix of the element IDs corresponding
   *     to the container of the content.
   * @param {boolean=} opt_canBeDefault Whether this section can be marked as
   *     the default starting point for subsequent instances of the NTP.  The
   *     default value for this is true.
   */
  function addMainSection(panelPrefix) {
    var paneEl = $(panelPrefix + '_container');
    var paneIndex = panes.push(paneEl) - 1;
    sectionPrefixes.push(panelPrefix);
  }

  /**
   * Handles the dynamic layout of the components on the new tab page.  Only
   * layouts that require calculation based on the screen size should go in
   * this function as it will be called during all resize changes
   * (orientation, keyword being displayed).
   */
  function computeDynamicLayout() {
    // Update the scrolling titles to ensure they are not in a now invalid
    // scroll position.
    var titleScrollers =
        document.getElementsByClassName('section-title-wrapper');
    for (var i = 0, len = titleScrollers.length; i < len; i++) {
      var titleEl =
          titleScrollers[i].getElementsByClassName('section-title')[0];
      handleTitleScroll(
          titleScrollers[i],
          titleEl.offsetLeft);
    }

    updateMostVisitedStyle();
    updateMostVisitedHeight();
  }

  /**
   * The centering of the 'recently closed' section is different depending on
   * the orientation of the device.  In landscape, it should be left-aligned
   * with the 'most used' section.  In portrait, it should be centered in the
   * screen.
   */
  function updateMostVisitedStyle() {
    if (isTablet()) {
      updateMostVisitedStyleTablet();
    } else {
      updateMostVisitedStylePhone();
    }
  }

  /**
   * Updates the style of the most visited pane for the phone.
   */
  function updateMostVisitedStylePhone() {
    var mostVisitedList = $('most_visited_list');
    var childEl = mostVisitedList.firstChild;
    if (!childEl)
      return;

    // 'natural' height and width of the thumbnail
    var thumbHeight = 72;
    var thumbWidth = 108;
    var labelHeight = 20;
    var labelWidth = thumbWidth + 20;
    var labelLeft = (thumbWidth - labelWidth) / 2;
    var itemHeight = thumbHeight + labelHeight;

    // default vertical margin between items
    var itemMarginTop = 0;
    var itemMarginBottom = 0;
    var itemMarginLeft = 20;
    var itemMarginRight = 20;

    var listHeight = 0;
    // set it to the unscaled size so centerGrid works correctly
    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'width', thumbWidth + 'px');

    var screenHeight =
        document.documentElement.offsetHeight -
        getButtonBarPadding();

    if (isPortrait()) {
      mostVisitedList.setAttribute(GRID_COLUMNS, '2');
      listHeight = screenHeight * .85;
      listHeight = listHeight >= 420 ? 420 : listHeight;
      // Size for 3 rows (4 gutters)
      itemMarginTop = (listHeight - (itemHeight * 3)) / 4;
    } else {
      mostVisitedList.setAttribute(GRID_COLUMNS, '3');
      listHeight = screenHeight;

      // If the screen height is less than targetHeight, scale the size of the
      // thumbnails such that the margin between the thumbnails remains
      // constant.
      var targetHeight = 220;
      if (screenHeight < targetHeight) {
        var targetRemainder = targetHeight - 2 * (thumbHeight + labelHeight);
        var scale = (screenHeight - 2 * labelHeight -
            targetRemainder) / (2 * thumbHeight);
        // update values based on scale
        thumbWidth *= scale;
        thumbHeight *= scale;
        labelWidth = thumbWidth + 20;
        itemHeight = thumbHeight + labelHeight;
      }

      // scale the vertical margin such that the items fit perfectly on the
      // screen
      var remainder = screenHeight - (2 * itemHeight);
      var margin = (remainder / 2);
      margin = margin > 24 ? 24 : margin;
      itemMarginTop = Math.round(margin / 2);
      itemMarginBottom = Math.round(margin - itemMarginTop);
    }

    mostVisitedList.style.minHeight = listHeight + 'px';

    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'height', itemHeight + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .thumbnail',
        'height', thumbHeight + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .thumbnail',
        'width', thumbWidth + 'px');
    modifyCssRule(
        'body[device="phone"] #most_visited_list .thumbnail-container',
        'height', thumbHeight + 'px');
    modifyCssRule(
        'body[device="phone"] #most_visited_list .thumbnail-container',
        'width', thumbWidth + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .title',
        'width', labelWidth + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .title',
        'left', labelLeft + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .inner-border',
        'height', thumbHeight - 2 + 'px');
    modifyCssRule('body[device="phone"] #most_visited_list .inner-border',
        'width', thumbWidth - 2 + 'px');

    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'margin-left', itemMarginLeft + 'px');
    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'margin-right', itemMarginRight + 'px');
    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'margin-top', itemMarginTop + 'px');
    modifyCssRule('body[device="phone"] .thumbnail-cell',
        'margin-bottom', itemMarginBottom + 'px');

    centerChildGrids($('most_visited_container'));
  }

  /**
   * Updates the style of the most visited pane for the tablet.
   */
  function updateMostVisitedStyleTablet() {
    function setCenterIconGrid(el, set) {
      if (set) {
        el.classList.add(GRID_CENTER_CSS_CLASS);
      } else {
        el.classList.remove(GRID_CENTER_CSS_CLASS);
        el.style.paddingLeft = '0px';
        el.style.paddingRight = '0px';
      }
    }
    var isPortrait = document.documentElement.offsetWidth <
        document.documentElement.offsetHeight;
    var mostVisitedContainer = $('most_visited_container');
    var mostVisitedList = $('most_visited_list');
    var recentlyClosedContainer = $('recently_closed_container');
    var recentlyClosedList = $('recently_closed_list');

    setCenterIconGrid(mostVisitedContainer, !isPortrait);
    setCenterIconGrid(mostVisitedList, isPortrait);
    setCenterIconGrid(recentlyClosedContainer, isPortrait);
    if (isPortrait) {
      recentlyClosedList.classList.add(GRID_CSS_CLASS);
    } else {
      recentlyClosedList.classList.remove(GRID_CSS_CLASS);
    }

    // Make the recently closed list visually left align with the most recently
    // closed items in landscape mode.  It will be reset by the grid centering
    // in portrait mode.
    if (!isPortrait)
      recentlyClosedContainer.style.paddingLeft = '14px';
  }

  /**
   * This handles updating some of the spacing to make the 'recently closed'
   * section appear at the bottom of the page.
   */
  function updateMostVisitedHeight() {
    if (!isTablet())
      return;
    // subtract away height of button bar
    var windowHeight = document.documentElement.offsetHeight;
    var padding = parseInt(window.getComputedStyle(document.body)
        .getPropertyValue('padding-bottom'));
    $('most_visited_container').style.minHeight =
        (windowHeight - padding) + 'px';
  }

  /**
   * Called by the native toolbar to open a different section. This handles
   * updating the hash url which in turns makes a history entry.
   *
   * @param {string} section The section to switch to.
   */
  var openSection = function(section) {
    if (!scrollToPane(getPaneIndex(section)))
      return;
    // Update the url so the native toolbar knows the pane has changed and
    // to create a history entry.
    document.location.hash = '#' + section;
  }

  /////////////////////////////////////////////////////////////////////////////
  // NTP Scoped Window Event Listeners.
  /////////////////////////////////////////////////////////////////////////////

  /**
   * Handles history on pop state changes.
   */
  function onPopStateHandler(event) {
    if (event.state != null) {
      var evtState = event.state;
      // Navigate back to the previously selected panel and ensure the same
      // bookmarks are loaded.
      var selectedPaneIndex = evtState.selectedPaneIndex == undefined ?
          0 : evtState.selectedPaneIndex;

      scrollToPane(selectedPaneIndex);
      setCurrentBookmarkFolderData(evtState.folderId);
    } else {
      // When loading the page, replace the default state with one that
      // specifies the default panel loaded via localStorage as well as the
      // default bookmark folder.
      history.replaceState(
          {folderId: bookmarkFolderId, selectedPaneIndex: currentPaneIndex},
          null, null);
    }
  }

  /**
   * Handles window resize events.
   */
  function windowResizeHandler() {
    // Scroll to the current pane to refactor all the margins and offset.
    scrollToPane(currentPaneIndex);
    computeDynamicLayout();
    // Center the padding for each of the grid views.
    centerChildGrids(document);
    centerEmptySections(document);
  }

  /*
   * We implement the context menu ourselves.
   */
  function contextMenuHandler(evt) {
    var section = SectionType.UNKNOWN;
    contextMenuUrl = null;
    contextMenuItem = null;
    // The node with a menu have been tagged with their section and url.
    // Let's find these tags.
    var node = evt.target;
    while (node) {
      if (section == SectionType.UNKNOWN &&
          node.getAttribute &&
          node.getAttribute(SECTION_KEY) != null) {
        section = node.getAttribute(SECTION_KEY);
        if (contextMenuUrl != null)
          break;
      }
      if (contextMenuUrl == null) {
        contextMenuUrl = node.getAttribute(CONTEXT_MENU_URL_KEY);
        contextMenuItem = node.contextMenuItem;
        if (section != SectionType.UNKNOWN)
          break;
      }
      node = node.parentNode;
    }

    var menuOptions;

    if (section == SectionType.BOOKMARKS &&
        !contextMenuItem.folder && !isIncognito) {
      menuOptions = [
        [
          ContextMenuItemIds.BOOKMARK_OPEN_IN_NEW_TAB,
          templateData.elementopeninnewtab
        ],
        [
          ContextMenuItemIds.BOOKMARK_OPEN_IN_INCOGNITO_TAB,
          templateData.elementopeninincognitotab
        ]
      ];
      if (contextMenuItem.editable) {
        menuOptions.push(
            [ContextMenuItemIds.BOOKMARK_EDIT, templateData.bookmarkedit],
            [ContextMenuItemIds.BOOKMARK_DELETE, templateData.bookmarkdelete]);
      }
      if (contextMenuUrl.search('chrome://') == -1 &&
          contextMenuUrl.search('about://') == -1) {
        menuOptions.push([
          ContextMenuItemIds.BOOKMARK_SHORTCUT,
          templateData.bookmarkshortcut
        ]);
      }
    } else if (section == SectionType.BOOKMARKS &&
               !contextMenuItem.folder &&
               isIncognito) {
      menuOptions = [
        [
          ContextMenuItemIds.BOOKMARK_OPEN_IN_INCOGNITO_TAB,
          templateData.elementopeninincognitotab
        ]
      ];
    } else if (section == SectionType.BOOKMARKS &&
               contextMenuItem.folder &&
               contextMenuItem.editable &&
               !isIncognito) {
      menuOptions = [
        [ContextMenuItemIds.BOOKMARK_EDIT, templateData.editfolder],
        [ContextMenuItemIds.BOOKMARK_DELETE, templateData.deletefolder]
      ];
    } else if (section == SectionType.MOST_VISITED) {
      menuOptions = [
        [
          ContextMenuItemIds.MOST_VISITED_OPEN_IN_NEW_TAB,
          templateData.elementopeninnewtab
        ],
        [
          ContextMenuItemIds.MOST_VISITED_OPEN_IN_INCOGNITO_TAB,
          templateData.elementopeninincognitotab
        ],
        [ContextMenuItemIds.MOST_VISITED_REMOVE, templateData.elementremove]
      ];
    } else if (section == SectionType.RECENTLY_CLOSED) {
      menuOptions = [
        [
          ContextMenuItemIds.RECENTLY_CLOSED_OPEN_IN_NEW_TAB,
          templateData.elementopeninnewtab
        ],
        [
          ContextMenuItemIds.RECENTLY_CLOSED_OPEN_IN_INCOGNITO_TAB,
          templateData.elementopeninincognitotab
        ],
        [
          ContextMenuItemIds.RECENTLY_CLOSED_REMOVE,
          templateData.removeall
        ]
      ];
    } else if (section == SectionType.FOREIGN_SESSION_HEADER) {
      menuOptions = [
        [
          ContextMenuItemIds.FOREIGN_SESSIONS_REMOVE,
          templateData.elementremove
        ]
      ];
    } else if (section == SectionType.PROMO_VC_SESSION_HEADER) {
      menuOptions = [
        [
          ContextMenuItemIds.PROMO_VC_SESSION_REMOVE,
          templateData.elementremove
        ]
      ];
    }

    if (menuOptions)
      chrome.send('showContextMenu', menuOptions);

    return false;
  }

  // Return an object with all the exports
  return {
    bookmarks: bookmarks,
    bookmarkChanged: bookmarkChanged,
    clearPromotions: clearPromotions,
    init: init,
    onCustomMenuSelected: onCustomMenuSelected,
    openSection: openSection,
    setFaviconDominantColor: setFaviconDominantColor,
    setForeignSessions: setForeignSessions,
    setIncognitoMode: setIncognitoMode,
    setMostVisitedPages: setMostVisitedPages,
    setPromotions: setPromotions,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setSyncEnabled: setSyncEnabled,
    snapshots: snapshots
  };
});

/////////////////////////////////////////////////////////////////////////////
//Utility Functions.
/////////////////////////////////////////////////////////////////////////////

/**
 * A best effort approach for checking simple data object equality.
 * @param {?} val1 The first value to check equality for.
 * @param {?} val2 The second value to check equality for.
 * @return {boolean} Whether the two objects are equal(ish).
 */
function equals(val1, val2) {
  if (typeof val1 != 'object' || typeof val2 != 'object')
    return val1 === val2;

  // Object and array equality checks.
  var keyCountVal1 = 0;
  for (var key in val1) {
    if (!(key in val2) || !equals(val1[key], val2[key]))
      return false;
    keyCountVal1++;
  }
  var keyCountVal2 = 0;
  for (var key in val2)
    keyCountVal2++;
  if (keyCountVal1 != keyCountVal2)
    return false;
  return true;
}

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * @return {boolean} Whether the device is currently in portrait mode.
 */
function isPortrait() {
  return document.documentElement.offsetWidth <
      document.documentElement.offsetHeight;
}

/**
 * Determine if the page should be formatted for tablets.
 * @return {boolean} true if the device is a tablet, false otherwise.
 */
function isTablet() {
  return document.body.getAttribute('device') == 'tablet';
}

/**
 * Determine if the page should be formatted for phones.
 * @return {boolean} true if the device is a phone, false otherwise.
 */
function isPhone() {
  return document.body.getAttribute('device') == 'phone';
}

/**
 * Get the page X coordinate of a touch event.
 * @param {TouchEvent} evt The touch event triggered by the browser.
 * @return {number} The page X coordinate of the touch event.
 */
function getTouchEventX(evt) {
  return (evt.touches[0] || e.changedTouches[0]).pageX;
}

/**
 * Get the page Y coordinate of a touch event.
 * @param {TouchEvent} evt The touch event triggered by the browser.
 * @return {number} The page Y coordinate of the touch event.
 */
function getTouchEventY(evt) {
  return (evt.touches[0] || e.changedTouches[0]).pageY;
}

/**
 * @param {Element} el The item to get the width of.
 * @param {boolean} excludeMargin If true, exclude the width of the margin.
 * @return {number} The total width of a given item.
 */
function getItemWidth(el, excludeMargin) {
  var elStyle = window.getComputedStyle(el);
  var width = el.offsetWidth;
  if (!width || width == 0) {
    width = parseInt(elStyle.getPropertyValue('width'));
    width +=
        parseInt(elStyle.getPropertyValue('border-left-width')) +
        parseInt(elStyle.getPropertyValue('border-right-width'));
    width +=
        parseInt(elStyle.getPropertyValue('padding-left')) +
        parseInt(elStyle.getPropertyValue('padding-right'));
  }
  if (!excludeMargin) {
    width += parseInt(elStyle.getPropertyValue('margin-left')) +
        parseInt(elStyle.getPropertyValue('margin-right'));
  }
  return width;
}

/**
 * @return {number} The padding height of the body due to the button bar
 */
function getButtonBarPadding() {
  var body = document.getElementsByTagName('body')[0];
  var style = window.getComputedStyle(body);
  return parseInt(style.getPropertyValue('padding-bottom'));
}

/**
 * Modify a css rule
 * @param {string} selector The selector for the rule (passed to findCssRule())
 * @param {string} property The property to update
 * @param {string} value The value to update the property to
 * @return {boolean} true if the rule was updated, false otherwise.
 */
function modifyCssRule(selector, property, value) {
  var rule = findCssRule(selector);
  if (!rule)
    return false;
  rule.style[property] = value;
  return true;
}

/**
 * Find a particular CSS rule.  The stylesheets attached to the document
 * are traversed in reverse order.  The rules in each stylesheet are also
 * traversed in reverse order.  The first rule found to match the selector
 * is returned.
 * @param {string} selector The selector for the rule.
 * @return {Object} The rule if one was found, null otherwise
 */
function findCssRule(selector) {
  var styleSheets = document.styleSheets;
  for (i = styleSheets.length - 1; i >= 0; i--) {
    var styleSheet = styleSheets[i];
    var rules = styleSheet.cssRules;
    if (rules == null)
      continue;
    for (j = rules.length - 1; j >= 0; j--) {
      if (rules[j].selectorText == selector)
        return rules[j];
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// NTP Entry point.
/////////////////////////////////////////////////////////////////////////////

/*
 * Handles initializing the UI when the page has finished loading.
 */
window.addEventListener('DOMContentLoaded', function(evt) {
  ntp.init();
  $('content-area').style.display = 'block';
});
