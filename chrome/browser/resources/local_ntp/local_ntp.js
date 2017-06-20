// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// NOTE: If you modify this file, you also have to change its hash in
// local_ntp.html and in LocalNtpSource::GetContentSecurityPolicyScriptSrc.
// To calculate the sum execute the following command
// sha256sum local_ntp.js | cut -d " " -f 1 | xxd -r -p | base64


/**
 * @fileoverview The local InstantExtended NTP.
 */


/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
'use strict';


/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}


/**
 * Specifications for an NTP design (not comprehensive).
 *
 * numTitleLines: Number of lines to display in titles.
 * tileWidth: The width of each suggestion tile, in px.
 * tileMargin: Spacing between successive tiles, in px.
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 *
 * @type {{
 *   numTitleLines: number,
 *   tileWidth: number,
 *   tileMargin: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 * }}
 */
var NTP_DESIGN = {
  numTitleLines: 1,
  tileWidth: 154,
  tileMargin: 16,
  titleColor: [50, 50, 50, 255],
  titleColorAgainstDark: [210, 210, 210, 255],
};


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
  FAKEBOX_FOCUS: 'fakebox-focused', // Applies focus styles to the fakebox
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  HIDE_FAKEBOX_AND_LOGO: 'hide-fakebox-logo',
  HIDE_NOTIFICATION: 'mv-notice-hide',
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
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
  TILES_IFRAME: 'mv-single',
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
  var info = ntpApiHandle.themeBackgroundInfo;
  var isThemeDark = getIsThemeDark(info);
  $(IDS.NTP_CONTENTS).classList.toggle(CLASSES.DARK, isThemeDark);
  if (!info) {
    return;
  }

  var background = [convertToRGBAColor(info.backgroundColorRgba),
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();

  document.body.style.background = background;
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, info.alternateLogo);
  updateThemeAttribution(info.attributionUrl, info.imageHorizontalAlignment);
  setCustomThemeStyle(info);

  // Inform the most visited iframe of the new theme.
  var themeinfo = {cmd: 'updateTheme'};
  themeinfo.tileBorderColor = convertToRGBAColor(info.sectionBorderColorRgba);
  themeinfo.tileHoverBorderColor = convertToRGBAColor(info.headerColorRgba);
  themeinfo.isThemeDark = isThemeDark;

  var titleColor = NTP_DESIGN.titleColor;
  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = info.textColorRgba;
  } else if (isThemeDark) {
    titleColor = NTP_DESIGN.titleColorAgainstDark;
  }
  themeinfo.tileTitleColor = convertToRGBAColor(titleColor);

  $(IDS.TILES_IFRAME).contentWindow.postMessage(themeinfo, '*');
}


/**
 * Callback for embeddedSearch.newTabPage.onthemechange.
 * @private
 */
function onThemeChange() {
  renderTheme();
}


/**
 * Updates the NTP style according to theme.
 * @param {Object} themeInfo The information about the theme.
 * @private
 */
