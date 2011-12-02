// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview New tab page
 * This is the main code for the new tab page used by touch-enabled Chrome
 * browsers.  For now this is still a prototype.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('ntp4', function() {
  'use strict';

  /**
   * NewTabView instance.
   * @type {!Object|undefined}
   */
  var newTabView;

  /**
   * The 'notification-container' element.
   * @type {!Element|undefined}
   */
  var notificationContainer;

  /**
   * Object for accessing localized strings.
   * @type {!LocalStrings}
   */
  var localStrings = new LocalStrings;

  /**
   * If non-null, an info bubble for showing messages to the user. It points at
   * the Most Visited label, and is used to draw more attention to the
   * navigation dot UI.
   * @type {!Element|undefined}
   */
  var infoBubble;

  /**
   * If non-null, an bubble confirming that the user has signed into sync. It
   * points at the login status at the top of the page.
   * @type {!Element|undefined}
   */
  var loginBubble;

  /**
   * true if |loginBubble| should be shown.
   * @type {Boolean}
   */
  var shouldShowLoginBubble = false;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * Creates a NewTabView object. NewTabView extends PageListView with
   * new tab UI specific logics.
   * @constructor
   * @extends {PageListView}
   */
  function NewTabView() {
    this.initialize(getRequiredElement('page-list'),
                    getRequiredElement('dot-list'),
                    getRequiredElement('card-slider-frame'),
                    getRequiredElement('trash'),
                    getRequiredElement('page-switcher-start'),
                    getRequiredElement('page-switcher-end'));
  }

  NewTabView.prototype = {
    __proto__: ntp4.PageListView.prototype,

    /** @inheritDoc */
    appendTilePage: function(page, title, titleIsEditable, opt_refNode) {
      ntp4.PageListView.prototype.appendTilePage.apply(this, arguments);

      if (infoBubble)
        window.setTimeout(infoBubble.reposition.bind(infoBubble), 0);
    }
  };

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function onLoad() {
    cr.enablePlatformSpecificCSSRules();

    // Load the current theme colors.
    themeChanged();

    newTabView = new NewTabView();

    notificationContainer = getRequiredElement('notification-container');
    notificationContainer.addEventListener(
        'webkitTransitionEnd', onNotificationTransitionEnd);

    cr.ui.decorate($('recently-closed-menu-button'), ntp4.RecentMenuButton);
    chrome.send('getRecentlyClosedTabs');

    newTabView.appendTilePage(new ntp4.MostVisitedPage(),
                              localStrings.getString('mostvisited'),
                              false);
    chrome.send('getMostVisited');

    if (localStrings.getString('login_status_message')) {
      loginBubble = new cr.ui.Bubble;
      loginBubble.anchorNode = $('login-container');
      loginBubble.setArrowLocation(cr.ui.ArrowLocation.TOP_END);
      loginBubble.bubbleAlignment =
          cr.ui.BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE;
      loginBubble.deactivateToDismissDelay = 2000;
      loginBubble.setCloseButtonVisible(false);

      $('login-status-learn-more').href =
          localStrings.getString('login_status_url');
      $('login-status-advanced').onclick = function() {
        chrome.send('showAdvancedLoginUI');
      }
      $('login-status-dismiss').onclick = loginBubble.hide.bind(loginBubble);

      var bubbleContent = $('login-status-bubble-contents');
      loginBubble.content = bubbleContent;
      bubbleContent.hidden = false;

      // The anchor node won't be updated until updateLogin is called so don't
      // show the bubble yet.
      shouldShowLoginBubble = true;
    } else if (localStrings.getString('ntp4_intro_message')) {
      infoBubble = new cr.ui.Bubble;
      infoBubble.anchorNode = newTabView.mostVisitedPage.navigationDot;
      infoBubble.setArrowLocation(cr.ui.ArrowLocation.BOTTOM_START);
      infoBubble.handleCloseEvent = function() {
        this.hide();
        chrome.send('introMessageDismissed');
      }

      var bubbleContent = $('ntp4-intro-bubble-contents');
      infoBubble.content = bubbleContent;
      bubbleContent.hidden = false;

      var learnMoreLink = infoBubble.querySelector('a');
      learnMoreLink.href = localStrings.getString('ntp4_intro_url');
      learnMoreLink.onclick = infoBubble.hide.bind(infoBubble);

      infoBubble.show();
    }

    var serverpromo = localStrings.getString('serverpromo');
    if (serverpromo) {
      showNotification(parseHtmlSubset(serverpromo), [], function() {
        chrome.send('closeNotificationPromo');
      }, 60000);
      chrome.send('notificationPromoViewed');
    }

    var loginContainer = getRequiredElement('login-container');
    loginContainer.addEventListener('click', function() {
      var rect = loginContainer.getBoundingClientRect();
      chrome.send('showSyncLoginUI',
                  [rect.left, rect.top, rect.width, rect.height]);
    });
    chrome.send('initializeSyncLogin');
  }

  // TODO(estade): rename newtab.css to new_tab_theme.css
  function themeChanged(hasAttribution) {
    $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
    if (typeof hasAttribution != 'undefined')
      document.documentElement.setAttribute('hasattribution', hasAttribution);
    updateLogo();
    updateAttribution();
  }

  /**
   * Sets the proper image for the logo at the bottom left.
   */
  function updateLogo() {
    var imageId = 'IDR_PRODUCT_LOGO';
    if (document.documentElement.getAttribute('customlogo') == 'true')
      imageId = 'IDR_CUSTOM_PRODUCT_LOGO';

    $('logo-img').src = 'chrome://theme/' + imageId + '?' + Date.now();
  }

  /**
   * Attributes the attribution image at the bottom left.
   */
  function updateAttribution() {
    var attribution = $('attribution');
    if (document.documentElement.getAttribute('hasattribution') == 'true') {
      $('attribution-img').src =
          'chrome://theme/IDR_THEME_NTP_ATTRIBUTION?' + Date.now();
      attribution.hidden = false;
    } else {
      attribution.hidden = true;
    }
  }

  /**
   * Timeout ID.
   * @type {number}
   */
  var notificationTimeout_ = 0;

  /**
   * Shows the notification bubble.
   * @param {string|Node} message The notification message or node to use as
   *     message.
   * @param {Array.<{text: string, action: function()}>} links An array of
   *     records describing the links in the notification. Each record should
   *     have a 'text' attribute (the display string) and an 'action' attribute
   *     (a function to run when the link is activated).
   * @param {Function} opt_closeHandler The callback invoked if the user
   *     manually dismisses the notification.
   */
  function showNotification(message, links, opt_closeHandler, opt_timeout) {
    window.clearTimeout(notificationTimeout_);

    var span = document.querySelector('#notification > span');
    if (typeof message == 'string') {
      span.textContent = message;
    } else {
      span.textContent = '';  // Remove all children.
      span.appendChild(message);
    }

    var linksBin = $('notificationLinks');
    linksBin.textContent = '';
    for (var i = 0; i < links.length; i++) {
      var link = linksBin.ownerDocument.createElement('div');
      link.textContent = links[i].text;
      link.action = links[i].action;
      link.onclick = function() {
        this.action();
        hideNotification();
      }
      link.setAttribute('role', 'button');
      link.setAttribute('tabindex', 0);
      link.className = 'linkButton';
      linksBin.appendChild(link);
    }

    document.querySelector('#notification button').onclick = function(e) {
      if (opt_closeHandler)
        opt_closeHandler();
      hideNotification();
    };

    var timeout = opt_timeout || 10000;
    notificationContainer.hidden = false;
    notificationContainer.classList.remove('inactive');
    notificationTimeout_ = window.setTimeout(hideNotification, timeout);
  }

  /**
   * Hide the notification bubble.
   */
  function hideNotification() {
    notificationContainer.classList.add('inactive');
  }

  /**
   * When done fading out, set hidden to true so the notification can't be
   * tabbed to or clicked.
   */
  function onNotificationTransitionEnd(e) {
    if (notificationContainer.classList.contains('inactive'));
      notificationContainer.hidden = true;
  }

  function setRecentlyClosedTabs(dataItems) {
    $('recently-closed-menu-button').dataItems = dataItems;
  }

  function setMostVisitedPages(data, hasBlacklistedUrls) {
    newTabView.mostVisitedPage.data = data;
  }

  /**
   * Set the dominant color for a node. This will be called in response to
   * getFaviconDominantColor. The node represented by |id| better have a setter
   * for stripeColor.
   * @param {string} id The ID of a node.
   * @param {string} color The color represented as a CSS string.
   */
  function setStripeColor(id, color) {
    var node = $(id);
    if (node)
      node.stripeColor = color;
  }

  /**
   * Updates the text displayed in the login container. If there is no text then
   * the login container is hidden.
   * @param {string} loginHeader The first line of text.
   * @param {string} loginSubHeader The second line of text.
   */
  function updateLogin(loginHeader, loginSubHeader) {
    if (loginHeader || loginSubHeader) {
      $('login-container').hidden = false;
      $('login-status-header').innerHTML = loginHeader;
      $('login-status-sub-header').innerHTML = loginSubHeader;
      $('card-slider-frame').classList.add('showing-login-area');
    } else {
      $('login-container').hidden = true;
      $('card-slider-frame').classList.remove('showing-login-area');
    }
    if (shouldShowLoginBubble) {
      window.setTimeout(loginBubble.show.bind(loginBubble), 0);
      chrome.send('loginMessageSeen');
      shouldShowLoginBubble = false;
    }
  }

  /**
   * Wrappers to forward the callback to corresponding PageListView member.
   */
  function appAdded(appData, opt_highlight) {
    newTabView.appAdded(appData, opt_highlight);
  }

  function appRemoved(appData, isUninstall) {
    newTabView.appRemoved(appData, isUninstall);
  }

  function appsPrefChangeCallback(data) {
    newTabView.appsPrefChangedCallback(data);
  }

  function enterRearrangeMode() {
    newTabView.enterRearrangeMode();
  }

  function getAppsCallback(data) {
    newTabView.getAppsCallback(data);
  }

  function getAppsPageIndex(page) {
    return newTabView.getAppsPageIndex(page);
  }

  function getCardSlider() {
    return newTabView.cardSlider;
  }

  function leaveRearrangeMode(e) {
    newTabView.leaveRearrangeMode(e);
  }

  function saveAppPageName(appPage, name) {
    newTabView.saveAppPageName(appPage, name);
  }

  function setAppToBeHighlighted(appId) {
    newTabView.highlightAppId = appId;
  }

  // Return an object with all the exports
  return {
    appAdded: appAdded,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    enterRearrangeMode: enterRearrangeMode,
    getAppsCallback: getAppsCallback,
    getAppsPageIndex: getAppsPageIndex,
    getCardSlider: getCardSlider,
    onLoad: onLoad,
    leaveRearrangeMode: leaveRearrangeMode,
    saveAppPageName: saveAppPageName,
    setAppToBeHighlighted: setAppToBeHighlighted,
    setMostVisitedPages: setMostVisitedPages,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setStripeColor: setStripeColor,
    showNotification: showNotification,
    themeChanged: themeChanged,
    updateLogin: updateLogin
  };
});

// publish ntp globals
// TODO(estade): update the content handlers to use ntp namespace instead of
// making these global.
var getAppsCallback = ntp4.getAppsCallback;
var appsPrefChangeCallback = ntp4.appsPrefChangeCallback;
var themeChanged = ntp4.themeChanged;
var recentlyClosedTabs = ntp4.setRecentlyClosedTabs;
var setMostVisitedPages = ntp4.setMostVisitedPages;
var updateLogin = ntp4.updateLogin;

document.addEventListener('DOMContentLoaded', ntp4.onLoad);
