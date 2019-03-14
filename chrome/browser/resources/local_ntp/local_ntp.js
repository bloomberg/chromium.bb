// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview The local InstantExtended NTP.
 */


/**
 * Whether the most visited tiles have finished loading, i.e. we've received the
 * 'loaded' postMessage from the iframe. Used by tests to detect that loading
 * has completed.
 * @type {boolean}
 */
var tilesAreLoaded = false;


/**
 * Whether the Most Visited and edit custom link iframes should be created while
 * running tests. Currently the SimpleJavascriptTests are flaky due to some
 * raciness in the creation/destruction of the iframe. crbug.com/786313.
 * @type {boolean}
 */
let iframesAndVoiceSearchDisabledForTesting = false;


/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
'use strict';


/**
 * Called by tests to disable the creation of Most Visited and edit custom link
 * iframes.
 */
function disableIframesAndVoiceSearchForTesting() {
  iframesAndVoiceSearchDisabledForTesting = true;
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
  // Applies styles to dialogs used in customization.
  CUSTOMIZE_DIALOG: 'customize-dialog',
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  FAKEBOX_FOCUS: 'fakebox-focused',  // Applies focus styles to the fakebox
  SHOW_EDIT_DIALOG: 'show',          // Displays the edit custom link dialog.
  HIDE_BODY_OVERFLOW: 'hidden',      // Prevents scrolling while the edit custom
                                     // link dialog is open.
  // Applies float animations to the Most Visited notification
  FLOAT_DOWN: 'float-down',
  FLOAT_UP: 'float-up',
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  // Applies a different style to the error notification if a link is present.
  HAS_LINK: 'has-link',
  HIDE_FAKEBOX: 'hide-fakebox',
  HIDE_NOTIFICATION: 'notice-hide',
  HIDE_PROMO: 'hide-promo',
  INITED: 'inited',  // Reveals the <body> once init() is done.
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
  MATERIAL_DESIGN_ICONS:
      'md-icons',  // Applies Material Design styles to Most Visited.
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  NON_WHITE_BG: 'non-white-bg',
  REMOVE_FAKEBOX: 'remove-fakebox',  // Hides the fakebox from the page.
  RTL: 'rtl',                        // Right-to-left language text.
  // Applied when the doodle notifier should be shown instead of the doodle.
  USE_NOTIFIER: 'use-notifier',
};


/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
var IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
  CUSTOM_LINKS_EDIT_IFRAME: 'custom-links-edit',
  CUSTOM_LINKS_EDIT_IFRAME_DIALOG: 'custom-links-edit-dialog',
  ERROR_NOTIFICATION: 'error-notice',
  ERROR_NOTIFICATION_CONTAINER: 'error-notice-container',
  ERROR_NOTIFICATION_LINK: 'error-notice-link',
  ERROR_NOTIFICATION_MSG: 'error-notice-msg',
  FAKEBOX: 'fakebox',
  FAKEBOX_CONTAINER: 'fakebox-container',
  FAKEBOX_INPUT: 'fakebox-input',
  FAKEBOX_TEXT: 'fakebox-text',
  FAKEBOX_MICROPHONE: 'fakebox-microphone',
  LOGO: 'logo',
  MOST_VISITED: 'most-visited',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CONTAINER: 'mv-notice-container',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  PROMO: 'promo',
  RESTORE_ALL_LINK: 'mv-restore',
  SUGGESTIONS: 'suggestions',
  TILES: 'mv-tiles',
  TILES_IFRAME: 'mv-single',
  UNDO_LINK: 'mv-undo',
  USER_CONTENT: 'user-content',
};


/**
 * Counterpart of search_provider_logos::LogoType.
 * @enum {string}
 * @const
 */
var LOGO_TYPE = {
  SIMPLE: 'SIMPLE',
  ANIMATED: 'ANIMATED',
  INTERACTIVE: 'INTERACTIVE',
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

  // 'Cancel' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_CANCEL: 54,
  // 'Done' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_DONE: 55,
};


/**
 * The maximum number of tiles to show in the Most Visited section.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_MOST_VISITED = 8;


/**
 * The maximum number of tiles to show in the Most Visited section if custom
 * links is enabled.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_CUSTOM_LINKS = 10;


/**
 * Background colors considered "white". Used to determine if it is possible to
 * display a Google Doodle, or if the notifier should be used instead. Also used
 * to determine if a colored or white logo should be used.
 * @type {Array<string>}
 * @const
 */
var WHITE_BACKGROUND_COLORS = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];


