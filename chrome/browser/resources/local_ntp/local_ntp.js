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
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 *
 * @type {{
 *   numTitleLines: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 * }}
 */
var NTP_DESIGN = {
  numTitleLines: 1,
  titleColor: [50, 50, 50, 255],
  titleColorAgainstDark: [210, 210, 210, 255],
};


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo',  // Shows white logo if required by theme
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  FADE: 'fade',  // Enables opacity transition on logo and doodle.
  FAKEBOX_FOCUS: 'fakebox-focused',  // Applies focus styles to the fakebox
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  HIDE_FAKEBOX_AND_LOGO: 'hide-fakebox-logo',
  HIDE_NOTIFICATION: 'mv-notice-hide',
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  NON_WHITE_BG: 'non-white-bg',
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
  FAKEBOX_MICROPHONE: 'fakebox-microphone',
  LOGO: 'logo',
  LOGO_DEFAULT: 'logo-default',
  LOGO_DOODLE: 'logo-doodle',
  LOGO_DOODLE_IMAGE: 'logo-doodle-image',
  LOGO_DOODLE_LINK: 'logo-doodle-link',
  LOGO_DOODLE_NOTIFIER: 'logo-doodle-notifier',
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
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
var LOG_TYPE = {
  // A static Doodle was shown, coming from cache.
  NTP_STATIC_LOGO_SHOWN_FROM_CACHE: 30,
  // A static Doodle was shown, coming from the network.
  NTP_STATIC_LOGO_SHOWN_FRESH: 31,
  // A call-to-action Doodle image was shown, coming from cache.
  NTP_CTA_LOGO_SHOWN_FROM_CACHE: 32,
  // A call-to-action Doodle image was shown, coming from the network.
  NTP_CTA_LOGO_SHOWN_FRESH: 33,

  // A static Doodle was clicked.
  NTP_STATIC_LOGO_CLICKED: 34,
  // A call-to-action Doodle was clicked.
  NTP_CTA_LOGO_CLICKED: 35,
  // An animated Doodle was clicked.
  NTP_ANIMATED_LOGO_CLICKED: 36,

  // The One Google Bar was shown.
  NTP_ONE_GOOGLE_BAR_SHOWN: 37,
};


/**
 * Background colors considered "white". Used to determine if it is possible
 * to display a Google Doodle, or if the notifier should be used instead.
 * @type {Array<string>}
 * @const
 */
var WHITE_BACKGROUND_COLORS = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];


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
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/** @type {number} @const */
var MAX_NUM_TILES_TO_SHOW = 8;


/**
 * Returns theme background info, first checking for history.state.notheme. If
 * the page has notheme set, returns a fallback light-colored theme.
 */
function getThemeBackgroundInfo() {
  if (history.state && history.state.notheme) {
    return {
      alternateLogo: false,
      backgroundColorRgba: [255, 255, 255, 255],
      colorRgba: [255, 255, 255, 255],
      headerColorRgba: [150, 150, 150, 255],
      linkColorRgba: [6, 55, 116, 255],
      sectionBorderColorRgba: [150, 150, 150, 255],
      textColorLightRgba: [102, 102, 102, 255],
      textColorRgba: [0, 0, 0, 255],
      usingDefaultTheme: true,
    };
  }
  return ntpApiHandle.themeBackgroundInfo;
}


/**
 * Heuristic to determine whether a theme should be considered to be dark, so
 * the colors of various UI elements can be adjusted.
 * @param {ThemeBackgroundInfo|undefined} info Theme background information.
 * @return {boolean} Whether the theme is dark.
 * @private
 */
