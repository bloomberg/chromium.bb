// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview New tab page
 * This is the main code for the new tab page used by touch-enabled Chrome
 * browsers.  For now this is still a prototype.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('ntp', function() {
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
   * If non-null, an info bubble for showing messages to the user. It points at
   * the Most Visited label, and is used to draw more attention to the
   * navigation dot UI.
   * @type {!cr.ui.Bubble|undefined}
   */
  var promoBubble;

  /**
   * If non-null, an bubble confirming that the user has signed into sync. It
   * points at the login status at the top of the page.
   * @type {!cr.ui.Bubble|undefined}
   */
  var loginBubble;

  /**
   * true if |loginBubble| should be shown.
   * @type {boolean}
   */
  var shouldShowLoginBubble = false;

  /**
   * The time when all sections are ready.
   * @type {number|undefined}
   * @private
   */
  var startTime;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * See description for these values in ntp_stats.h.
   * @enum {number}
   */
  var NtpFollowAction = {
    CLICKED_TILE: 11,
    CLICKED_OTHER_NTP_PANE: 12,
    OTHER: 13
  };

  /**
   * Creates a NewTabView object. NewTabView extends PageListView with
   * new tab UI specific logics.
   * @constructor
   * @extends {ntp.PageListView}
   */
  function NewTabView() {
    var pageSwitcherStart;
    var pageSwitcherEnd;
    if (loadTimeData.getValue('showApps')) {
      pageSwitcherStart = /** @type {!ntp.PageSwitcher} */(
          getRequiredElement('page-switcher-start'));
      pageSwitcherEnd = /** @type {!ntp.PageSwitcher} */(
          getRequiredElement('page-switcher-end'));
    }
    this.initialize(getRequiredElement('page-list'),
                    getRequiredElement('dot-list'),
                    getRequiredElement('card-slider-frame'),
                    getRequiredElement('trash'),
                    pageSwitcherStart, pageSwitcherEnd);
  }

  NewTabView.prototype = {
    __proto__: ntp.PageListView.prototype,

    /** @override */
    appendTilePage: function(page, title, titleIsEditable, opt_refNode) {
      ntp.PageListView.prototype.appendTilePage.apply(this, arguments);

      if (promoBubble)
        window.setTimeout(promoBubble.reposition.bind(promoBubble), 0);
    }
  };

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function onLoad() {
    sectionsToWaitFor = 0;
    if (loadTimeData.getBoolean('showApps')) {
      sectionsToWaitFor++;
      if (loadTimeData.getBoolean('showAppLauncherPromo')) {
        $('app-launcher-promo-close-button').addEventListener('click',
            function() { chrome.send('stopShowingAppLauncherPromo'); });
        $('apps-promo-learn-more').addEventListener('click',
            function() { chrome.send('onLearnMore'); });
      }
    }
    measureNavDots();

    // Load the current theme colors.
    themeChanged();

    newTabView = new NewTabView();

    notificationContainer = getRequiredElement('notification-container');
    notificationContainer.addEventListener(
        'webkitTransitionEnd', onNotificationTransitionEnd);

    if (!loadTimeData.getBoolean('showWebStoreIcon')) {
      var webStoreIcon = $('chrome-web-store-link');
      // Not all versions of the NTP have a footer, so this may not exist.
      if (webStoreIcon)
        webStoreIcon.hidden = true;
    } else {
      var webStoreLink = loadTimeData.getString('webStoreLink');
      var url = appendParam(webStoreLink, 'utm_source', 'chrome-ntp-launcher');
      $('chrome-web-store-link').href = url;
      $('chrome-web-store-link').addEventListener('click',
          onChromeWebStoreButtonClick);
    }

    // We need to wait for all the footer menu setup to be completed before
    // we can compute its layout.
    layoutFooter();

    if (loadTimeData.getString('login_status_message')) {
      loginBubble = new cr.ui.Bubble;
      loginBubble.anchorNode = $('login-container');
      loginBubble.arrowLocation = cr.ui.ArrowLocation.TOP_END;
      loginBubble.bubbleAlignment =
          cr.ui.BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE;
      loginBubble.deactivateToDismissDelay = 2000;
      loginBubble.closeButtonVisible = false;

      $('login-status-advanced').onclick = function() {
        chrome.send('showAdvancedLoginUI');
      };
      $('login-status-dismiss').onclick = loginBubble.hide.bind(loginBubble);

      var bubbleContent = $('login-status-bubble-contents');
      loginBubble.content = bubbleContent;

      // The anchor node won't be updated until updateLogin is called so don't
      // show the bubble yet.
      shouldShowLoginBubble = true;
    }

    if (loadTimeData.valueExists('bubblePromoText')) {
      promoBubble = new cr.ui.Bubble;
      promoBubble.anchorNode = getRequiredElement('promo-bubble-anchor');
      promoBubble.arrowLocation = cr.ui.ArrowLocation.BOTTOM_START;
      promoBubble.bubbleAlignment = cr.ui.BubbleAlignment.ENTIRELY_VISIBLE;
      promoBubble.deactivateToDismissDelay = 2000;
      promoBubble.content = parseHtmlSubset(
          loadTimeData.getString('bubblePromoText'), ['BR']);

      var bubbleLink = promoBubble.querySelector('a');
      if (bubbleLink) {
        bubbleLink.addEventListener('click', function(e) {
          chrome.send('bubblePromoLinkClicked');
        });
      }

      promoBubble.handleCloseEvent = function() {
        promoBubble.hide();
        chrome.send('bubblePromoClosed');
      };
      promoBubble.show();
      chrome.send('bubblePromoViewed');
    }

    $('login-container').addEventListener('click', showSyncLoginUI);
    if (loadTimeData.getBoolean('shouldShowSyncLogin'))
      chrome.send('initializeSyncLogin');

    doWhenAllSectionsReady(function() {
      // Tell the slider about the pages.
      newTabView.updateSliderCards();
      // Mark the current page.
      newTabView.cardSlider.currentCardValue.navigationDot.classList.add(
          'selected');

      if (loadTimeData.valueExists('notificationPromoText')) {
        var promoText = loadTimeData.getString('notificationPromoText');
        var tags = ['IMG'];
        var attrs = {
          src: function(node, value) {
            return node.tagName == 'IMG' &&
                   /^data\:image\/(?:png|gif|jpe?g)/.test(value);
          },
        };

        var promo = parseHtmlSubset(promoText, tags, attrs);
        var promoLink = promo.querySelector('a');
        if (promoLink) {
          promoLink.addEventListener('click', function(e) {
            chrome.send('notificationPromoLinkClicked');
          });
        }

        showNotification(promo, [], function() {
          chrome.send('notificationPromoClosed');
        }, 60000);
        chrome.send('notificationPromoViewed');
      }

      cr.dispatchSimpleEvent(document, 'ntpLoaded', true, true);
      document.documentElement.classList.remove('starting-up');

      startTime = Date.now();
    });
  }

  /**
   * Launches the chrome web store app with the chrome-ntp-launcher
   * source.
   * @param {Event} e The click event.
   */
  function onChromeWebStoreButtonClick(e) {
    chrome.send('recordAppLaunchByURL',
                [encodeURIComponent(this.href),
                 ntp.APP_LAUNCH.NTP_WEBSTORE_FOOTER]);
  }

  /**
   * The number of sections to wait on.
   * @type {number}
   */
  var sectionsToWaitFor = -1;

  /**
   * Queued callbacks which lie in wait for all sections to be ready.
   * @type {Array}
   */
  var readyCallbacks = [];

  /**
   * Fired as each section of pages becomes ready.
   * @param {Event} e Each page's synthetic DOM event.
   */
  document.addEventListener('sectionready', function(e) {
    if (--sectionsToWaitFor <= 0) {
      while (readyCallbacks.length) {
        readyCallbacks.shift()();
      }
    }
  });

  /**
   * This is used to simulate a fire-once event (i.e. $(document).ready() in
   * jQuery or Y.on('domready') in YUI. If all sections are ready, the callback
   * is fired right away. If all pages are not ready yet, the function is queued
   * for later execution.
   * @param {Function} callback The work to be done when ready.
   */
  function doWhenAllSectionsReady(callback) {
    assert(typeof callback == 'function');
    if (sectionsToWaitFor > 0)
      readyCallbacks.push(callback);
    else
      window.setTimeout(callback, 0);  // Do soon after, but asynchronously.
  }

  /**
   * Measure the width of a nav dot with a given title.
   * @param {string} id The loadTimeData ID of the desired title.
   * @return {number} The width of the nav dot.
   */
  function measureNavDot(id) {
    var measuringDiv = $('fontMeasuringDiv');
    measuringDiv.textContent = loadTimeData.getString(id);
    // The 4 is for border and padding.
    return Math.max(measuringDiv.clientWidth * 1.15 + 4, 80);
  }

  /**
   * Fills in an invisible div with the longest dot title string so that
   * its length may be measured and the nav dots sized accordingly.
   */
  function measureNavDots() {
    var styleElement = document.createElement('style');
    styleElement.type = 'text/css';
    // max-width is used because if we run out of space, the nav dots will be
    // shrunk.
    var pxWidth = measureNavDot('appDefaultPageName');
    styleElement.textContent = '.dot { max-width: ' + pxWidth + 'px; }';
    document.querySelector('head').appendChild(styleElement);
  }

  /**
   * Layout the footer so that the nav dots stay centered.
   */
  function layoutFooter() {
    // We need the image to be loaded.
    var logo = $('logo-img');
    var logoImg = logo.querySelector('img');
    if (!logoImg.complete) {
      logoImg.onload = layoutFooter;
      return;
    }

    var menu = $('footer-menu-container');
    if (menu.clientWidth > logoImg.width)
      logo.style.WebkitFlex = '0 1 ' + menu.clientWidth + 'px';
    else
      menu.style.WebkitFlex = '0 1 ' + logoImg.width + 'px';
  }

  /**
   * Called when the theme has changed.
   * @param {Object=} opt_themeData Not used; only exists to match equivalent
   *     function in incognito NTP.
   */
  function themeChanged(opt_themeData) {
    $('themecss').href = 'chrome://theme/css/new_tab_theme.css?' + Date.now();
  }

  function setBookmarkBarAttached(attached) {
    document.documentElement.setAttribute('bookmarkbarattached', attached);
  }

  /**
   * Timeout ID.
   * @type {number}
   */
  var notificationTimeout = 0;

  /**
   * Shows the notification bubble.
   * @param {string|Node} message The notification message or node to use as
   *     message.
   * @param {Array<{text: string, action: function()}>} links An array of
   *     records describing the links in the notification. Each record should
   *     have a 'text' attribute (the display string) and an 'action' attribute
   *     (a function to run when the link is activated).
   * @param {Function=} opt_closeHandler The callback invoked if the user
   *     manually dismisses the notification.
   * @param {number=} opt_timeout
   */
  function showNotification(message, links, opt_closeHandler, opt_timeout) {
    window.clearTimeout(notificationTimeout);

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
      var link = new ActionLink;
      link.textContent = links[i].text;
      link.action = links[i].action;
      link.onclick = function() {
        this.action();
        hideNotification();
      };
      linksBin.appendChild(link);
    }

    function closeFunc(e) {
      if (opt_closeHandler)
        opt_closeHandler();
      hideNotification();
    }

    document.querySelector('#notification button').onclick = closeFunc;
    document.addEventListener('dragstart', closeFunc);

    notificationContainer.hidden = false;
    showNotificationOnCurrentPage();

    newTabView.cardSlider.frame.addEventListener(
        'cardSlider:card_change_ended', onCardChangeEnded);

    var timeout = opt_timeout || 10000;
    notificationTimeout = window.setTimeout(hideNotification, timeout);
  }

  /**
   * Hide the notification bubble.
   */
  function hideNotification() {
    notificationContainer.classList.add('inactive');

    newTabView.cardSlider.frame.removeEventListener(
        'cardSlider:card_change_ended', onCardChangeEnded);
  }

  /**
   * Happens when 1 or more consecutive card changes end.
   * @param {Event} e The cardSlider:card_change_ended event.
   */
  function onCardChangeEnded(e) {
    // If we ended on the same page as we started, ignore.
    if (newTabView.cardSlider.currentCardValue.notification)
      return;

    // Hide the notification the old page.
    notificationContainer.classList.add('card-changed');

    showNotificationOnCurrentPage();
  }

  /**
   * Move and show the notification on the current page.
   */
  function showNotificationOnCurrentPage() {
    var page = newTabView.cardSlider.currentCardValue;
    doWhenAllSectionsReady(function() {
      if (page != newTabView.cardSlider.currentCardValue)
        return;

      // NOTE: This moves the notification to inside of the current page.
      page.notification = notificationContainer;

      // Reveal the notification and instruct it to hide itself if ignored.
      notificationContainer.classList.remove('inactive');

      // Gives the browser time to apply this rule before we remove it (causing
      // a transition).
      window.setTimeout(function() {
        notificationContainer.classList.remove('card-changed');
      }, 0);
    });
  }

  /**
   * When done fading out, set hidden to true so the notification can't be
   * tabbed to or clicked.
   * @param {Event} e The webkitTransitionEnd event.
   */
  function onNotificationTransitionEnd(e) {
    if (notificationContainer.classList.contains('inactive'))
      notificationContainer.hidden = true;
  }

  /**
   * Set the dominant color for a node. This will be called in response to
   * getFaviconDominantColor. The node represented by |id| better have a setter
   * for stripeColor.
   * @param {string} id The ID of a node.
   * @param {string} color The color represented as a CSS string.
   */
  function setFaviconDominantColor(id, color) {
    var node = $(id);
    if (node)
      node.stripeColor = color;
  }

  /**
   * Updates the text displayed in the login container. If there is no text then
   * the login container is hidden.
   * @param {string} loginHeader The first line of text.
   * @param {string} loginSubHeader The second line of text.
   * @param {string} iconURL The url for the login status icon. If this is null
        then the login status icon is hidden.
   * @param {boolean} isUserSignedIn Indicates if the user is signed in or not.
   */
  function updateLogin(loginHeader, loginSubHeader, iconURL, isUserSignedIn) {
    /** @const */ var showLogin = loginHeader || loginSubHeader;

    $('login-container').hidden = !showLogin;
    $('login-container').classList.toggle('signed-in', isUserSignedIn);
    $('card-slider-frame').classList.toggle('showing-login-area', !!showLogin);

    if (showLogin) {
      // TODO(dbeam): we should use .textContent instead to mitigate XSS.
      $('login-status-header').innerHTML = loginHeader;
      $('login-status-sub-header').innerHTML = loginSubHeader;

      var headerContainer = $('login-status-header-container');
      headerContainer.classList.toggle('login-status-icon', !!iconURL);
      headerContainer.style.backgroundImage = iconURL ? url(iconURL) : 'none';
    }

    if (shouldShowLoginBubble) {
      window.setTimeout(loginBubble.show.bind(loginBubble), 0);
      chrome.send('loginMessageSeen');
      shouldShowLoginBubble = false;
    } else if (loginBubble) {
      loginBubble.reposition();
    }
  }

  /**
   * Show the sync login UI.
   * @param {Event} e The click event.
   */
  function showSyncLoginUI(e) {
    var rect = e.currentTarget.getBoundingClientRect();
    chrome.send('showSyncLoginUI',
                [rect.left, rect.top, rect.width, rect.height]);
  }

  /**
   * Wrappers to forward the callback to corresponding PageListView member.
   */

  /**
   * Called by chrome when a new app has been added to chrome or has been
   * enabled if previously disabled.
   * @param {Object} appData A data structure full of relevant information for
   *     the app.
   * @param {boolean=} opt_highlight Whether the app about to be added should
   *     be highlighted.
   */
  function appAdded(appData, opt_highlight) {
    newTabView.appAdded(appData, opt_highlight);
  }

  /**
   * Called by chrome when an app has changed positions.
   * @param {Object} appData The data for the app. This contains page and
   *     position indices.
   */
  function appMoved(appData) {
    newTabView.appMoved(appData);
  }

  /**
   * Called by chrome when an existing app has been disabled or
   * removed/uninstalled from chrome.
   * @param {Object} appData A data structure full of relevant information for
   *     the app.
   * @param {boolean} isUninstall True if the app is being uninstalled;
   *     false if the app is being disabled.
   * @param {boolean} fromPage True if the removal was from the current page.
   */
  function appRemoved(appData, isUninstall, fromPage) {
    newTabView.appRemoved(appData, isUninstall, fromPage);
  }

  /**
   * Callback invoked by chrome whenever an app preference changes.
   * @param {Object} data An object with all the data on available
   *     applications.
   */
  function appsPrefChangeCallback(data) {
    newTabView.appsPrefChangedCallback(data);
  }

  /**
   * Callback invoked by chrome whenever the app launcher promo pref changes.
   * @param {boolean} show Identifies if we should show or hide the promo.
   */
  function appLauncherPromoPrefChangeCallback(show) {
    newTabView.appLauncherPromoPrefChangeCallback(show);
  }

  /**
   * Called whenever tiles should be re-arranging themselves out of the way
   * of a moving or insert tile.
   */
  function enterRearrangeMode() {
    newTabView.enterRearrangeMode();
  }

  /**
   * Callback invoked by chrome with the apps available.
   *
   * Note that calls to this function can occur at any time, not just in
   * response to a getApps request. For example, when a user
   * installs/uninstalls an app on another synchronized devices.
   * @param {Object} data An object with all the data on available
   *        applications.
   */
  function getAppsCallback(data) {
    newTabView.getAppsCallback(data);
  }

  /**
   * Return the index of the given apps page.
   * @param {ntp.AppsPage} page The AppsPage we wish to find.
   * @return {number} The index of |page| or -1 if it is not in the collection.
   */
  function getAppsPageIndex(page) {
    return newTabView.getAppsPageIndex(page);
  }

  function getCardSlider() {
    return newTabView.cardSlider;
  }

  /**
   * Invoked whenever some app is released
   */
  function leaveRearrangeMode() {
    newTabView.leaveRearrangeMode();
  }

  /**
   * Save the name of an apps page.
   * Store the apps page name into the preferences store.
   * @param {ntp.AppsPage} appPage The app page for which we wish to save.
   * @param {string} name The name of the page.
   */
  function saveAppPageName(appPage, name) {
    newTabView.saveAppPageName(appPage, name);
  }

  function setAppToBeHighlighted(appId) {
    newTabView.highlightAppId = appId;
  }

  // Return an object with all the exports
  return {
    appAdded: appAdded,
    appMoved: appMoved,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    appLauncherPromoPrefChangeCallback: appLauncherPromoPrefChangeCallback,
    enterRearrangeMode: enterRearrangeMode,
    getAppsCallback: getAppsCallback,
    getAppsPageIndex: getAppsPageIndex,
    getCardSlider: getCardSlider,
    onLoad: onLoad,
    leaveRearrangeMode: leaveRearrangeMode,
    NtpFollowAction: NtpFollowAction,
    saveAppPageName: saveAppPageName,
    setAppToBeHighlighted: setAppToBeHighlighted,
    setBookmarkBarAttached: setBookmarkBarAttached,
    setFaviconDominantColor: setFaviconDominantColor,
    showNotification: showNotification,
    themeChanged: themeChanged,
    updateLogin: updateLogin
  };
});

document.addEventListener('DOMContentLoaded', ntp.onLoad);

var toCssPx = cr.ui.toCssPx;