/**
 * Background color for Chrome dark mode. Used to determine if it is possible to
 * display a Google Doodle, or if the notifier should be used instead.
 * @type {string}
 * @const
 */
const DARK_MODE_BACKGROUND_COLOR = 'rgba(50,54,57,1)';


/**
 * Enum for keycodes.
 * @enum {number}
 * @const
 */
var KEYCODE = {ENTER: 13, SPACE: 32};


/**
 * The period of time (ms) before the Most Visited notification is hidden.
 * @type {number}
 */
const NOTIFICATION_TIMEOUT = 10000;


/**
 * The period of time (ms) before transitions can be applied to a toast
 * notification after modifying the "display" property.
 * @type {number}
 */
const DISPLAY_TIMEOUT = 20;


/**
 * The last blacklisted tile rid if any, which by definition should not be
 * filler.
 * @type {?number}
 */
var lastBlacklistedTile = null;


/**
 * The timeout function for automatically hiding the pop-up notification. Only
 * set if a notification is visible.
 * @type {?Object}
 */
let delayedHideNotification;


/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/**
 * True if dark mode is enabled.
 * @type {boolean}
 */
let isDarkModeEnabled = false;


/**
 * True if dark colored chips should be used instead of light mode chips when
 * dark mode is enabled.
 * @type {boolean}
 */
let useDarkChips = false;


/**
 * Returns a timeout that can be executed early. Calls back true if this was
 * an early execution, false otherwise.
 * @param {!Function} timeout The timeout function. Requires a boolean param.
 * @param {number} delay The timeout delay.
 * @return {Object}
 */
function createExecutableTimeout(timeout, delay) {
  let timeoutId = window.setTimeout(() => {
    timeout(/*executedEarly=*/ false);
  }, delay);
  return {
    clear: () => {
      window.clearTimeout(timeoutId);
    },
    trigger: () => {
      window.clearTimeout(timeoutId);
      return timeout(/*executedEarly=*/ true);
    }
  };
}


/**
 * Returns theme background info, first checking for history.state.notheme. If
 * the page has notheme set, returns a fallback light-colored theme (or dark-
 * colored theme if dark mode is enabled). This is used when the doodle is
 * displayed after clicking the notifier.
 */
function getThemeBackgroundInfo() {
  if (history.state && history.state.notheme) {
    return {
      alternateLogo: false,
      backgroundColorRgba:
          (isDarkModeEnabled ? [50, 54, 57, 255] : [255, 255, 255, 255]),
      colorRgba: [255, 255, 255, 255],
      headerColorRgba: [150, 150, 150, 255],
      linkColorRgba: [6, 55, 116, 255],
      sectionBorderColorRgba: [150, 150, 150, 255],
      textColorLightRgba: [102, 102, 102, 255],
      textColorRgba: [0, 0, 0, 255],
      usingDarkMode: isDarkModeEnabled,
      usingDefaultTheme: true,
    };
  }
  return ntpApiHandle.themeBackgroundInfo;
}


/**
 * Heuristic to determine whether a theme should be considered to be dark, so
 * the colors of various UI elements can be adjusted.
 *
 * The user theme/custom background will always take precedence over dark mode
 * when considering darkness. Therefore, dark mode should only be checked if
 * this is the default NTP. Dark mode is considered a dark theme if enabled.
 *
 * @param {ThemeBackgroundInfo|undefined} info Theme background information.
 * @return {boolean} Whether the theme is dark.
 * @private
 */
function getIsThemeDark() {
  var info = getThemeBackgroundInfo();
  // Only check for dark mode if this is the default NTP (i.e. no theme or
  // custom background set).
  if (!info || info.usingDefaultTheme && !info.customBackgroundConfigured) {
    // Dark mode is always considered a dark theme.
    return isDarkModeEnabled;
  }

  // Heuristic: light text implies dark theme.
  var rgba = info.textColorRgba;
  var luminance = 0.3 * rgba[0] + 0.59 * rgba[1] + 0.11 * rgba[2];
  return luminance >= 128;
}


/**
 * Determine whether dark chips should be used if dark mode is enabled. This is
 * is the case when dark mode is enabled and a background image (from a custom
 * background or user theme) is not set.
 *
 * @param {ThemeBackgroundInfo|undefined} info Theme background information.
 * @return {boolean} Whether the chips should be dark.
 * @private
 */
function getUseDarkChips(info) {
  return info.usingDarkMode && !info.imageUrl;
}


/**
 * Updates the NTP based on the current theme.
 * @private
 */
