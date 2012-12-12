// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview New tab page
 * This is the main code for the new tab page. NewTabView manages page list,
 * dot list and handles apps pages callbacks from backend. It also handles
 * the layout of the Bottom Panel and the global UI states of the New Tab Page.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('ntp', function() {
  'use strict';

  var APP_LAUNCH = {
    // The histogram buckets (keep in sync with extension_constants.h).
    NTP_APPS_MAXIMIZED: 0,
    NTP_APPS_COLLAPSED: 1,
    NTP_APPS_MENU: 2,
    NTP_MOST_VISITED: 3,
    NTP_RECENTLY_CLOSED: 4,
    NTP_APP_RE_ENABLE: 16,
    NTP_WEBSTORE_FOOTER: 18,
    NTP_WEBSTORE_PLUS_ICON: 19,
  };

  /**
   * @type {number}
   * @const
   */
  var BOTTOM_PANEL_HORIZONTAL_MARGIN = 100;

  /**
   * The height required to show the Bottom Panel.
   * @type {number}
   * @const
   */
  var HEIGHT_FOR_BOTTOM_PANEL = 531;

  /**
   * The Bottom Panel width required to show 6 cols of Tiles, which is used
   * in the width computation.
   * @type {number}
   * @const
   */
  var MAX_BOTTOM_PANEL_WIDTH = 920;

  /**
   * The minimum width of the Bottom Panel's content.
   * @type {number}
   * @const
   */
  var MIN_BOTTOM_PANEL_CONTENT_WIDTH = 200;

  /**
   * The minimum Bottom Panel width. If the available width is smaller than
   * this value, then the width of the Bottom Panel's content will be fixed to
   * MIN_BOTTOM_PANEL_CONTENT_WIDTH.
   * @type {number}
   * @const
   */
  var MIN_BOTTOM_PANEL_WIDTH = 300;

  /**
   * The normal Bottom Panel width. If the window width is greater than or
   * equal to this value, then the width of the Bottom Panel's content will be
   * the available width minus side margin. If the available width is smaller
   * than this value, then the width of the Bottom Panel's content will be an
   * interpolation between the normal width, and the minimum width defined by
   * the constant MIN_BOTTOM_PANEL_CONTENT_WIDTH.
   * @type {number}
   * @const
   */
  var NORMAL_BOTTOM_PANEL_WIDTH = 500;

  /**
   * @type {number}
   * @const
   */
  var TILE_ROW_HEIGHT = 100;

  //----------------------------------------------------------------------------

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
   * @type {!Element|undefined}
   */
  var promoBubble;

  /**
   * The total number of thumbnails that were hovered over.
   * @type {number}
   * @private
   */
  var hoveredThumbnailCount = 0;

  /**
   * The time when all sections are ready.
   * @type {number|undefined}
   * @private
   */
  var startTime;

  /**
   * The top position of the Bottom Panel.
   * @type {number|undefined}
   * @private
   */
  var bottomPanelOffsetTop;

  /**
   * The height of the Bottom Panel Header, in pixels.
   * @type {number|undefined}
   * @private
   */
  var headerHeight;

  /**
   * The height of the Bottom Panel Footer, in pixels.
   * @type {number|undefined}
   * @private
   */
  var footerHeight;

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
    OTHER: 13,
  };

  /**
   * Creates a NewTabView object.
   * @constructor
   */
  function NewTabView() {
    this.initialize(getRequiredElement('page-list'),
                    getRequiredElement('dot-list'),
                    getRequiredElement('card-slider-frame'));
  }

  NewTabView.prototype = {
    /**
     * The CardSlider object to use for changing app pages.
     * @type {CardSlider|undefined}
     */
    cardSlider: undefined,

    /**
     * The frame div for this.cardSlider.
     * @type {!Element|undefined}
     */
    sliderFrame: undefined,

    /**
     * The 'page-list' element.
     * @type {!Element|undefined}
     */
    pageList: undefined,

    /**
     * A list of all 'tile-page' elements.
     * @type {!NodeList|undefined}
     */
    tilePages: undefined,

    /**
     * The Apps page.
     * @type {!Element|undefined}
     */
    appsPage: undefined,

    /**
     * The Most Visited page.
     * @type {!Element|undefined}
     */
    mostVisitedPage: undefined,

    /**
     * The Recently Closed page.
     * @type {!Element|undefined}
     */
    recentlyClosedPage: undefined,

    /**
     * The Devices page.
     * @type {!Element|undefined}
     */
    otherDevicesPage: undefined,

    /**
     * The 'dots-list' element.
     * @type {!Element|undefined}
     */
    dotList: undefined,

    /**
     * The type of page that is currently shown. The value is a numerical ID.
     * @type {number}
     */
    shownPage: 0,

    /**
     * The index of the page that is currently shown, within the page type.
     * For example if the third Apps page is showing, this will be 2.
     * @type {number}
     */
    shownPageIndex: 0,

    /**
     * If non-null, this is the ID of the app to highlight to the user the next
     * time getAppsCallback runs. "Highlight" in this case means to switch to
     * the page and run the new tile animation.
     * @type {?string}
     */
    highlightAppId: null,

    /**
     * Initializes new tab view.
     * @param {!Element} pageList A DIV element to host all pages.
     * @param {!Element} dotList An UL element to host nav dots. Each dot
     *     represents a page.
     * @param {!Element} cardSliderFrame The card slider frame that hosts
     *     pageList.
     */
    initialize: function(pageList, dotList, cardSliderFrame) {
      this.pageList = pageList;

      this.dotList = dotList;
      cr.ui.decorate(this.dotList, ntp.DotList);

      this.shownPage = loadTimeData.getInteger('shown_page_type');
      this.shownPageIndex = loadTimeData.getInteger('shown_page_index');

      if (loadTimeData.getBoolean('showApps')) {
        // When the Apps Page is available, then the dot list should be visible.
        this.dotList.removeAttribute('hidden');
        // Request data on the apps so we can fill them in.
        // Note that this is kicked off asynchronously.  'getAppsCallback' will
        // be invoked at some point after this function returns.
        chrome.send('getApps');
      } else if (this.shownPage == loadTimeData.getInteger('apps_page_id')) {
        // No apps page.
        this.setShownPage_(
            loadTimeData.getInteger('most_visited_page_id'), 0);
      }

      this.tilePages = this.pageList.getElementsByClassName('tile-page');

      // Initialize the cardSlider without any cards at the moment.
      this.sliderFrame = cardSliderFrame;
      this.cardSlider = new cr.ui.CardSlider(this.sliderFrame, this.pageList,
          this.sliderFrame.offsetWidth);

      var cardSlider = this.cardSlider;
      this.cardSlider.initialize(
          loadTimeData.getBoolean('isSwipeTrackingFromScrollEventsEnabled'));

      // Prevent touch events from triggering any sort of native scrolling.
      document.addEventListener('touchmove', function(e) {
        e.preventDefault();
      }, true);

      // Handle events from the card slider.
      this.pageList.addEventListener('cardSlider:card_changed',
          this.onCardChanged_.bind(this));
      this.pageList.addEventListener('cardSlider:card_added',
          this.onCardAdded_.bind(this));
      this.pageList.addEventListener('cardSlider:card_removed',
         this.onCardRemoved_.bind(this));

      // Update apps when online state changes.
      window.addEventListener('online',
          this.updateOfflineEnabledApps_.bind(this));
      window.addEventListener('offline',
          this.updateOfflineEnabledApps_.bind(this));
    },

    /**
     * Starts listening to user input events. The resize and keydown events
     * must be added only when all NTP have finished loading because they
     * will act in the current selected page.
     */
    onReady: function() {
      window.addEventListener('resize', this.onWindowResize_.bind(this));
      document.addEventListener('keydown', this.onDocKeyDown_.bind(this));
    },

    /**
     * Appends a tile page.
     *
     * @param {TilePage} page The page element.
     * @param {string} title The title of the tile page.
     * @param {TilePage=} opt_refNode Optional reference node to insert in front
     *     of.
     * When opt_refNode is falsey, |page| will just be appended to the end of
     * the page list.
     */
    appendTilePage: function(page, title, opt_refNode) {
      if (opt_refNode) {
        var refIndex = this.getTilePageIndex(opt_refNode);
        this.cardSlider.addCardAtIndex(page, refIndex);
      } else {
        this.cardSlider.appendCard(page);
      }

      // Remember special MostVisitedPage.
      if (typeof ntp.MostVisitedPage != 'undefined' &&
          page instanceof ntp.MostVisitedPage) {
        assert(this.tilePages.length == 1,
               'MostVisitedPage should be added as first tile page');
        this.mostVisitedPage = page;
      }

      if (typeof ntp.AppsPage != 'undefined' &&
          page instanceof ntp.AppsPage) {
        this.appsPage = page;
      }

      if (typeof ntp.RecentlyClosedPage != 'undefined' &&
          page instanceof ntp.RecentlyClosedPage) {
        this.recentlyClosedPage = page;
      }

      // Remember special OtherDevicesPage.
      if (typeof ntp.OtherDevicesPage != 'undefined' &&
          page instanceof ntp.OtherDevicesPage) {
        this.otherDevicesPage = page;
      }

      // Make a deep copy of the dot template to add a new one.
      var newDot = new ntp.NavDot(page, title);
      page.navigationDot = newDot;
      this.dotList.insertBefore(newDot,
                                opt_refNode ? opt_refNode.navigationDot : null);
      // Set a tab index on the first dot.
      if (this.dotList.dots.length == 1)
        newDot.tabIndex = 3;
    },

    /**
     * Called by chrome when an app has changed positions.
     * @param {Object} data The data for the app. This contains page and
     *     position indices.
     */
    appMoved: function(data) {
      assert(loadTimeData.getBoolean('showApps'));

      var app = $(data.id);
      assert(app, 'trying to move an app that doesn\'t exist');
      app.remove(false);

      this.appsPage.insertApp(data, false);
    },

    /**
     * Called by chrome when an existing app has been disabled or
     * removed/uninstalled from chrome.
     * @param {Object} data A data structure full of relevant information for
     *     the app.
     * @param {boolean} isUninstall True if the app is being uninstalled;
     *     false if the app is being disabled.
     * @param {boolean} fromPage True if the removal was from the current page.
     */
    appRemoved: function(data, isUninstall, fromPage) {
      assert(loadTimeData.getBoolean('showApps'));

      var app = $(data.id);
      assert(app, 'trying to remove an app that doesn\'t exist');

      if (!isUninstall)
        app.replaceAppData(data);
      else
        app.remove(!!fromPage);
    },

    /**
     * @return {boolean} If the page is still starting up.
     * @private
     */
    isStartingUp_: function() {
      return document.documentElement.classList.contains('starting-up');
    },

    /**
     * Tracks whether apps have been loaded at least once.
     * @type {boolean}
     * @private
     */
    appsLoaded_: false,

    /**
     * Callback invoked by chrome with the apps available.
     *
     * Note that calls to this function can occur at any time, not just in
     * response to a getApps request. For example, when a user
     * installs/uninstalls an app on another synchronized devices.
     * @param {Object} data An object with all the data on available
     *        applications.
     */
    getAppsCallback: function(data) {
      assert(loadTimeData.getBoolean('showApps'));

      var startTime = Date.now();

      // Get the array of apps and add any special synthesized entries.
      var apps = data.apps;

      // Sort alphabetically.
      apps.sort(function(a, b) {
        return a.title.toLocaleLowerCase() > b.title.toLocaleLowerCase() ? 1 :
          a.title.toLocaleLowerCase() < b.title.toLocaleLowerCase() ? -1 : 0;
      });

      // An app to animate (in case it was just installed).
      var highlightApp;

      if (this.appsPage) {
        this.appsPage.removeAllTiles();
      } else {
        var page = new ntp.AppsPage();
        page.setDataList(apps);
        this.appendTilePage(page, loadTimeData.getString('appDefaultPageName'));
      }

      for (var i = 0; i < apps.length; i++) {
        var app = apps[i];
        if (app.id == this.highlightAppId)
          highlightApp = app;
        else
          this.appsPage.insertApp(app, false);
      }

      if (highlightApp)
        this.appAdded(highlightApp, true);

      logEvent('apps.layout: ' + (Date.now() - startTime));

      // Tell the slider about the pages and mark the current page.
      this.updateSliderCards();

      if (!this.appsLoaded_) {
        this.appsLoaded_ = true;
        cr.dispatchSimpleEvent(document, 'sectionready', true, true);
      }
    },

    /**
     * Called by chrome when a new app has been added to chrome or has been
     * enabled if previously disabled.
     * @param {Object} data A data structure full of relevant information for
     *     the app.
     * @param {boolean=} opt_highlight Whether the app about to be added should
     *     be highlighted.
     */
    appAdded: function(data, opt_highlight) {
      assert(loadTimeData.getBoolean('showApps'));

      if (data.id == this.highlightAppId) {
        opt_highlight = true;
        this.highlightAppId = null;
      }

      var pageIndex = data.page_index || 0;

      var app = $(data.id);
      if (app) {
        app.replaceAppData(data);
      } else if (opt_highlight) {
        this.appsPage.insertAndHighlightApp(data);
        this.setShownPage_(loadTimeData.getInteger('apps_page_id'),
                           data.page_index);
      } else {
        this.appsPage.insertApp(data, false);
      }
    },

    /**
     * Callback invoked by chrome whenever an app preference changes.
     * @param {Object} data An object with all the data on available
     *     applications.
     */
    appsPrefChangedCallback: function(data) {
      assert(loadTimeData.getBoolean('showApps'));

      for (var i = 0; i < data.apps.length; ++i) {
        $(data.apps[i].id).data = data.apps[i];
      }
    },

    /**
     * Invoked whenever the pages in page-list have changed so that the
     * CardSlider knows about the new elements.
     */
    updateSliderCards: function() {
      var pageNo = Math.max(0, Math.min(this.cardSlider.currentCard,
                                        this.tilePages.length - 1));
      this.cardSlider.setCards(Array.prototype.slice.call(this.tilePages),
                               pageNo);
      switch (this.shownPage) {
        case loadTimeData.getInteger('apps_page_id'):
          this.cardSlider.selectCardByValue(this.appsPage);
          break;
        case loadTimeData.getInteger('most_visited_page_id'):
          if (this.mostVisitedPage)
            this.cardSlider.selectCardByValue(this.mostVisitedPage);
          break;
      }
    },

    /**
     * Handler for cardSlider:card_changed events from this.cardSlider.
     * @param {Event} e The cardSlider:card_changed event.
     * @private
     */
    onCardChanged_: function(e) {
      var page = e.cardSlider.currentCardValue;

      // Don't change shownPage until startup is done (and page changes actually
      // reflect user actions).
      if (!this.isStartingUp_()) {
        if (page.classList.contains('apps-page')) {
          this.setShownPage_(loadTimeData.getInteger('apps_page_id'), 0);
        } else if (page.classList.contains('most-visited-page')) {
          this.setShownPage_(
              loadTimeData.getInteger('most_visited_page_id'), 0);
        } else if (page.classList.contains('recently-closed-page')) {
          this.setShownPage_(
              loadTimeData.getInteger('recently_closed_page_id'), 0);
        } else if (page.classList.contains('other-devices-page')) {
          this.setShownPage_(
              loadTimeData.getInteger('other_devices_page_id'), 0);
        } else {
          console.error('unknown page selected');
        }
      }

      // Update the active dot
      var curDot = this.dotList.getElementsByClassName('selected')[0];
      if (curDot)
        curDot.classList.remove('selected');
      page.navigationDot.classList.add('selected');
    },

    /**
     * Saves/updates the newly selected page to open when first loading the NTP.
     * @type {number} shownPage The new shown page type.
     * @type {number} shownPageIndex The new shown page index.
     * @private
     */
    setShownPage_: function(shownPage, shownPageIndex) {
      assert(shownPageIndex >= 0);
      this.shownPage = shownPage;
      this.shownPageIndex = shownPageIndex;
      chrome.send('pageSelected', [this.shownPage, this.shownPageIndex]);
    },

    /**
     * Listen for card additions to update the current card accordingly.
     * @param {Event} e A card removed or added event.
     */
    onCardAdded_: function(e) {
      var page = e.addedCard;
      // When the second arg passed to insertBefore is falsey, it acts just like
      // appendChild.
      this.pageList.insertBefore(page, this.tilePages[e.addedIndex]);
      this.layout(false, page);
      this.onCardAddedOrRemoved_();
    },

    /**
     * Listen for card removals to update the current card accordingly.
     * @param {Event} e A card removed or added event.
     */
    onCardRemoved_: function(e) {
      e.removedCard.remove();
      this.onCardAddedOrRemoved_();
    },

    /**
     * Called when a card is removed or added.
     * @private
     */
    onCardAddedOrRemoved_: function() {
      if (this.isStartingUp_())
        return;

      // Without repositioning there were issues - http://crbug.com/133457.
      this.cardSlider.repositionFrame();
    },

    /**
     * Window resize handler.
     * @private
     */
    onWindowResize_: function(e) {
      this.cardSlider.resize(this.sliderFrame.offsetWidth);
      this.layout(true);
    },

    /**
     * Handler for key events on the page. Ctrl-Arrow will switch the visible
     * page.
     * @param {Event} e The KeyboardEvent.
     * @private
     */
    onDocKeyDown_: function(e) {
      if (!e.ctrlKey || e.altKey || e.metaKey || e.shiftKey)
        return;

      var direction = 0;
      if (e.keyIdentifier == 'Left')
        direction = -1;
      else if (e.keyIdentifier == 'Right')
        direction = 1;
      else
        return;

      var cardIndex =
          (this.cardSlider.currentCard + direction +
           this.cardSlider.cardCount) % this.cardSlider.cardCount;
      this.cardSlider.selectCard(cardIndex, true);

      e.stopPropagation();
    },

    /**
     * Listener for offline status change events. Updates apps that are
     * not offline-enabled to be grayscale if the browser is offline.
     * @private
     */
    updateOfflineEnabledApps_: function() {
      var apps = document.querySelectorAll('.app');
      for (var i = 0; i < apps.length; ++i) {
        if (apps[i].data.enabled && !apps[i].data.offline_enabled) {
          apps[i].setIcon();
          apps[i].loadIcon();
        }
      }
    },

    /**
     * Returns the index of a given tile page.
     * @param {TilePage} page The TilePage we wish to find.
     * @return {number} The index of |page| or -1 if it is not in the
     *    collection.
     */
    getTilePageIndex: function(page) {
      return Array.prototype.indexOf.call(this.tilePages, page);
    },

    /**
     * Removes a page and navigation dot (if the navdot exists).
     * @param {TilePage} page The page to be removed.
     */
    removeTilePageAndDot_: function(page) {
      if (page.navigationDot)
        page.navigationDot.remove();
      this.cardSlider.removeCard(page);
    },

    /**
     * The width of the Bottom Panel's content.
     * @type {number}
     */
    contentWidth_: 0,

    /**
     * Calculates the layout of the NTP's Bottom Panel. This method will resize
     * and position all container elements in the Bottom Panel. At the end of
     * the layout process it will dispatch the layout method to the current
     * selected TilePage. Alternatively, you can pass a specific TilePage in
     * the |opt_page| parameter, which is useful for initializing the layout
     * of a recently created TilePage.
     *
     * The |NewTabView.layout| deals with the global layout state while the
     * |TilePage.layout| deals with the per-page layout state. A general rule
     * would be: if you need to resize any element which is outside the
     * card-slider-frame, it should be handled here in NewTabView. Otherwise,
     * it should be handled in TilePage.
     *
     * @param {boolean=} opt_animate Whether the layout should be animated.
     * @param {ntp.TilePage=} opt_page Alternative TilePage to calculate layout.
     */
    layout: function(opt_animate, opt_page) {
      opt_animate = typeof opt_animate == 'undefined' ? false : opt_animate;

      var viewHeight = cr.doc.documentElement.clientHeight;
      var isBottomPanelVisible = viewHeight >= HEIGHT_FOR_BOTTOM_PANEL;
      // Toggles the visibility of the Bottom Panel when there is (or there
      // is not) space to show the entire panel.
      this.showBottomPanel_(isBottomPanelVisible);

      // The layout calculation can be skipped if Bottom Panel is not visible.
      if (!isBottomPanelVisible && !opt_page)
        return;

      // Calculates the width of the Bottom Panel's Content.
      var width = this.calculateContentWidth_();
      if (width != this.contentWidth_) {
        this.contentWidth_ = width;
        $('bottom-panel-footer').style.width = width + 'px';
      }

      // Finally, dispatch the layout method to the current page.
      var currentPage = opt_page || this.cardSlider.currentCardValue;

      var contentHeight = TILE_ROW_HEIGHT;
      if (!opt_page && currentPage.config.scrollable) {
        contentHeight = viewHeight - bottomPanelOffsetTop -
            headerHeight - footerHeight;
        contentHeight = Math.max(TILE_ROW_HEIGHT, contentHeight);
        contentHeight = Math.min(2 * TILE_ROW_HEIGHT, contentHeight);
      }
      this.contentHeight_ = contentHeight;

      $('card-slider-frame').style.height = contentHeight + 'px';

      currentPage.layout(opt_animate);
    },

    /**
     * @return {number} The height of the Bottom Panel's content.
     */
    get contentHeight() {
      return this.contentHeight_;
    },

    /**
     * @return {number} The width of the Bottom Panel's content.
     */
    get contentWidth() {
      return this.contentWidth_;
    },

    /**
     * @return {number} The width of the Bottom Panel's content.
     * @private
     */
    calculateContentWidth_: function() {
      var windowWidth = cr.doc.documentElement.clientWidth;
      var margin = 2 * BOTTOM_PANEL_HORIZONTAL_MARGIN;

      var width;
      if (windowWidth >= MAX_BOTTOM_PANEL_WIDTH) {
        width = MAX_BOTTOM_PANEL_WIDTH - margin;
      } else if (windowWidth >= NORMAL_BOTTOM_PANEL_WIDTH) {
        width = windowWidth - margin;
      } else if (windowWidth >= MIN_BOTTOM_PANEL_WIDTH) {
        // Interpolation between the previous and next states.
        var minMargin = MIN_BOTTOM_PANEL_WIDTH - MIN_BOTTOM_PANEL_CONTENT_WIDTH;
        var factor = (windowWidth - MIN_BOTTOM_PANEL_WIDTH) /
            (NORMAL_BOTTOM_PANEL_WIDTH - MIN_BOTTOM_PANEL_WIDTH);
        var interpolatedMargin = minMargin + factor * (margin - minMargin);
        width = windowWidth - interpolatedMargin;
      } else {
        width = MIN_BOTTOM_PANEL_CONTENT_WIDTH;
      }

      return width;
    },

    /**
     * Animates the display of the Bottom Panel.
     * @param {boolean} show Whether or not to show the Bottom Panel.
     */
    showBottomPanel_: function(show) {
      $('bottom-panel').classList.toggle('hide-bottom-panel', !show);
    },
  };

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function onLoad() {
    // Load the current theme colors.
    themeChanged();

    newTabView = new NewTabView();

    bottomPanelOffsetTop = $('bottom-panel').offsetTop;
    headerHeight = $('bottom-panel-header').offsetHeight;
    footerHeight = $('bottom-panel-footer').offsetHeight;

    notificationContainer = getRequiredElement('notification-container');
    notificationContainer.addEventListener(
        'webkitTransitionEnd', onNotificationTransitionEnd);

    var mostVisited = new ntp.MostVisitedPage();
    newTabView.appendTilePage(mostVisited,
                              loadTimeData.getString('mostvisited'));
    chrome.send('getMostVisited');

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

    doWhenAllSectionsReady(function() {
      // Tell the slider about the pages.
      newTabView.updateSliderCards();
      newTabView.onReady();

      // Restore the visibility only after calling updateSliderCards to avoid
      // flickering, otherwise for a small fraction of a second the Page List is
      // partially rendered.
      $('bottom-panel').style.visibility = 'visible';

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

  /*
   * The number of sections to wait on.
   * @type {number}
   */
  var sectionsToWaitFor = 1;

  /**
   * Queued callbacks which lie in wait for all sections to be ready.
   * @type {!Array}
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
   * @param {function} callback The work to be done when ready.
   */
  function doWhenAllSectionsReady(callback) {
    assert(typeof callback == 'function');
    if (sectionsToWaitFor > 0)
      readyCallbacks.push(callback);
    else
      window.setTimeout(callback, 0);  // Do soon after, but asynchronously.
  }

  function themeChanged(opt_hasAttribution) {
    $('themecss').href = 'chrome://theme/css/new_tab_theme.css?' + Date.now();

    if (typeof opt_hasAttribution != 'undefined') {
      document.documentElement.setAttribute('hasattribution',
                                            opt_hasAttribution);
    }

    updateAttribution();
  }

  function setBookmarkBarAttached(attached) {
    document.documentElement.setAttribute('bookmarkbarattached', attached);
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
  var notificationTimeout = 0;

  /**
   * Shows the notification bubble.
   * @param {string|Node} message The notification message or node to use as
   *     message.
   * @param {Array.<{text: string, action: function()}>} links An array of
   *     records describing the links in the notification. Each record should
   *     have a 'text' attribute (the display string) and an 'action' attribute
   *     (a function to run when the link is activated).
   * @param {Function=} opt_closeHandler The callback invoked if the user
   *     manually dismisses the notification.
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
      var link = linksBin.ownerDocument.createElement('div');
      link.textContent = links[i].text;
      link.action = links[i].action;
      link.onclick = function() {
        this.action();
        hideNotification();
      };
      link.setAttribute('role', 'button');
      link.setAttribute('tabindex', 0);
      link.className = 'link-button';
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
    window.setTimeout(function() {
      notificationContainer.classList.remove('inactive');
    }, 0);

    var timeout = opt_timeout || 10000;
    notificationTimeout = window.setTimeout(hideNotification, timeout);
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
   * @param {Event} e The webkitTransitionEnd event.
   */
  function onNotificationTransitionEnd(e) {
    if (notificationContainer.classList.contains('inactive'))
      notificationContainer.hidden = true;
  }

  function setMostVisitedPages(dataList, hasBlacklistedUrls) {
    var page = newTabView.mostVisitedPage;
    var state = page.getTileRepositioningState();
    if (state) {
      if (state.isRemoving)
        page.animateTileRemoval(state.index, dataList);
      else
        page.animateTileRestoration(state.index, dataList);

      page.resetTileRepositioningState();
    } else {
      page.setDataList(dataList);
      cr.dispatchSimpleEvent(document, 'sectionready', true, true);
    }
  }

  function getThumbnailUrl(url) {
    return 'chrome://thumb/' + url;
  }

  /**
   * Increments the parameter used to log the total number of thumbnail hovered
   * over.
   */
  function incrementHoveredThumbnailCount() {
    hoveredThumbnailCount++;
  }

  /**
   * Logs the time to click for the specified item and the total number of
   * thumbnails hovered over.
   * @param {string} item The item to log the time-to-click.
   */
  function logTimeToClickAndHoverCount(item) {
    var timeToClick = Date.now() - startTime;
    chrome.send('logTimeToClick',
        ['ExtendedNewTabPage.TimeToClick' + item, timeToClick]);
    chrome.send('metricsHandler:recordInHistogram',
        ['ExtendedNewTabPage.hoveredThumbnailCount',
         hoveredThumbnailCount, 40]);
  }

  /**
   * Wrappers to forward the callback to corresponding NewTabView member.
   */
  function appAdded() {
    return newTabView.appAdded.apply(newTabView, arguments);
  }

  function appMoved() {
    return newTabView.appMoved.apply(newTabView, arguments);
  }

  function appRemoved() {
    return newTabView.appRemoved.apply(newTabView, arguments);
  }

  function appsPrefChangeCallback() {
    return newTabView.appsPrefChangedCallback.apply(newTabView, arguments);
  }

  function getAppsCallback() {
    return newTabView.getAppsCallback.apply(newTabView, arguments);
  }

  function getCardSlider() {
    return newTabView.cardSlider;
  }

  function setAppToBeHighlighted(appId) {
    newTabView.highlightAppId = appId;
  }

  function layout(opt_animate) {
    newTabView.layout(opt_animate);
  }

  function getContentHeight() {
    return newTabView.contentHeight;
  }

  function getContentWidth() {
    return newTabView.contentWidth;
  }

  function noop() {
    // Ignore some NTP4 callbacks for backwards compatibility purposes.
  }

  // Return an object with all the exports
  return {
    APP_LAUNCH: APP_LAUNCH,
    TILE_ROW_HEIGHT: TILE_ROW_HEIGHT,
    appAdded: appAdded,
    appMoved: appMoved,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    getAppsCallback: getAppsCallback,
    getCardSlider: getCardSlider,
    getContentHeight: getContentHeight,
    getContentWidth: getContentWidth,
    getThumbnailUrl: getThumbnailUrl,
    incrementHoveredThumbnailCount: incrementHoveredThumbnailCount,
    layout: layout,
    logTimeToClickAndHoverCount: logTimeToClickAndHoverCount,
    NtpFollowAction: NtpFollowAction,
    onLoad: onLoad,
    setAppToBeHighlighted: setAppToBeHighlighted,
    setBookmarkBarAttached: setBookmarkBarAttached,
    setForeignSessions: noop,
    setMostVisitedPages: setMostVisitedPages,
    setRecentlyClosedTabs: noop,
    showNotification: showNotification,
    themeChanged: themeChanged,
    updateLogin: noop,
  };
});

document.addEventListener('DOMContentLoaded', ntp.onLoad);

var toCssPx = cr.ui.toCssPx;
