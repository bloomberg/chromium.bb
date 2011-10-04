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
   * The CardSlider object to use for changing app pages.
   * @type {CardSlider|undefined}
   */
  var cardSlider;

  /**
   * The 'page-list' element.
   * @type {!Element|undefined}
   */
  var pageList;

  /**
   * A list of all 'tile-page' elements.
   * @type {!NodeList|undefined}
   */
  var tilePages;

  /**
   * The Most Visited page.
   * @type {!Element|undefined}
   */
  var mostVisitedPage;

  /**
   * A list of all 'apps-page' elements.
   * @type {!NodeList|undefined}
   */
  var appsPages;

  /**
   * The Bookmarks page.
   * @type {!Element|undefined}
   */
  var bookmarksPage;

  /**
   * The 'dots-list' element.
   * @type {!Element|undefined}
   */
  var dotList;

  /**
   * The 'notification-container' element.
   * @type {!Element|undefined}
   */
  var notificationContainer;

  /**
   * The left and right paging buttons.
   * @type {!Element|undefined}
   */
  var pageSwitcherStart;
  var pageSwitcherEnd;

  /**
   * The 'trash' element.  Note that technically this is unnecessary,
   * JavaScript creates the object for us based on the id.  But I don't want
   * to rely on the ID being the same, and JSCompiler doesn't know about it.
   * @type {!Element|undefined}
   */
  var trash;

  /**
   * The type of page that is currently shown. The value is a numerical ID.
   * @type {number}
   */
  var shownPage = 0;

  /**
   * The index of the page that is currently shown, within the page type.
   * For example if the third Apps page is showing, this will be 2.
   * @type {number}
   */
  var shownPageIndex = 0;

  /**
   * EventTracker for managing event listeners for page events.
   * @type {!EventTracker}
   */
  var eventTracker = new EventTracker;

  /**
   * Object for accessing localized strings.
   * @type {!LocalStrings}
   */
  var localStrings = new LocalStrings;

  /**
   * If non-null, this is the ID of the app to highlight to the user the next
   * time getAppsCallback runs. "Highlight" in this case means to switch to
   * the page and run the new tile animation.
   * @type {String}
   */
  var highlightAppId = null;

  /**
   * If non-null, an info bubble for showing messages to the user. It points at
   * the Most Visited label, and is used to draw more attention to the
   * navigation dot UI.
   * @type {!Element|undefined}
   */
  var infoBubble;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function initialize() {
    cr.enablePlatformSpecificCSSRules();

    // Load the current theme colors.
    themeChanged();

    dotList = getRequiredElement('dot-list');
    pageList = getRequiredElement('page-list');
    trash = getRequiredElement('trash');
    new ntp4.Trash(trash);

    shownPage = templateData['shown_page_type'];
    shownPageIndex = templateData['shown_page_index'];

    // Request data on the apps so we can fill them in.
    // Note that this is kicked off asynchronously.  'getAppsCallback' will be
    // invoked at some point after this function returns.
    chrome.send('getApps');

    // Prevent touch events from triggering any sort of native scrolling
    document.addEventListener('touchmove', function(e) {
      e.preventDefault();
    }, true);

    tilePages = pageList.getElementsByClassName('tile-page');
    appsPages = pageList.getElementsByClassName('apps-page');

    pageSwitcherStart = getRequiredElement('page-switcher-start');
    ntp4.initializePageSwitcher(pageSwitcherStart);
    pageSwitcherEnd = getRequiredElement('page-switcher-end');
    ntp4.initializePageSwitcher(pageSwitcherEnd);

    notificationContainer = getRequiredElement('notification-container');
    notificationContainer.addEventListener(
        'webkitTransitionEnd', onNotificationTransitionEnd);

    // Initialize the cardSlider without any cards at the moment
    var sliderFrame = getRequiredElement('card-slider-frame');
    cardSlider = new CardSlider(sliderFrame, pageList, sliderFrame.offsetWidth);
    cardSlider.initialize();

    // Ensure the slider is resized appropriately with the window
    window.addEventListener('resize', function() {
      cardSlider.resize(sliderFrame.offsetWidth);
      updatePageSwitchers();
    });

    // Handle the page being changed
    pageList.addEventListener(
        CardSlider.EventType.CARD_CHANGED,
        cardChangedHandler);

    cr.ui.decorate($('recently-closed-menu-button'), ntp4.RecentMenuButton);
    chrome.send('getRecentlyClosedTabs');

    mostVisitedPage = new ntp4.MostVisitedPage();
    appendTilePage(mostVisitedPage, localStrings.getString('mostvisited'));
    chrome.send('getMostVisited');

    if (localStrings.getString('ntp4_intro_message')) {
      infoBubble = new cr.ui.Bubble;
      infoBubble.anchorNode = mostVisitedPage.navigationDot;
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
      chrome.send('introMessageSeen');
    }

    var bookmarkFeatures = localStrings.getString('bookmark_features');
    if (bookmarkFeatures == 'true') {
      bookmarksPage = new ntp4.BookmarksPage();
      appendTilePage(bookmarksPage, localStrings.getString('bookmarksPage'));
      chrome.send('getBookmarksData');
    }

    var serverpromo = localStrings.getString('serverpromo');
    if (serverpromo) {
      showNotification(parseHtmlSubset(serverpromo), [], function() {
        chrome.send('closeNotificationPromo');
      }, 60000);
      chrome.send('notificationPromoViewed');
    }
  }

  /**
   * Simple common assertion API
   * @param {*} condition The condition to test.  Note that this may be used to
   *     test whether a value is defined or not, and we don't want to force a
   *     cast to Boolean.
   * @param {string=} opt_message A message to use in any error.
   */
  function assert(condition, opt_message) {
    'use strict';
    if (!condition) {
      var msg = 'Assertion failed';
      if (opt_message)
        msg = msg + ': ' + opt_message;
      throw new Error(msg);
    }
  }

  /**
   * Get an element that's known to exist by its ID. We use this instead of just
   * calling getElementById and not checking the result because this lets us
   * satisfy the JSCompiler type system.
   * @param {string} id The identifier name.
   * @return {!Element} the Element.
   */
  function getRequiredElement(id) {
    var element = document.getElementById(id);
    assert(element, 'Missing required element: ' + id);
    return element;
  }

  /**
   * Callback invoked by chrome with the apps available.
   *
   * Note that calls to this function can occur at any time, not just in
   * response to a getApps request. For example, when a user installs/uninstalls
   * an app on another synchronized devices.
   * @param {Object} data An object with all the data on available
   *        applications.
   */
  function getAppsCallback(data) {
    var startTime = Date.now();

    // Clear any existing apps pages and dots.
    // TODO(rbyers): It might be nice to preserve animation of dots after an
    // uninstall. Could we re-use the existing page and dot elements?  It seems
    // unfortunate to have Chrome send us the entire apps list after an
    // uninstall.
    while (appsPages.length > 0) {
      var page = appsPages[0];
      var dot = page.navigationDot;

      eventTracker.remove(page);
      page.tearDown();
      page.parentNode.removeChild(page);
      dot.parentNode.removeChild(dot);
    }

    // Get the array of apps and add any special synthesized entries
    var apps = data.apps;

    // Get a list of page names
    var pageNames = data.appPageNames;

    function stringListIsEmpty(list) {
      for (var i = 0; i < list.length; i++) {
        if (list[i])
          return false;
      }
      return true;
    }

    if (!pageNames || stringListIsEmpty(pageNames))
      pageNames = [localStrings.getString('appDefaultPageName')];

    // Sort by launch index
    apps.sort(function(a, b) {
      return a.app_launch_index - b.app_launch_index;
    });

    // An app to animate (in case it was just installed).
    var highlightApp;

    // Add the apps, creating pages as necessary
    for (var i = 0; i < apps.length; i++) {
      var app = apps[i];
      var pageIndex = (app.page_index || 0);
      while (pageIndex >= appsPages.length) {
        var pageName = '';
        if (appsPages.length < pageNames.length)
          pageName = pageNames[appsPages.length];

        var origPageCount = appsPages.length;
        appendTilePage(new ntp4.AppsPage(), pageName, bookmarksPage);
        // Confirm that appsPages is a live object, updated when a new page is
        // added (otherwise we'd have an infinite loop)
        assert(appsPages.length == origPageCount + 1, 'expected new page');
      }

      if (app.id == highlightAppId)
        highlightApp = app;
      else
        appsPages[pageIndex].appendApp(app);
    }

    ntp4.AppsPage.setPromo(data.showPromo ? data : null);

    // Tell the slider about the pages.
    updateSliderCards();

    if (highlightApp)
      appAdded(highlightApp, true);

    // Mark the current page.
    cardSlider.currentCardValue.navigationDot.classList.add('selected');
    logEvent('apps.layout: ' + (Date.now() - startTime));

    document.documentElement.classList.remove('starting-up');
  }

  /**
   * Called by chrome when a new app has been added to chrome or has been
   * enabled if previously disabled.
   * @param {Object} appData A data structure full of relevant information for
   *     the app.
   */
  function appAdded(appData, opt_highlight) {
    if (appData.id == highlightAppId) {
      opt_highlight = true;
      highlightAppId = null;
    }

    var pageIndex = appData.page_index || 0;

    if (pageIndex >= appsPages.length) {
      while (pageIndex >= appsPages.length) {
        appendTilePage(new ntp4.AppsPage(), '', bookmarksPage);
      }
      updateSliderCards();
    }

    var page = appsPages[pageIndex];
    var app = $(appData.id);
    if (app)
      app.replaceAppData(appData);
    else
      page.appendApp(appData, opt_highlight);
  }

  /**
   * Sets that an app should be highlighted if it is added. Called right before
   * appAdded for new installs.
   */
  function setAppToBeHighlighted(appId) {
    highlightAppId = appId;
  }

  /**
   * Called by chrome when an existing app has been disabled or
   * removed/uninstalled from chrome.
   * @param {Object} appData A data structure full of relevant information for
   *     the app.
   * @param {boolean} isUninstall True if the app is being uninstalled;
   *     false if the app is being disabled.
   */
  function appRemoved(appData, isUninstall) {
    var app = $(appData.id);
    assert(app, 'trying to remove an app that doesn\'t exist');

    if (!isUninstall)
      app.replaceAppData(appData);
    else
      app.remove();
  }

  /**
   * Given a theme resource name, construct a URL for it.
   * @param {string} resourceName The name of the resource.
   * @return {string} A url which can be used to load the resource.
   */
  function getThemeUrl(resourceName) {
    return 'chrome://theme/' + resourceName;
  }

  /**
   * Callback invoked by chrome whenever an app preference changes.
   * @param {Object} data An object with all the data on available
   *     applications.
   */
  function appsPrefChangeCallback(data) {
    for (var i = 0; i < data.apps.length; ++i) {
      $(data.apps[i].id).appData = data.apps[i];
    }

    // Set the App dot names. Skip the first and last dots (Most Visited and
    // Bookmarks).
    var dots = dotList.getElementsByClassName('dot');
    // TODO(csilv): Remove this calcluation if/when we remove the flag for
    // for the bookmarks page.
    var length = bookmarksPage ? dots.length - 2 : dots.Length - 1;
    for (var i = 1; i < length; ++i) {
      dots[i].displayTitle = data.appPageNames[i - 1] || '';
    }
  }

  /**
   * Listener for offline status change events. Updates apps that are
   * not offline-enabled to be grayscale if the browser is offline.
   */
  function updateOfflineEnabledApps() {
    var apps = document.querySelectorAll('.app');
    for (var i = 0; i < apps.length; ++i) {
      if (apps[i].appData.enabled && !apps[i].appData.offline_enabled) {
        apps[i].setIcon();
        apps[i].loadIcon();
      }
    }
  }

  function getCardSlider() {
    return cardSlider;
  }

  /**
   * Invoked whenever the pages in apps-page-list have changed so that
   * the Slider knows about the new elements.
   */
  function updateSliderCards() {
    var pageNo = Math.min(cardSlider.currentCard, tilePages.length - 1);
    cardSlider.setCards(Array.prototype.slice.call(tilePages), pageNo);
    switch (shownPage) {
      case templateData['apps_page_id']:
        cardSlider.selectCardByValue(
            appsPages[Math.min(shownPageIndex, appsPages.length - 1)]);
        break;
      case templateData['bookmarks_page_id']:
        if (bookmarksPage)
          cardSlider.selectCardByValue(bookmarksPage);
        break;
      case templateData['most_visited_page_id']:
        cardSlider.selectCardByValue(mostVisitedPage);
        break;
    }
  }

  /**
   * Appends a tile page (for bookmarks or most visited).
   *
   * @param {TilePage} page The page element.
   * @param {string} title The title of the tile page.
   * @param {TilePage} refNode Optional reference node to insert in front of.
   * When refNode is falsey, |page| will just be appended to the end of the
   * page list.
   */
  function appendTilePage(page, title, refNode) {
    // When refNode is falsey, insertBefore acts just like appendChild.
    pageList.insertBefore(page, refNode);

    // If we're appending an AppsPage and it's a temporary page, animate it.
    var animate = page instanceof ntp4.AppsPage &&
                  page.classList.contains('temporary');
    // Make a deep copy of the dot template to add a new one.
    var newDot = new ntp4.NavDot(page, title, true, animate);
    page.navigationDot = newDot;
    dotList.insertBefore(newDot, refNode ? refNode.navigationDot : null);

    if (infoBubble)
      window.setTimeout(infoBubble.reposition.bind(infoBubble), 0);

    eventTracker.add(page, 'pagelayout', onPageLayout);
  }

  /**
   * Search an elements ancestor chain for the nearest element that is a member
   * of the specified class.
   * @param {!Element} element The element to start searching from.
   * @param {string} className The name of the class to locate.
   * @return {Element} The first ancestor of the specified class or null.
   */
  function getParentByClassName(element, className) {
    for (var e = element; e; e = e.parentElement) {
      if (e.classList.contains(className))
        return e;
    }
    return null;
  }

  /**
   * Called whenever tiles should be re-arranging themselves out of the way of a
   * moving or insert tile.
   */
  function enterRearrangeMode() {
    var tempPage = new ntp4.AppsPage();
    tempPage.classList.add('temporary');
    appendTilePage(tempPage, '', bookmarksPage);
    var tempIndex = Array.prototype.indexOf.call(tilePages, tempPage);
    if (cardSlider.currentCard >= tempIndex)
      cardSlider.currentCard += 1;
    updateSliderCards();

    if (ntp4.getCurrentlyDraggingTile().firstChild.canBeRemoved())
      $('footer').classList.add('showing-trash-mode');
  }

  /**
   * Invoked whenever some app is released
   * @param {Grabber.Event} e The Grabber RELEASE event.
   */
  function leaveRearrangeMode(e) {
    var tempPage = document.querySelector('.tile-page.temporary');
    var dot = tempPage.navigationDot;
    if (!tempPage.tileCount && tempPage != cardSlider.currentCardValue) {
      dot.animateRemove();
      var tempIndex = Array.prototype.indexOf.call(tilePages, tempPage);
      if (cardSlider.currentCard > tempIndex)
        cardSlider.currentCard -= 1;
      tempPage.parentNode.removeChild(tempPage);
      updateSliderCards();
    } else {
      tempPage.classList.remove('temporary');
      saveAppPageName(tempPage, '');
    }

    $('footer').classList.remove('showing-trash-mode');
  }

  /**
   * Callback for the 'click' event on a page switcher.
   * @param {Event} e The event.
   */
  function onPageSwitcherClicked(e) {
    cardSlider.selectCard(cardSlider.currentCard +
        (e.currentTarget == pageSwitcherStart ? -1 : 1), true);
  }

  /**
   * Handler for the mousewheel event on a pager. We pass through the scroll
   * to the page.
   * @param {Event} e The mousewheel event.
   */
  function onPageSwitcherScrolled(e) {
    cardSlider.currentCardValue.scrollBy(-e.wheelDeltaY);
  };

  /**
   * Callback for the 'pagelayout' event.
   * @param {Event} e The event.
   */
  function onPageLayout(e) {
    if (Array.prototype.indexOf.call(tilePages, e.currentTarget) !=
        cardSlider.currentCard) {
      return;
    }

    updatePageSwitchers();
  }

  /**
   * Adjusts the size and position of the page switchers according to the
   * layout of the current card.
   */
  function updatePageSwitchers() {
    var page = cardSlider.currentCardValue;

    pageSwitcherStart.hidden = !page || (cardSlider.currentCard == 0);
    pageSwitcherEnd.hidden = !page ||
        (cardSlider.currentCard == cardSlider.cardCount - 1);

    if (!page)
      return;

    var pageSwitcherLeft = isRTL() ? pageSwitcherEnd : pageSwitcherStart;
    var pageSwitcherRight = isRTL() ? pageSwitcherStart : pageSwitcherEnd;
    var scrollbarWidth = page.scrollbarWidth;
    pageSwitcherLeft.style.width =
        (page.sideMargin + 13) + 'px';
    pageSwitcherLeft.style.left = '0';
    pageSwitcherRight.style.width =
        (page.sideMargin - scrollbarWidth + 13) + 'px';
    pageSwitcherRight.style.right = scrollbarWidth + 'px';

    var offsetTop = page.querySelector('.tile-page-content').offsetTop + 'px';
    pageSwitcherLeft.style.top = offsetTop;
    pageSwitcherRight.style.top = offsetTop;
    pageSwitcherLeft.style.paddingBottom = offsetTop;
    pageSwitcherRight.style.paddingBottom = offsetTop;
  }

  /**
   * Returns the index of the given page.
   * @param {AppsPage} page The AppsPage for we wish to find.
   * @return {number} The index of |page|, or -1 if it is not here.
   */
  function getAppsPageIndex(page) {
    return Array.prototype.indexOf.call(appsPages, page);
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
   * Handler for CARD_CHANGED on cardSlider.
   * @param {Event} e The CARD_CHANGED event.
   */
  function cardChangedHandler(e) {
    var page = e.cardSlider.currentCardValue;

    // Don't change shownPage until startup is done (and page changes actually
    // reflect user actions).
    if (!document.documentElement.classList.contains('starting-up')) {
      if (page.classList.contains('apps-page')) {
        shownPage = templateData['apps_page_id'];
        shownPageIndex = getAppsPageIndex(page);
      } else if (page.classList.contains('most-visited-page')) {
        shownPage = templateData['most_visited_page_id'];
        shownPageIndex = 0;
      } else if (page.classList.contains('bookmarks-page')) {
        shownPage = templateData['bookmarks_page_id'];
        shownPageIndex = 0;
      } else {
        console.error('unknown page selected');
      }
      chrome.send('pageSelected', [shownPage, shownPageIndex]);
    }

    // Update the active dot
    var curDot = dotList.getElementsByClassName('selected')[0];
    if (curDot)
      curDot.classList.remove('selected');
    page.navigationDot.classList.add('selected');
    updatePageSwitchers();
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
      var action = links[i].action;
      link.onclick = function(e) {
        action();
        hideNotification();
      }
      link.setAttribute('role', 'button');
      link.setAttribute('tabindex', 0);
      link.className = "linkButton";
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
    mostVisitedPage.data = data;
  }

  function setBookmarksData(data) {
    bookmarksPage.data = data;
  }

  /**
   * Check the directionality of the page.
   * @return {boolean} True if Chrome is running an RTL UI.
   */
  function isRTL() {
    return document.documentElement.dir == 'rtl';
  }

  /*
   * Save the name of an app page.
   * Store the app page name into the preferences store.
   * @param {AppsPage} appPage The app page for which we wish to save.
   * @param {string} name The name of the page.
   */
  function saveAppPageName(appPage, name) {
    var index = getAppsPageIndex(appPage);
    assert(index != -1);
    chrome.send('saveAppPageName', [name, index]);
  }

  function bookmarkImportBegan() {
    bookmarksPage.bookmarkImportBegan.apply(bookmarksPage, arguments);
  }

  function bookmarkImportEnded() {
    bookmarksPage.bookmarkImportEnded.apply(bookmarksPage, arguments);
  }

  function bookmarkNodeAdded() {
    bookmarksPage.bookmarkNodeAdded.apply(bookmarksPage, arguments);
  }

  function bookmarkNodeChanged() {
    bookmarksPage.bookmarkNodeChanged.apply(bookmarksPage, arguments);
  }

  function bookmarkNodeChildrenReordered() {
    bookmarksPage.bookmarkNodeChildrenReordered.apply(bookmarksPage, arguments);
  }

  function bookmarkNodeMoved() {
    bookmarksPage.bookmarkNodeMoved.apply(bookmarksPage, arguments);
  }

  function bookmarkNodeRemoved() {
    bookmarksPage.bookmarkNodeRemoved.apply(bookmarksPage, arguments);
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
  };

  // Return an object with all the exports
  return {
    appAdded: appAdded,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    assert: assert,
    bookmarkImportBegan: bookmarkImportBegan,
    bookmarkImportEnded: bookmarkImportEnded,
    bookmarkNodeAdded: bookmarkNodeAdded,
    bookmarkNodeChanged: bookmarkNodeChanged,
    bookmarkNodeChildrenReordered: bookmarkNodeChildrenReordered,
    bookmarkNodeMoved: bookmarkNodeMoved,
    bookmarkNodeRemoved: bookmarkNodeRemoved,
    enterRearrangeMode: enterRearrangeMode,
    getAppsCallback: getAppsCallback,
    getAppsPageIndex: getAppsPageIndex,
    getCardSlider: getCardSlider,
    initialize: initialize,
    isRTL: isRTL,
    leaveRearrangeMode: leaveRearrangeMode,
    saveAppPageName: saveAppPageName,
    setAppToBeHighlighted: setAppToBeHighlighted,
    setBookmarksData: setBookmarksData,
    setMostVisitedPages: setMostVisitedPages,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setStripeColor: setStripeColor,
    showNotification: showNotification,
    themeChanged: themeChanged,
    updateOfflineEnabledApps: updateOfflineEnabledApps
  };
});

// publish ntp globals
// TODO(estade): update the content handlers to use ntp namespace instead of
// making these global.
var assert = ntp4.assert;
var getAppsCallback = ntp4.getAppsCallback;
var appsPrefChangeCallback = ntp4.appsPrefChangeCallback;
var themeChanged = ntp4.themeChanged;
var recentlyClosedTabs = ntp4.setRecentlyClosedTabs;
var setMostVisitedPages = ntp4.setMostVisitedPages;

document.addEventListener('DOMContentLoaded', ntp4.initialize);
window.addEventListener('online', ntp4.updateOfflineEnabledApps);
window.addEventListener('offline', ntp4.updateOfflineEnabledApps);