function renderTheme() {
  $(IDS.NTP_CONTENTS).classList.toggle(CLASSES.DARK, getIsThemeDark());

  var info = getThemeBackgroundInfo();
  if (!info) {
    return;
  }

  // Update dark mode styling.
  isDarkModeEnabled = info.usingDarkMode;
  useDarkChips = getUseDarkChips(info);
  document.documentElement.setAttribute('darkmode', isDarkModeEnabled);
  document.body.classList.toggle('light-chip', !useDarkChips);

  var background = [
    convertToRGBAColor(info.backgroundColorRgba), info.imageUrl,
    info.imageTiling, info.imageHorizontalAlignment, info.imageVerticalAlignment
  ].join(' ').trim();

  // If a custom background has been selected the image will be applied to the
  // custom-background element instead of the body.
  if (!info.customBackgroundConfigured) {
    document.body.style.background = background;
  }

  // Dark mode uses a white Google logo.
  const useWhiteLogo =
      info.alternateLogo || (info.usingDefaultTheme && isDarkModeEnabled);
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, useWhiteLogo);
  const isNonWhiteBackground = !WHITE_BACKGROUND_COLORS.includes(background);
  document.body.classList.toggle(CLASSES.NON_WHITE_BG, isNonWhiteBackground);

  // The doodle notifier should be shown for non-default backgrounds. This
  // includes non-white backgrounds, excluding dark mode gray if dark mode is
  // enabled.
  const isDefaultBackground = WHITE_BACKGROUND_COLORS.includes(background) ||
      (isDarkModeEnabled && background === DARK_MODE_BACKGROUND_COLOR);
  document.body.classList.toggle(CLASSES.USE_NOTIFIER, !isDefaultBackground);

  updateThemeAttribution(info.attributionUrl, info.imageHorizontalAlignment);
  setCustomThemeStyle(info);

  if (info.customBackgroundConfigured) {
    var imageWithOverlay = [
      customBackgrounds.CUSTOM_BACKGROUND_OVERLAY, 'url(' + info.imageUrl + ')'
    ].join(',').trim();

    if (imageWithOverlay != document.body.style.backgroundImage) {
      customBackgrounds.closeCustomizationDialog();
      customBackgrounds.clearAttribution();
    }

    // |image| and |imageWithOverlay| use the same url as their source. Waiting
    // to display the custom background until |image| is fully loaded ensures
    // that |imageWithOverlay| is also loaded.
    $('custom-bg').style.backgroundImage = imageWithOverlay;
    var image = new Image();
    image.onload = function() {
      $('custom-bg').style.opacity = '1';
    };
    image.src = info.imageUrl;

    customBackgrounds.setAttribution(
        info.attribution1, info.attribution2, info.attributionActionUrl);
  } else {
    $('custom-bg').style.opacity = '0';
    window.setTimeout(function() {
      $('custom-bg').style.backgroundImage = '';
    }, 1000);
    customBackgrounds.clearAttribution();
  }

  $(customBackgrounds.IDS.RESTORE_DEFAULT)
      .classList.toggle(
          customBackgrounds.CLASSES.OPTION_DISABLED,
          !info.customBackgroundConfigured);
  $(customBackgrounds.IDS.RESTORE_DEFAULT).tabIndex =
      (info.customBackgroundConfigured ? 0 : -1);

  $(customBackgrounds.IDS.EDIT_BG)
      .classList.toggle(
          customBackgrounds.CLASSES.ENTRY_POINT_ENHANCED,
          !info.customBackgroundConfigured);

  if (configData.isGooglePage) {
    // Hide the settings menu or individual options if the related features are
    // disabled.
    customBackgrounds.setMenuVisibility();
  }
}

/**
 * Sends the current theme info to the most visited iframe.
 * @private
 */
function sendThemeInfoToMostVisitedIframe() {
  var info = getThemeBackgroundInfo();
  if (!info) {
    return;
  }

  var isThemeDark = getIsThemeDark();

  var message = {cmd: 'updateTheme'};
  message.isThemeDark = isThemeDark;
  message.isUsingTheme = !info.usingDefaultTheme;
  message.isDarkMode = getUseDarkChips(info);

  var titleColor = NTP_DESIGN.titleColor;
  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = info.textColorRgba;
  } else if (isThemeDark) {
    titleColor = NTP_DESIGN.titleColorAgainstDark;
  }
  message.tileTitleColor = convertToRGBAColor(titleColor);

  $(IDS.TILES_IFRAME).contentWindow.postMessage(message, '*');
}


/**
 * Sends the current theme info to the edit custom link iframe.
 * @private
 */