function setCustomThemeStyle(themeInfo) {
  var customStyleElement = $(IDS.CUSTOM_THEME_STYLE);
  var head = document.head;
  if (!themeInfo.usingDefaultTheme) {
    $(IDS.NTP_CONTENTS).classList.remove(CLASSES.DEFAULT_THEME);
    var themeStyle =
      '#attribution {' +
      '  color: ' + convertToRGBAColor(themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-msg {' +
      '  color: ' + convertToRGBAColor(themeInfo.textColorRgba) + ';' +
      '}' +
      '#mv-notice-links span {' +
      '  color: ' + convertToRGBAColor(themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-notice-x {' +
      '  -webkit-filter: drop-shadow(0 0 0 ' +
          convertToRGBAColor(themeInfo.textColorRgba) + ');' +
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
    $(IDS.NTP_CONTENTS).classList.add(CLASSES.DEFAULT_THEME);
    if (customStyleElement)
      head.removeChild(customStyleElement);
  }
}


/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string} url The URL of the attribution image, if any.
 * @param {string} themeBackgroundAlignment The alignment of the theme
 *  background image. This is used to compute the attribution's alignment.
 * @private
 */
function updateThemeAttribution(url, themeBackgroundAlignment) {
  if (!url) {
    setAttributionVisibility_(false);
    return;
  }

  var attribution = $(IDS.ATTRIBUTION);
  var attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;

  // To avoid conflicts, place the attribution on the left for themes that
  // right align their background images.
  attribution.classList.toggle(CLASSES.LEFT_ALIGN_ATTRIBUTION,
                               themeBackgroundAlignment == 'right');
  setAttributionVisibility_(true);
}


/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 * @private
 */
function setAttributionVisibility_(show) {
  $(IDS.ATTRIBUTION).style.display = show ? '' : 'none';
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
 * Callback for embeddedSearch.newTabPage.onmostvisitedchange. Called when the
 * NTP tiles are updated.
 */
function onMostVisitedChange() {
  reloadTiles();
}


/**
 * Fetches new data (RIDs) from the embeddedSearch.newTabPage API and passes
 * them to the iframe.
 */
function reloadTiles() {
  var pages = ntpApiHandle.mostVisited;
  var cmds = [];
  for (var i = 0; i < Math.min(MAX_NUM_TILES_TO_SHOW, pages.length); ++i) {
    cmds.push({cmd: 'tile', rid: pages[i].rid});
  }
  cmds.push({cmd: 'show', maxVisible: numColumnsShown * NUM_ROWS});

  $(IDS.TILES_IFRAME).contentWindow.postMessage(cmds, '*');
}


/**
 * Shows the blacklist notification and triggers a delay to hide it.
 */
function showNotification() {
  var notification = $(IDS.NOTIFICATION);
  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
  notification.scrollTop;
  notification.classList.add(CLASSES.DELAYED_HIDE_NOTIFICATION);
}


/**
 * Hides the blacklist notification.
 */
function hideNotification() {
  var notification = $(IDS.NOTIFICATION);
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
  newNumColumns =
      Math.max(MIN_NUM_COLUMNS, Math.min(newNumColumns, MAX_NUM_COLUMNS));

  if (numColumnsShown === newNumColumns)
    return false;

  numColumnsShown = newNumColumns;
  // We add an extra pixel because rounding errors on different zooms can
  // make the width shorter than it should be.
  var tilesContainerWidth = Math.ceil(numColumnsShown * tileRequiredWidth) + 1;
  $(IDS.TILES).style.width = tilesContainerWidth + 'px';
  // -2 to account for border.
  var fakeboxWidth = (tilesContainerWidth - NTP_DESIGN.tileMargin - 2);
  $(IDS.FAKEBOX).style.width = fakeboxWidth + 'px';
  return true;
}


/**
 * Resizes elements because the number of tile columns may need to change in
 * response to resizing. Also shows or hides extra tiles tiles according to the
 * new width of the page.
 */
function onResize() {
  if (updateContentWidth()) {
    // If the number of tile columns changes, inform the iframe.
    $(IDS.TILES_IFRAME).contentWindow.postMessage(
        {cmd: 'tilesVisible', maxVisible: numColumnsShown * NUM_ROWS}, '*');
  }
}


/**
 * Callback for embeddedSearch.newTabPage.oninputstart. Handles new input by
 * disposing the NTP, according to where the input was entered.
 */
function onInputStart() {
  if (isFakeboxFocused()) {
    setFakeboxFocus(false);
    setFakeboxDragFocus(false);
    setFakeboxAndLogoVisibility(false);
  }
}


/**
 * Callback for embeddedSearch.newTabPage.oninputcancel. Restores the NTP
 * (re-enables the fakebox and unhides the logo.)
 */
function onInputCancel() {
  setFakeboxAndLogoVisibility(true);
}


/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} focus True to show a dragging focus on the fakebox.
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
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return $(IDS.FAKEBOX).contains(event.target);
}


/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxAndLogoVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX_AND_LOGO, !show);
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
 * Event handler for the focus changed and blacklist messages on link elements.
 * Used to toggle visual treatment on the tiles (depending on the message).
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  var cmd = event.data.cmd;
  var args = event.data;
  if (cmd == 'tileBlacklisted') {
    showNotification();
    lastBlacklistedTile = args.tid;

    ntpApiHandle.deleteMostVisitedItem(args.tid);
  }
  // TODO(treib): Should we also handle the 'loaded' message from the iframe
  // here? We could hide the page until it arrives, to avoid flicker.
}


/**
 * Prepares the New Tab Page by adding listeners, the most visited pages
 * section, and Google-specific elements for a Google-provided page.
 */