function getIsThemeDark() {
  var info = getThemeBackgroundInfo();
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
  var info = getThemeBackgroundInfo();
  var isThemeDark = getIsThemeDark();
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
  var isNonWhiteBackground = !WHITE_BACKGROUND_COLORS.includes(background);
  document.body.classList.toggle(CLASSES.NON_WHITE_BG, isNonWhiteBackground);
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
 * Updates the OneGoogleBar (if it is loaded) based on the current theme.
 * @private
 */
function renderOneGoogleBarTheme() {
  if (!window.gbar) {
    return;
  }
  try {
    var oneGoogleBarApi = window.gbar.a;
    var oneGoogleBarPromise = oneGoogleBarApi.bf();
    oneGoogleBarPromise.then(function(oneGoogleBar) {
      var isThemeDark = getIsThemeDark();
      var setForegroundStyle = oneGoogleBar.pc.bind(oneGoogleBar);
      setForegroundStyle(isThemeDark ? 1 : 0);
    });
  } catch (err) {
    console.log('Failed setting OneGoogleBar theme:\n' + err);
  }
}


/**
 * Callback for embeddedSearch.newTabPage.onthemechange.
 * @private
 */
function onThemeChange() {
  renderTheme();
  renderOneGoogleBarTheme();
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
  cmds.push({cmd: 'show'});

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
  return $(IDS.FAKEBOX).contains(event.target) &&
      !$(IDS.FAKEBOX_MICROPHONE).contains(event.target);
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
 * Event handler for messages from the most visited iframe.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  var cmd = event.data.cmd;
  var args = event.data;
  if (cmd == 'loaded') {
    if (configData.isGooglePage) {
      // Load the OneGoogleBar script. It'll create a global variable name "og"
      // which is a dict corresponding to the native OneGoogleBarData type.
      // We do this only after all the tiles have loaded, to avoid slowing down
      // the main page load.
      var ogScript = document.createElement('script');
      ogScript.src = 'chrome-search://local-ntp/one-google.js';
      document.body.appendChild(ogScript);
      ogScript.onload = function() {
        injectOneGoogleBar(og);
      };
    }
  } else if (cmd == 'tileBlacklisted') {
    showNotification();
    lastBlacklistedTile = args.tid;

    ntpApiHandle.deleteMostVisitedItem(args.tid);
  }
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

    if (configData.isVoiceSearchEnabled) {
      speech.init(
          configData.googleBaseUrl, configData.translatedStrings,
          $(IDS.FAKEBOX_MICROPHONE), searchboxApiHandle);
    }

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

    // Load the Doodle. After the first request completes (getting cached
    // data), issue a second request for fresh Doodle data.
    loadDoodle(/*v=*/null, function(ddl) {
      if (ddl === null) {
        // Got no ddl object at all, the feature is probably disabled. Just show
        // the logo.
        showLogoOrDoodle(null, null, /*fromCache=*/true);
        return;
      }

      // Got a (possibly empty) ddl object. Show logo or doodle.
      showLogoOrDoodle(
          ddl.image || null, ddl.metadata || null, /*fromCache=*/true);
      // If we got a valid ddl object (from cache), load a fresh one.
      if (ddl.v !== null) {
        loadDoodle(ddl.v, function(ddl) {
          if (ddl.usable) {
            fadeToLogoOrDoodle(ddl.image, ddl.metadata);
          }
        });
      }
    });

    // Set up doodle notifier (but it may be invisible).
    var doodleNotifier = $(IDS.LOGO_DOODLE_NOTIFIER);
    doodleNotifier.title = configData.translatedStrings.clickToViewDoodle;
    doodleNotifier.addEventListener('click', function(e) {
      e.preventDefault();
      var state = window.history.state || {};
      state.notheme = true;
      window.history.replaceState(state, document.title);
      onThemeChange();
    });
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
function injectOneGoogleBar(ogb) {
  var inHeadStyle = document.createElement('style');
  inHeadStyle.type = 'text/css';
  inHeadStyle.appendChild(document.createTextNode(ogb.inHeadStyle));
  document.head.appendChild(inHeadStyle);

  var inHeadScript = document.createElement('script');
  inHeadScript.type = 'text/javascript';
  inHeadScript.appendChild(document.createTextNode(ogb.inHeadScript));
  document.head.appendChild(inHeadScript);

  renderOneGoogleBarTheme();

  var ogElem = $('one-google');
  ogElem.innerHTML = ogb.barHtml;
  ogElem.classList.remove('hidden');

  var afterBarScript = document.createElement('script');
  afterBarScript.type = 'text/javascript';
  afterBarScript.appendChild(document.createTextNode(ogb.afterBarScript));
  ogElem.parentNode.insertBefore(afterBarScript, ogElem.nextSibling);

  $('one-google-end-of-body').innerHTML = ogb.endOfBodyHtml;

  var endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(document.createTextNode(ogb.endOfBodyScript));
  document.body.appendChild(endOfBodyScript);

  ntpApiHandle.logEvent(LOG_TYPE.NTP_ONE_GOOGLE_BAR_SHOWN);
}


/** Loads the Doodle. On success, the loaded script declares a global variable
 * ddl, which onload() receives as its single argument. On failure, onload() is
 * called with null as the argument. If v is null, then the call requests a
 * cached logo. If non-null, it must be the ddl.v of a previous request for a
 * cached logo, and the corresponding fresh logo is returned.
 * @param {?number} v
 * @param {function(?{v, usable, image, metadata})} onload
 */
var loadDoodle = function(v, onload) {
  var ddlScript = document.createElement('script');
  ddlScript.src = 'chrome-search://local-ntp/doodle.js';
  if (v !== null)
    ddlScript.src += '?v=' + v;
  ddlScript.onload = function() {
    onload(ddl);
  };
  ddlScript.onerror = function() {
    onload(null);
  };
  // TODO(treib,sfiera): Add a timeout in case something goes wrong?
  document.body.appendChild(ddlScript);
};


/** Returns true if |element| is fully hidden. Returns false if fully visible,
 * fading in, or fading out.
 * @param {HTMLElement} element
 */
var isFadedOut = function(element) {
  return (element.style.opacity == 0) &&
      (window.getComputedStyle(element).opacity == 0);
};


/** Returns true if the doodle given by |image| and |metadata| is currently
 * visible. If |image| is null, returns true when the default logo is visible;
 * if non-null, checks that it matches the doodle that is currently visible.
 * Here, "visible" means fully-visible or fading in.
 *
 * @param {?Object} image
 * @param {?Object} metadata
 * @returns {boolean}
 */
var isDoodleCurrentlyVisible = function(image, metadata) {
  var haveDoodle = ($(IDS.LOGO_DOODLE).style.opacity != 0);
  var wantDoodle = (image !== null) && (metadata !== null);
  if (!haveDoodle || !wantDoodle)
    return haveDoodle === wantDoodle;

  // Have a visible doodle and a query doodle. Test that they match.
  var logoDoodleImage = $(IDS.LOGO_DOODLE_IMAGE);
  return (logoDoodleImage.src === image) ||
      (logoDoodleImage.src === metadata.animatedUrl);
};


var showLogoOrDoodle = function(image, metadata, fromCache) {
  if (metadata !== null) {
    applyDoodleMetadata(metadata);
    $(IDS.LOGO_DOODLE_IMAGE).src = image;
    $(IDS.LOGO_DOODLE).style.opacity = 1;

    var isCta = !!metadata.animatedUrl;
    var eventType = isCta ?
        (fromCache ? LOG_TYPE.NTP_CTA_LOGO_SHOWN_FROM_CACHE :
                     LOG_TYPE.NTP_CTA_LOGO_SHOWN_FRESH) :
        (fromCache ? LOG_TYPE.NTP_STATIC_LOGO_SHOWN_FROM_CACHE :
                     LOG_TYPE.NTP_STATIC_LOGO_SHOWN_FRESH);
    ntpApiHandle.logEvent(eventType);
  } else {
    $(IDS.LOGO_DEFAULT).style.opacity = 1;
  }
};


/** The image and metadata that should be shown, according to the latest fetch.
 * After a logo fades out, onDoodleTransitionEnd fades in a logo according to
 * targetDoodle.
 */
var targetDoodle = {
  image: null,
  metadata: null,
};


/**
 * Starts fading out the given element, which should be either the default logo
 * or the doodle.
 *
 * @param {HTMLElement} element
 */
var startFadeOut = function(element) {
  element.classList.add(CLASSES.FADE);
  element.addEventListener('transitionend', onDoodleTransitionEnd);
  element.style.opacity = 0;
};


/**
 * Integrates a fresh doodle into the page as appropriate. If the correct logo
 * or doodle is already shown, just updates the metadata. Otherwise, initiates
 * a fade from the currently-shown logo/doodle to the new one.
 *
 * @param {?Object} image
 * @param {?Object} metadata
 */
var fadeToLogoOrDoodle = function(image, metadata) {
  // If the image is already visible, there's no need to start a fade-out.
  // However, metadata may have changed, so update the doodle's alt text and
  // href, if applicable.
  if (isDoodleCurrentlyVisible(image, metadata)) {
    if (metadata !== null) {
      applyDoodleMetadata(metadata);
    }
    return;
  }

  // Set the target to use once the current logo/doodle has finished fading out.
  targetDoodle.image = image;
  targetDoodle.metadata = metadata;

  // Start fading out the current logo or doodle. onDoodleTransitionEnd will
  // apply the change when the fade-out finishes.
  startFadeOut($(IDS.LOGO_DEFAULT));
  startFadeOut($(IDS.LOGO_DOODLE));
};


var onDoodleTransitionEnd = function(e) {
  var logoDoodle = $(IDS.LOGO_DOODLE);
  var logoDoodleImage = $(IDS.LOGO_DOODLE_IMAGE);
  var logoDefault = $(IDS.LOGO_DEFAULT);

  if (isFadedOut(logoDoodle) && isFadedOut(logoDefault)) {
    // Fade-out finished. Start fading in the appropriate logo.
    showLogoOrDoodle(
        targetDoodle.image, targetDoodle.metadata, /*fromCache=*/false);

    logoDefault.removeEventListener('transitionend', onDoodleTransitionEnd);
    logoDoodle.removeEventListener('transitionend', onDoodleTransitionEnd);
  }
};


var applyDoodleMetadata = function(metadata) {
  var logoDoodleLink = $(IDS.LOGO_DOODLE_LINK);
  var logoDoodleImage = $(IDS.LOGO_DOODLE_IMAGE);

  logoDoodleImage.title = metadata.altText;

  if (metadata.animatedUrl) {
    logoDoodleLink.removeAttribute('href');
    logoDoodleLink.onclick = function(e) {
      ntpApiHandle.logEvent(LOG_TYPE.NTP_CTA_LOGO_CLICKED);
      e.preventDefault();
      logoDoodleImage.src = metadata.animatedUrl;
      logoDoodleLink.href = metadata.onClickUrl;
      logoDoodleLink.onclick = function() {
        ntpApiHandle.logEvent(LOG_TYPE.NTP_ANIMATED_LOGO_CLICKED);
      };
    };
  } else {
    logoDoodleLink.href = metadata.onClickUrl;
    logoDoodleLink.onclick = function() {
      ntpApiHandle.logEvent(LOG_TYPE.NTP_STATIC_LOGO_CLICKED);
    };
  }
};


return {
  init: init,  // Exposed for testing.
  listen: listen
};

}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