function sendThemeInfoToEditCustomLinkIframe() {
  if (!configData.isGooglePage) {
    return;
  }

  const info = getThemeBackgroundInfo();
  if (!info) {
    return;
  }

  let message = {cmd: 'updateTheme'};
  message.isDarkMode = info.usingDarkMode;

  $(IDS.CUSTOM_LINKS_EDIT_IFRAME).contentWindow.postMessage(message, '*');
}


/**
 * Updates the OneGoogleBar (if it is loaded) based on the current theme.
 * TODO(crbug.com/918582): Add support for OGB dark mode.
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
  // Save the current dark mode state to check if dark mode has changed.
  const usingDarkChips = useDarkChips;

  renderTheme();
  renderOneGoogleBarTheme();
  sendThemeInfoToMostVisitedIframe();
  sendThemeInfoToEditCustomLinkIframe();

  // If dark mode has been changed, refresh the MV tiles to render the
  // appropriate icon.
  if (usingDarkChips != useDarkChips) {
    reloadTiles();
  }
}


/**
 * Updates the NTP style according to theme.
 * @param {Object} themeInfo The information about the theme.
 * @private
 */
function setCustomThemeStyle(themeInfo) {
  var textColor = null;
  var textColorLight = null;
  var mvxFilter = null;
  if (!themeInfo.usingDefaultTheme) {
    textColor = convertToRGBAColor(themeInfo.textColorRgba);
    textColorLight = convertToRGBAColor(themeInfo.textColorLightRgba);
    mvxFilter = 'drop-shadow(0 0 0 ' + textColor + ')';
  }

  $(IDS.NTP_CONTENTS)
      .classList.toggle(CLASSES.DEFAULT_THEME, themeInfo.usingDefaultTheme);

  document.body.style.setProperty('--text-color', textColor);
  document.body.style.setProperty('--text-color-light', textColorLight);
  // Themes reuse the "light" text color for links too.
  document.body.style.setProperty('--text-color-link', textColorLight);
  $(IDS.NOTIFICATION_CLOSE_BUTTON)
      .style.setProperty('--theme-filter', mvxFilter);
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
  // Don't attempt to load tiles if the MV data isn't available yet - this can
  // happen occasionally, see https://crbug.com/794942. In that case, we should
  // get an onMostVisitedChange call once they are available.
  // Note that MV data being available is different from having > 0 tiles. There
  // can legitimately be 0 tiles, e.g. if the user blacklisted them all.
  if (!ntpApiHandle.mostVisitedAvailable) {
    return;
  }

  var pages = ntpApiHandle.mostVisited;
  var cmds = [];
  let maxNumTiles = configData.isGooglePage ? MAX_NUM_TILES_CUSTOM_LINKS :
                                              MAX_NUM_TILES_MOST_VISITED;
  for (var i = 0; i < Math.min(maxNumTiles, pages.length); ++i) {
    cmds.push({cmd: 'tile', rid: pages[i].rid, darkMode: useDarkChips});
  }
  cmds.push({cmd: 'show'});

  $(IDS.TILES_IFRAME).contentWindow.postMessage(cmds, '*');
}


/**
 * Callback for embeddedSearch.newTabPage.onaddcustomlinkdone. Called when the
 * custom link was successfully added. Shows the "Shortcut added" notification.
 * @param {boolean} success True if the link was successfully added.
 */
function onAddCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkAddedMsg);
  } else {
    showErrorNotification(configData.translatedStrings.linkCantCreate);
  }
  ntpApiHandle.logEvent(LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_DONE);
}


/**
 * Callback for embeddedSearch.newTabPage.onupdatecustomlinkdone. Called when
 * the custom link was successfully updated. Shows the "Shortcut edited"
 * notification.
 * @param {boolean} success True if the link was successfully updated.
 */
function onUpdateCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkEditedMsg);
  } else {
    showErrorNotification(configData.translatedStrings.linkCantEdit);
  }
}


/**
 * Callback for embeddedSearch.newTabPage.ondeletecustomlinkdone. Called when
 * the custom link was successfully deleted. Shows the "Shortcut deleted"
 * notification.
 * @param {boolean} success True if the link was successfully deleted.
 */
function onDeleteCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkRemovedMsg);
  } else {
    showErrorNotification(configData.translatedStrings.linkCantRemove);
  }
}


/**
 * Shows the Most Visited pop-up notification and triggers a delay to hide it.
 * The message will be set to |msg|.
 * @param {string} msg The notification message.
 */