function init() {
  // Hide notifications after fade out, so we can't focus on links via keyboard.
  $(IDS.NOTIFICATION).addEventListener('transitionend', hideNotification);

  $(IDS.NOTIFICATION_MESSAGE).textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, KEYCODE.ENTER, onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.ENTER, onRestoreAll);
  restoreAllLink.textContent =
      configData.translatedStrings.restoreThumbnailsShort;

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  $(IDS.NOTIFICATION_CLOSE_BUTTON).addEventListener('click', hideNotification);

  window.addEventListener('resize', onResize);
  updateContentWidth();

  var embeddedSearchApiHandle = window.chrome.embeddedSearch;

  ntpApiHandle = embeddedSearchApiHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  var searchboxApiHandle = embeddedSearchApiHandle.searchBox;

  if (configData.isGooglePage) {
    // Set up the fakebox (which only exists on the Google NTP).
    ntpApiHandle.oninputstart = onInputStart;
    ntpApiHandle.oninputcancel = onInputCancel;

    if (ntpApiHandle.isInputInProgress) {
      onInputStart();
    }

    $(IDS.FAKEBOX_TEXT).textContent =
        configData.translatedStrings.searchboxPlaceholder;

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
      setFakeboxDragFocus(false);
    };
    inputbox.ondragenter = function() {
      setFakeboxDragFocus(true);
    };
    inputbox.ondragleave = function() {
      setFakeboxDragFocus(false);
    };

    // Update the fakebox style to match the current key capturing state.
    setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);

    // Inject the OneGoogleBar loader script. It'll create a global variable
    // named "og" with the following fields:
    //  .html - the main bar HTML.
    //  .end_of_body_html - HTML to be inserted at the end of the body.
    var ogScript = document.createElement('script');
    ogScript.src = 'chrome-search://local-ntp/one-google.js';
    document.body.appendChild(ogScript);
    ogScript.onload = function() {
      injectOneGoogleBar(og.html, og.end_of_body_html);
    };
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
    // Grabbing the root HTML element.
    document.documentElement.setAttribute('dir', 'rtl');
    // Add class for setting alignments based on language directionality.
    document.documentElement.classList.add(CLASSES.RTL);
  }

  // Collect arguments for the most visited iframe.
  var args = [];

  if (searchboxApiHandle.rtl)
    args.push('rtl=1');
  if (NTP_DESIGN.numTitleLines > 1)
    args.push('ntl=' + NTP_DESIGN.numTitleLines);

  args.push('removeTooltip=' +
      encodeURIComponent(configData.translatedStrings.removeThumbnailTooltip));

  // Create the most visited iframe.
  var iframe = document.createElement('iframe');
  iframe.id = IDS.TILES_IFRAME;
  iframe.tabIndex = 1;
  iframe.src = 'chrome-search://most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    reloadTiles();
    renderTheme();
  };

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}


/**
 * Injects the One Google Bar into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectOneGoogleBar(barHtml, endOfBodyHtml) {
  var inHeadStyle = document.createElement('link');
  inHeadStyle.rel = "stylesheet";
  inHeadStyle.href = 'chrome-search://local-ntp/one-google/in-head.css';
  document.head.appendChild(inHeadStyle);

  inHeadStyle.onload = function() {
    var inHeadScript = document.createElement('script');
    inHeadScript.src = 'chrome-search://local-ntp/one-google/in-head.js';
    document.head.appendChild(inHeadScript);

    inHeadScript.onload = function() {
      var ogElem = $('one-google');
      ogElem.innerHTML = barHtml;
      ogElem.classList.remove('hidden');

      var afterBarScript = document.createElement('script');
      afterBarScript.src =
          'chrome-search://local-ntp/one-google/after-bar.js';
      ogElem.parentNode.insertBefore(afterBarScript, ogElem.nextSibling);

      afterBarScript.onload = function() {
        $('one-google-end-of-body').innerHTML = endOfBodyHtml;

        var endOfBodyScript = document.createElement('script');
        endOfBodyScript.src =
            'chrome-search://local-ntp/one-google/end-of-body.js';
        document.body.appendChild(endOfBodyScript);
      };
    };
  };
}


return {
  init: init,  // Exposed for testing.
  listen: listen
};

}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
