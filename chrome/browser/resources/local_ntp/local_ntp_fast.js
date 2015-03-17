// Copyright 2015 The Chromium Authors. All rights reserved.
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
  'use strict';

<include src="../../../../ui/webui/resources/js/util.js">
<include src='local_ntp_design.js'>

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo', // Shows white logo if required by theme
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  FAKEBOX_DISABLE: 'fakebox-disable', // Makes fakebox non-interactive
  FAKEBOX_FOCUS: 'fakebox-focused', // Applies focus styles to the fakebox
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  HIDE_FAKEBOX_AND_LOGO: 'hide-fakebox-logo',
  HIDE_NOTIFICATION: 'mv-notice-hide',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  RTL: 'rtl'  // Right-to-left language text.
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
 * The last blacklisted tile rid if any, which by definition should not be
 * filler.
 * @type {?number}
 */
var lastBlacklistedTile = null;


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
 * The color of the title in RRGGBBAA format.
 * @type {?string}
 */
var titleColor = null;


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
    if (configData.translatedStrings.searchboxPlaceholder) {
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
 * @param {Array<number>} color Array of rgba color components.
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
 * @param {Array<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 * @private
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
                    color[3] / 255 + ')';
}


/**
 * Called when page data change.
 */
function onMostVisitedChange() {
  reloadTiles();
}


/**
 * Fetches new data, creates, and renders tiles.
 */
function reloadTiles() {
  var pages = ntpApiHandle.mostVisited;
  var iframe = $('mv-single').contentWindow;

  tiles = [];
  for (var i = 0; i < Math.min(MAX_NUM_TILES_TO_SHOW, pages.length); ++i) {
    iframe.postMessage({cmd: 'tile', rid: pages[i].rid}, '*');
  }
  iframe.postMessage({cmd: 'show'}, '*');
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
  hideNotification();
  if (lastBlacklistedTile != null) {
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedTile);
  }
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
  $(IDS.TILES).style.width = tilesContainerWidth + 'px';
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
  updateContentWidth();
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
  var cmd = event.data.cmd;
  var args = event.data;
  if (cmd == 'tileBlacklisted') {
    showNotification();
    lastBlacklistedTile = args.rid;

    ntpApiHandle.deleteMostVisitedItem(args.rid);
  }
}


/**
 * Prepares the New Tab Page by adding listeners, rendering the current
 * theme, the most visited pages section, and Google-specific elements for a
 * Google-provided page.
 */
function init() {
  notification = $(IDS.NOTIFICATION);
  attribution = $(IDS.ATTRIBUTION);
  ntpContents = $(IDS.NTP_CONTENTS);

  if (configData.isGooglePage) {
    var logo = document.createElement('div');
    logo.id = IDS.LOGO;
    logo.title = 'Google';

    fakebox = document.createElement('div');
    fakebox.id = IDS.FAKEBOX;
    var fakeboxHtml = [];
    fakeboxHtml.push('<div id="' + IDS.FAKEBOX_TEXT + '"></div>');
    fakeboxHtml.push('<input id="' + IDS.FAKEBOX_INPUT +
        '" autocomplete="off" tabindex="-1" type="url" aria-hidden="true">');
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
        // Send pasted text to Omnibox.
        var text = event.clipboardData.getData('text/plain');
        if (text)
          searchboxApiHandle.paste(text);
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
    // Grabbing the root HTML element.
    document.documentElement.setAttribute('dir', 'rtl');
    // Add class for setting alignments based on language directionality.
    document.documentElement.classList.add(CLASSES.RTL);
  }

  var iframe = document.createElement('iframe');
  iframe.id = 'mv-single';
  var args = [];

  if (searchboxApiHandle.rtl) {
    args.push('rtl=1');
  }

  iframe.src = '//most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    reloadTiles();
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
  init: init,
  listen: listen
};
}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