function showNotification(msg) {
  $(IDS.NOTIFICATION_MESSAGE).textContent = msg;

  if (configData.isGooglePage) {
    floatUpNotification($(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER));
  } else {
    var notification = $(IDS.NOTIFICATION);
    notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
    notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
    notification.scrollTop;
    notification.classList.add(CLASSES.DELAYED_HIDE_NOTIFICATION);
  }

  $(IDS.UNDO_LINK).focus();
}


/**
 * Hides the Most Visited pop-up notification.
 */
function hideNotification() {
  if (configData.isGooglePage) {
    floatDownNotification(
        $(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER),
        /*showPromo=*/ true);
  } else {
    var notification = $(IDS.NOTIFICATION);
    notification.classList.add(CLASSES.HIDE_NOTIFICATION);
    notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
  }
}


/**
 * Shows the error pop-up notification and triggers a delay to hide it. The
 * message will be set to |msg|. If |linkName| and |linkOnClick| are present,
 * shows an error link with text set to |linkName| and onclick handled by
 * |linkOnClick|.
 * @param {string} msg The notification message.
 * @param {?string} linkName The error link text.
 * @param {?Function} linkOnClick The error link onclick handler.
 */
function showErrorNotification(msg, linkName, linkOnClick) {
  let notification = $(IDS.ERROR_NOTIFICATION);
  $(IDS.ERROR_NOTIFICATION_MSG).textContent = msg;
  if (linkName && linkOnClick) {
    let notificationLink = $(IDS.ERROR_NOTIFICATION_LINK);
    notificationLink.textContent = linkName;
    notificationLink.onclick = linkOnClick;
    notification.classList.add(CLASSES.HAS_LINK);
  } else {
    notification.classList.remove(CLASSES.HAS_LINK);
  }
  floatUpNotification(notification, $(IDS.ERROR_NOTIFICATION_CONTAINER));
}


/**
 * Animates the specified notification to float up. Automatically hides any
 * pre-existing notification and sets a delayed timer to hide the new
 * notification.
 * @param {!Element} notification The notification element.
 * @param {!Element} notificationContainer The notification container element.
 */
function floatUpNotification(notification, notificationContainer) {
  // Hide any pre-existing notification.
  if (delayedHideNotification) {
    delayedHideNotification.trigger();
    delayedHideNotification = null;
  }

  // Hide middle-slot promo if one is present.
  let promo = $(IDS.PROMO);
  if (promo) {
    promo.classList.add(CLASSES.FLOAT_DOWN);
    // Prevent keyboard focus once the promo is hidden.
    promo.addEventListener('transitionend', (event) => {
      if (event.propertyName === 'bottom' &&
          promo.classList.contains(CLASSES.FLOAT_DOWN)) {
        promo.classList.add(CLASSES.HIDE_NOTIFICATION);
      }
    }, {once: true});
  }

  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  // Timeout is required for the "float" transition to work. Modifying the
  // "display" property prevents transitions from activating for a brief period
  // of time.
  window.setTimeout(() => {
    notificationContainer.classList.add(CLASSES.FLOAT_UP);
  }, DISPLAY_TIMEOUT);

  // Automatically hide the notification after a period of time.
  delayedHideNotification = createExecutableTimeout((executedEarly) => {
    // Early execution occurs if another notification should be shown. In this
    // case, we do not want to re-show the promo yet.
    floatDownNotification(notification, notificationContainer, !executedEarly);
  }, NOTIFICATION_TIMEOUT);
}


/**
 * Animates the pop-up notification to float down, and clears the timeout to
 * hide the notification.
 * @param {!Element} notification The notification element.
 * @param {!Element} notificationContainer The notification container element.
 * @param {boolean} showPromo Do show the promo if present.
 */
function floatDownNotification(notification, notificationContainer, showPromo) {
  if (!notificationContainer.classList.contains(CLASSES.FLOAT_UP)) {
    return;
  }

  // Clear the timeout to hide the notification.
  if (delayedHideNotification) {
    delayedHideNotification.clear();
    delayedHideNotification = null;
  }

  if (showPromo) {
    // Show middle-slot promo if one is present.
    let promo = $(IDS.PROMO);
    if (promo) {
      promo.classList.remove(CLASSES.HIDE_NOTIFICATION);
      // Timeout is required for the "float" transition to work. Modifying the
      // "display" property prevents transitions from activating for a brief
      // period of time.
      window.setTimeout(() => {
        promo.classList.remove(CLASSES.FLOAT_DOWN);
      }, DISPLAY_TIMEOUT);
    }
  }

  // Reset notification visibility once the animation is complete.
  notificationContainer.addEventListener('transitionend', (event) => {
    // Blur the hidden items.
    $(IDS.UNDO_LINK).blur();
    $(IDS.RESTORE_ALL_LINK).blur();
    if (notification.classList.contains(CLASSES.HAS_LINK)) {
      notification.classlist.remove(CLASSES.HAS_LINK);
      $(IDS.ERROR_NOTIFICATION_LINK).blur();
    }
    // Hide the notification
    if (!notification.classList.contains(CLASSES.FLOAT_UP)) {
      notification.classList.add(CLASSES.HIDE_NOTIFICATION);
    }
  }, {once: true});
  notificationContainer.classList.remove(CLASSES.FLOAT_UP);
}


/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (configData.isGooglePage) {
    ntpApiHandle.undoCustomLinkAction();
  } else if (lastBlacklistedTile != null) {
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedTile);
  }
}


/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (configData.isGooglePage) {
    ntpApiHandle.resetCustomLinks();
  } else {
    ntpApiHandle.undoAllMostVisitedDeletions();
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
    setFakeboxVisibility(false);
  }
}


/**
 * Callback for embeddedSearch.newTabPage.oninputcancel. Restores the NTP
 * (re-enables the fakebox and unhides the logo.)
 */
function onInputCancel() {
  setFakeboxVisibility(true);
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
function setFakeboxVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX, !show);
}


/**
 * @param {!Element} element The element to register the handler for.
 * @param {number} keycode The keycode of the key to register.
 * @param {!Function} handler The key handler to register.
 */
function registerKeyHandler(element, keycode, handler) {
  element.addEventListener('keydown', function(event) {
    if (event.keyCode == keycode) {
      handler(event);
    }
  });
}


/**
 * Event handler for messages from the most visited and edit custom link iframe.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  var cmd = event.data.cmd;
  var args = event.data;
  if (cmd === 'loaded') {
    tilesAreLoaded = true;
    if (configData.isGooglePage) {
      // Show search suggestions if they were previously hidden.
      if ($(IDS.SUGGESTIONS)) {
        $(IDS.SUGGESTIONS).style.visibility = 'visible';
      }
      if (!$('one-google-loader')) {
        // Load the OneGoogleBar script. It'll create a global variable name
        // "og" which is a dict corresponding to the native OneGoogleBarData
        // type. We do this only after all the tiles have loaded, to avoid
        // slowing down the main page load.
        var ogScript = document.createElement('script');
        ogScript.id = 'one-google-loader';
        ogScript.src = 'chrome-search://local-ntp/one-google.js';
        document.body.appendChild(ogScript);
        ogScript.onload = function() {
          injectOneGoogleBar(og);
        };
      }
      if (!$('promo-loader')) {
        var promoScript = document.createElement('script');
        promoScript.id = 'promo-loader';
        promoScript.src = 'chrome-search://local-ntp/promo.js';
        document.body.appendChild(promoScript);
        promoScript.onload = function() {
          injectPromo(promo);
        };
      }
      $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
          .classList.toggle(
              customBackgrounds.CLASSES.OPTION_DISABLED,
              !args.showRestoreDefault);
      $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).tabIndex =
          (args.showRestoreDefault ? 0 : -1);
    }
  } else if (cmd === 'tileBlacklisted') {
    if (configData.isGooglePage) {
      showNotification(configData.translatedStrings.linkRemovedMsg);
    } else {
      showNotification(
          configData.translatedStrings.thumbnailRemovedNotification);
    }
    lastBlacklistedTile = args.tid;

    ntpApiHandle.deleteMostVisitedItem(args.tid);
  } else if (cmd === 'resizeDoodle') {
    doodles.resizeDoodleHandler(args);
  } else if (cmd === 'startEditLink') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME)
        .contentWindow.postMessage({cmd: 'linkData', tid: args.tid}, '*');
    // Small delay to allow the dialog to finish setting up before displaying.
    window.setTimeout(function() {
      $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).showModal();
    }, 10);
  } else if (cmd === 'closeDialog') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).close();
  } else if (cmd === 'focusMenu') {
    // Focus the edited tile's menu or the add shortcut tile after closing the
    // custom link edit dialog without saving.
    $(IDS.TILES_IFRAME)
        .contentWindow.postMessage({cmd: 'focusMenu', tid: args.tid}, '*');
  }
}

function showSearchSuggestions() {
  // Inject search suggestions as early as possible to avoid shifting of other
  // elements.
  if (!$('search-suggestions-loader')) {
    var ssScript = document.createElement('script');
    ssScript.id = 'search-suggestions-loader';
    ssScript.src = 'chrome-search://local-ntp/search-suggestions.js';
    ssScript.async = false;
    document.body.appendChild(ssScript);
    ssScript.onload = function() {
      injectSearchSuggestions(search_suggestions);
    };
  }
}


/**
 * Enables Material Design styles for the Most Visited section. Implicitly
 * enables Material Design for the rest of NTP.
 */
function enableMDIcons() {
  $(IDS.MOST_VISITED).classList.add(CLASSES.MATERIAL_DESIGN_ICONS);
  $(IDS.TILES).classList.add(CLASSES.MATERIAL_DESIGN_ICONS);
  animations.addRippleAnimations();
}

/**
 * Prepares the New Tab Page by adding listeners, the most visited pages
 * section, and Google-specific elements for a Google-provided page.
 */
function init() {
  // If an accessibility tool is in use, increase the time for which the
  // "tile was blacklisted" notification is shown.
  if (configData.isAccessibleBrowser) {
    document.body.style.setProperty('--mv-notice-time', '30s');
  }

  // Hide notifications after fade out, so we can't focus on links via keyboard.
  $(IDS.NOTIFICATION).addEventListener('transitionend', (event) => {
    if (event.properyName === 'opacity') {
      hideNotification();
    }
  });

  $(IDS.NOTIFICATION_MESSAGE).textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, KEYCODE.ENTER, onUndo);
  registerKeyHandler(undoLink, KEYCODE.SPACE, onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.ENTER, onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.SPACE, onRestoreAll);
  restoreAllLink.textContent =
      (configData.isGooglePage ?
           configData.translatedStrings.restoreDefaultLinks :
           configData.translatedStrings.restoreThumbnailsShort);

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  $(IDS.NOTIFICATION_CLOSE_BUTTON).addEventListener('click', hideNotification);

  var embeddedSearchApiHandle = window.chrome.embeddedSearch;

  ntpApiHandle = embeddedSearchApiHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  renderTheme();

  var searchboxApiHandle = embeddedSearchApiHandle.searchBox;

  if (configData.isGooglePage) {
    showSearchSuggestions();
    enableMDIcons();

    ntpApiHandle.onaddcustomlinkdone = onAddCustomLinkDone;
    ntpApiHandle.onupdatecustomlinkdone = onUpdateCustomLinkDone;
    ntpApiHandle.ondeletecustomlinkdone = onDeleteCustomLinkDone;

    customBackgrounds.init(showErrorNotification, hideNotification);

    if (configData.removeFakebox) {
      document.body.classList.add(CLASSES.REMOVE_FAKEBOX);
    } else {
      // Set up the fakebox (which only exists on the Google NTP).
      ntpApiHandle.oninputstart = onInputStart;
      ntpApiHandle.oninputcancel = onInputCancel;

      if (ntpApiHandle.isInputInProgress) {
        onInputStart();
      }

      $(IDS.FAKEBOX_TEXT).textContent =
          configData.translatedStrings.searchboxPlaceholder;

      if (!iframesAndVoiceSearchDisabledForTesting) {
        speech.init(
            configData.googleBaseUrl, configData.translatedStrings,
            $(IDS.FAKEBOX_MICROPHONE), searchboxApiHandle);
      }

      // Listener for updating the key capture state.
      document.body.onmousedown = function(event) {
        if (isFakeboxClick(event)) {
          searchboxApiHandle.startCapturingKeyStrokes();
        } else if (isFakeboxFocused()) {
          searchboxApiHandle.stopCapturingKeyStrokes();
        }
      };
      searchboxApiHandle.onkeycapturechange = function() {
        setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      };
      var inputbox = $(IDS.FAKEBOX_INPUT);
      inputbox.onpaste = function(event) {
        event.preventDefault();
        // Send pasted text to Omnibox.
        var text = event.clipboardData.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
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
      utils.disableOutlineOnMouseClick($(IDS.FAKEBOX_MICROPHONE));

      // Update the fakebox style to match the current key capturing state.
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      // Also tell the browser that we're capturing, otherwise it's possible
      // that both fakebox and Omnibox have visible focus at the same time, see
      // crbug.com/792850.
      if (searchboxApiHandle.isKeyCaptureEnabled) {
        searchboxApiHandle.startCapturingKeyStrokes();
      }
    }

    doodles.init();

    $(customBackgrounds.IDS.EDIT_BG_TEXT).textContent =
        configData.translatedStrings.customizeButtonLabel;
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

  if (!iframesAndVoiceSearchDisabledForTesting) {
    createIframes();
  }

  utils.setPlatformClass(document.body);
  utils.disableOutlineOnMouseClick($(customBackgrounds.IDS.EDIT_BG));
  document.body.classList.add(CLASSES.INITED);
}


/**
 * Create the Most Visited and edit custom links iframes.
 */
function createIframes() {
  // Collect arguments for the most visited iframe.
  var args = [];

  var searchboxApiHandle = window.chrome.embeddedSearch.searchBox;

  if (searchboxApiHandle.rtl) {
    args.push('rtl=1');
  }
  if (NTP_DESIGN.numTitleLines > 1) {
    args.push('ntl=' + NTP_DESIGN.numTitleLines);
  }

  args.push(
      'title=' +
      encodeURIComponent(configData.translatedStrings.mostVisitedTitle));
  args.push('removeTooltip=' +
      encodeURIComponent(configData.translatedStrings.removeThumbnailTooltip));

  if (configData.isGooglePage) {
    args.push('enableCustomLinks=1');
    args.push(
        'addLink=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    args.push(
        'addLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.addLinkTooltip));
    args.push(
        'editLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.editLinkTooltip));
  }

  // Create the most visited iframe.
  var iframe = document.createElement('iframe');
  iframe.id = IDS.TILES_IFRAME;
  iframe.name = IDS.TILES_IFRAME;
  iframe.title = configData.translatedStrings.mostVisitedTitle;
  iframe.src = 'chrome-search://most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    sendThemeInfoToMostVisitedIframe();
    reloadTiles();
  };

  if (configData.isGooglePage) {
    // Collect arguments for the edit custom link iframe.
    let clArgs = [];

    if (searchboxApiHandle.rtl) {
      clArgs.push('rtl=1');
    }

    clArgs.push(
        'addTitle=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    clArgs.push(
        'editTitle=' +
        encodeURIComponent(configData.translatedStrings.editLinkTitle));
    clArgs.push(
        'nameField=' +
        encodeURIComponent(configData.translatedStrings.nameField));
    clArgs.push(
        'urlField=' +
        encodeURIComponent(configData.translatedStrings.urlField));
    clArgs.push(
        'linkRemove=' +
        encodeURIComponent(configData.translatedStrings.linkRemove));
    clArgs.push(
        'linkCancel=' +
        encodeURIComponent(configData.translatedStrings.linkCancel));
    clArgs.push(
        'linkDone=' +
        encodeURIComponent(configData.translatedStrings.linkDone));
    clArgs.push(
        'invalidUrl=' +
        encodeURIComponent(configData.translatedStrings.invalidUrl));

    // Create the edit custom link iframe.
    let clIframe = document.createElement('iframe');
    clIframe.id = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.name = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.title = configData.translatedStrings.editLinkTitle;
    clIframe.src = 'chrome-search://most-visited/edit.html?' + clArgs.join('&');
    let clIframeDialog = document.createElement('dialog');
    clIframeDialog.id = IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG;
    clIframeDialog.classList.add(CLASSES.CUSTOMIZE_DIALOG);
    clIframeDialog.appendChild(clIframe);
    document.body.appendChild(clIframeDialog);

    clIframe.onload = () => {
      sendThemeInfoToEditCustomLinkIframe();
    };
  }

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}


/**
 * Injects a middle-slot promo into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectPromo(promo) {
  if (promo.promoHtml == '') {
    return;
  }

  let promoContainer = document.createElement('div');
  promoContainer.id = IDS.PROMO;
  promoContainer.innerHTML += promo.promoHtml;
  $(IDS.NTP_CONTENTS).appendChild(promoContainer);

  if (promo.promoLogUrl) {
    navigator.sendBeacon(promo.promoLogUrl);
  }
}


/**
 * Injects search suggestions into the page. Called *synchronously* with cached
 * data as not to cause shifting of the most visited tiles.
 */
function injectSearchSuggestions(suggestions) {
  if (suggestions.suggestionsHtml === '') {
    return;
  }

  let suggestionsContainer = document.createElement('div');
  suggestionsContainer.id = IDS.SUGGESTIONS;
  suggestionsContainer.style.visibility = 'hidden';
  suggestionsContainer.innerHTML += suggestions.suggestionsHtml;
  $(IDS.USER_CONTENT).insertAdjacentElement('afterbegin', suggestionsContainer);

  let endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(
      document.createTextNode(suggestions.suggestionsEndOfBodyScript));
  document.body.appendChild(endOfBodyScript);
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


return {
  init: init,  // Exposed for testing.
  listen: listen,
  disableIframesAndVoiceSearchForTesting: disableIframesAndVoiceSearchForTesting
};

}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
