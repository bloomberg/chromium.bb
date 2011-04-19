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
   * Template to use for creating new 'dot' elements
   * @type {!Element|undefined}
   */
  var dotTemplate;

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
   * The 'dots-list' element.
   * @type {!Element|undefined}
   */
  var dotList;

  /**
   * A list of all 'dots' elements.
   * @type {!NodeList|undefined}
   */
  var dots;

  /**
   * The 'trash' element.  Note that technically this is unnecessary,
   * JavaScript creates the object for us based on the id.  But I don't want
   * to rely on the ID being the same, and JSCompiler doesn't know about it.
   * @type {!Element|undefined}
   */
  var trash;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * All the Grabber objects currently in use on the page
   * @type {Array.<Grabber>}
   */
  var grabbers = [];

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function initialize() {
    // Load the current theme colors.
    themeChanged(false);

    dotList = getRequiredElement('dot-list');
    pageList = getRequiredElement('page-list');
    trash = getRequiredElement('trash');
    trash.hidden = true;

    // Request data on the apps so we can fill them in.
    // Note that this is kicked off asynchronously.  'getAppsCallback' will be
    // invoked at some point after this function returns.
    chrome.send('getApps');

    // Prevent touch events from triggering any sort of native scrolling
    document.addEventListener('touchmove', function(e) {
      e.preventDefault();
    }, true);

    // Get the template elements and remove them from the DOM.  Things are
    // simpler if we start with 0 pages and 0 apps and don't leave hidden
    // template elements behind in the DOM.
    dots = dotList.getElementsByClassName('dot');
    assert(dots.length == 1,
           'Expected exactly one dot in the dots-list.');
    dotTemplate = dots[0];
    dotList.removeChild(dots[0]);

    tilePages = pageList.getElementsByClassName('tile-page');
    appsPages = pageList.getElementsByClassName('apps-page');

    // Initialize the cardSlider without any cards at the moment
    var sliderFrame = getRequiredElement('card-slider-frame');
    cardSlider = new CardSlider(sliderFrame, pageList, [], 0,
                                sliderFrame.offsetWidth);
    cardSlider.initialize();

    // Ensure the slider is resized appropriately with the window
    window.addEventListener('resize', function() {
      cardSlider.resize(sliderFrame.offsetWidth);
    });

    // Handle the page being changed
    pageList.addEventListener(
        CardSlider.EventType.CARD_CHANGED,
        function(e) {
          // Update the active dot
          var curDot = dotList.getElementsByClassName('selected')[0];
          if (curDot)
            curDot.classList.remove('selected');
          var newPageIndex = e.cardSlider.currentCard;
          dots[newPageIndex].classList.add('selected');
          // If an app was being dragged, move it to the end of the new page
          if (draggingAppContainer)
            appsPages[newPageIndex].appendChild(draggingAppContainer);
        });

    // Add a drag handler to the body (for drags that don't land on an existing
    // app)
    document.addEventListener(Grabber.EventType.DRAG_ENTER, appDragEnter);

    // Handle dropping an app anywhere other than on the trash
    document.addEventListener(Grabber.EventType.DROP, appDrop);

    // Add handles to manage the transition into/out-of rearrange mode
    // Note that we assume here that we only use a Grabber for moving apps,
    // so ANY GRAB event means we're enterring rearrange mode.
    sliderFrame.addEventListener(Grabber.EventType.GRAB, enterRearrangeMode);
    sliderFrame.addEventListener(Grabber.EventType.RELEASE, leaveRearrangeMode);

    // Add handlers for the tash can
    trash.addEventListener(Grabber.EventType.DRAG_ENTER, function(e) {
      trash.classList.add('hover');
      e.grabbedElement.classList.add('trashing');
      e.stopPropagation();
    });
    trash.addEventListener(Grabber.EventType.DRAG_LEAVE, function(e) {
      e.grabbedElement.classList.remove('trashing');
      trash.classList.remove('hover');
    });
    trash.addEventListener(Grabber.EventType.DROP, appTrash);

    cr.ui.decorate($('recently-closed-menu-button'), ntp4.RecentMenuButton);
    chrome.send('getRecentlyClosedTabs');

    mostVisitedPage = new ntp4.MostVisitedPage('Most Visited');
    appendTilePage(mostVisitedPage);
    chrome.send('getMostVisited');
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
    // Clean up any existing grabber objects - cancelling any outstanding drag.
    // Ideally an async app update wouldn't disrupt an active drag but
    // that would require us to re-use existing elements and detect how the apps
    // have changed, which would be a lot of work.
    // Note that we have to explicitly clean up the grabber objects so they stop
    // listening to events and break the DOM<->JS cycles necessary to enable
    // collection of all these objects.
    grabbers.forEach(function(g) {
      // Note that this may raise DRAG_END/RELEASE events to clean up an
      // oustanding drag.
      g.dispose();
    });
    assert(!draggingAppContainer && !draggingAppOriginalPosition &&
           !draggingAppOriginalPage);
    grabbers = [];

    // Clear any existing apps pages and dots.
    // TODO(rbyers): It might be nice to preserve animation of dots after an
    // uninstall. Could we re-use the existing page and dot elements?  It seems
    // unfortunate to have Chrome send us the entire apps list after an
    // uninstall.
    for (var i = 0; i < appsPages.length; i++) {
      var page = appsPages[i];
      var dot = page.navigationDot;

      page.tearDown();
      page.parentNode.removeChild(page);
      dot.parentNode.removeChild(dot);
    }

    // Get the array of apps and add any special synthesized entries
    var apps = data.apps;

    // Sort by launch index
    apps.sort(function(a, b) {
      return a.app_launch_index - b.app_launch_index;
    });

    // Add the apps, creating pages as necessary
    for (var i = 0; i < apps.length; i++) {
      var app = apps[i];
      var pageIndex = (app.page_index || 0);
      while (pageIndex >= appsPages.length) {
        var origPageCount = appsPages.length;
        appendTilePage(new ntp4.AppsPage('Apps'));
        // Confirm that appsPages is a live object, updated when a new page is
        // added (otherwise we'd have an infinite loop)
        assert(appsPages.length == origPageCount + 1, 'expected new page');
      }

      appsPages[pageIndex].appendApp(app);
    }

    // Add a couple blank apps pages for testing. TODO(estade): remove this.
    appendTilePage(new ntp4.AppsPage('Foo'));
    appendTilePage(new ntp4.AppsPage('Bar'));

    // Tell the slider about the pages
    updateSliderCards();

    // Mark the current page
    dots[cardSlider.currentCard].classList.add('selected');
  }

  /**
   * Make a synthesized app object representing the chrome web store.  It seems
   * like this could just as easily come from the back-end, and then would
   * support being rearranged, etc.
   * @return {Object} The app object as would be sent from the webui back-end.
   */
  function makeWebstoreApp() {
    return {
      id: '',   // Empty ID signifies this is a special synthesized app
      page_index: 0,
      app_launch_index: -1,   // always first
      name: templateData.web_store_title,
      launch_url: templateData.web_store_url,
      icon_big: getThemeUrl('IDR_WEBSTORE_ICON')
    };
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
   * The normal NTP uses this to keep track of the current launch-type of an
   * app, updating the choices in the context menu.  We don't have such a menu
   * so don't use this at all (but it still needs to be here for chrome to
   * call).
   * @param {Object} data An object with all the data on available
   *        applications.
   */
  function appsPrefChangeCallback(data) {
  }

  /**
   * Invoked whenever the pages in apps-page-list have changed so that
   * the Slider knows about the new elements.
   */
  function updateSliderCards() {
    var pageNo = cardSlider.currentCard;
    if (pageNo >= tilePages.length)
      pageNo = tilePages.length - 1;
    var pageArray = [];
    for (var i = 0; i < tilePages.length; i++)
      pageArray[i] = tilePages[i];
    cardSlider.setCards(pageArray, pageNo);
  }

  /**
   * Appends a tile page (for apps or most visited).
   *
   * @param {TilePage} page The page element.
   * @param {boolean=} opt_animate If true, add the class 'new' to the created
   *        dot.
   */
  function appendTilePage(page, opt_animate) {
    pageList.appendChild(page);

    // Make a deep copy of the dot template to add a new one.
    var dotCount = dots.length;
    var newDot = dotTemplate.cloneNode(true);
    newDot.querySelector('span').textContent = page.pageName;
    if (opt_animate)
      newDot.classList.add('new');
    dotList.appendChild(newDot);
    page.navigationDot = newDot;

    // Add click handler to the dot to change the page.
    // TODO(rbyers): Perhaps this should be TouchHandler.START_EVENT_ (so we
    // don't rely on synthesized click events, and the change takes effect
    // before releasing). However, click events seems to be synthesized for a
    // region outside the border, and a 10px box is too small to require touch
    // events to fall inside of. We could get around this by adding a box around
    // the dot for accepting the touch events.
    function switchPage(e) {
      cardSlider.selectCard(dotCount, true);
      e.stopPropagation();
    }
    newDot.addEventListener('click', switchPage);

    // Change pages whenever an app is dragged over a dot.
    newDot.addEventListener(Grabber.EventType.DRAG_ENTER, switchPage);
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
   * The container where the app currently being dragged came from.
   * @type {!Element|undefined}
   */
  var draggingAppContainer;

  /**
   * The apps-page that the app currently being dragged camed from.
   * @type {!Element|undefined}
   */
  var draggingAppOriginalPage;

  /**
   * The element that was originally after the app currently being dragged (or
   * null if it was the last on the page).
   * @type {!Element|undefined}
   */
  var draggingAppOriginalPosition;

  /**
   * Invoked when app dragging begins.
   * @param {Grabber.Event} e The event from the Grabber indicating the drag.
   */
  function appDragStart(e) {
    // Pull the element out to the sliderFrame using fixed positioning. This
    // ensures that the app is not affected (remains under the finger) if the
    // slider changes cards and is translated.  An alternate approach would be
    // to use fixed positioning for the slider (so that changes to its position
    // don't affect children that aren't positioned relative to it), but we
    // don't yet have GPU acceleration for this.
    var element = e.grabbedElement;

    var pos = element.getBoundingClientRect();
    element.style.webkitTransform = '';

    element.style.position = 'fixed';
    // Don't want to zoom around the middle since the left/top co-ordinates
    // are post-transform values.
    element.style.webkitTransformOrigin = 'left top';
    element.style.left = pos.left + 'px';
    element.style.top = pos.top + 'px';

    // Keep track of what app is being dragged and where it came from
    assert(!draggingAppContainer, 'got DRAG_START without DRAG_END');
    draggingAppContainer = element.parentNode;
    assert(draggingAppContainer.classList.contains('app-container'));
    draggingAppOriginalPosition = draggingAppContainer.nextSibling;
    draggingAppOriginalPage = draggingAppContainer.parentNode;

    // Move the app out of the container
    // Note that appendChild also removes the element from its current parent.
    sliderFrame.appendChild(element);
  }

  /**
   * Invoked when app dragging terminates (either successfully or not)
   * @param {Grabber.Event} e The event from the Grabber.
   */
  function appDragEnd(e) {
    // Stop floating the app
    var appBeingDragged = e.grabbedElement;
    assert(appBeingDragged.classList.contains('app'));
    appBeingDragged.style.position = '';
    appBeingDragged.style.webkitTransformOrigin = '';
    appBeingDragged.style.left = '';
    appBeingDragged.style.top = '';

    // Ensure the trash can is not active (we won't necessarily get a DRAG_LEAVE
    // for it - eg. if we drop on it, or the drag is cancelled)
    trash.classList.remove('hover');
    appBeingDragged.classList.remove('trashing');

    // If we have an active drag (i.e. it wasn't aborted by an app update)
    if (draggingAppContainer) {
      // Put the app back into it's container
      if (appBeingDragged.parentNode != draggingAppContainer)
        draggingAppContainer.appendChild(appBeingDragged);

      // If we care about the container's original position
      if (draggingAppOriginalPage)
      {
        // Then put the container back where it came from
        if (draggingAppOriginalPosition) {
          draggingAppOriginalPage.insertBefore(draggingAppContainer,
                                               draggingAppOriginalPosition);
        } else {
          draggingAppOriginalPage.appendChild(draggingAppContainer);
        }
      }
    }

    draggingAppContainer = undefined;
    draggingAppOriginalPage = undefined;
    draggingAppOriginalPosition = undefined;
  }

  /**
   * Invoked when an app is dragged over another app.  Updates the DOM to affect
   * the rearrangement (but doesn't commit the change until the app is dropped).
   * @param {Grabber.Event} e The event from the Grabber indicating the drag.
   */
  function appDragEnter(e)
  {
    assert(draggingAppContainer, 'expected stored container');
    var sourceContainer = draggingAppContainer;

    // Ensure enter events delivered to an app-container don't also get
    // delivered to the document.
    e.stopPropagation();

    var curPage = appsPages[cardSlider.currentCard];
    var followingContainer = null;

    // If we dragged over a specific app, determine which one to insert before
    if (e.currentTarget != document) {

      // Start by assuming we'll insert the app before the one dragged over
      followingContainer = e.currentTarget;
      assert(followingContainer.classList.contains('app-container'),
             'expected drag over container');
      assert(followingContainer.parentNode == curPage);
      if (followingContainer == draggingAppContainer)
        return;

      // But if it's after the current container position then we'll need to
      // move ahead by one to account for the container being removed.
      if (curPage == draggingAppContainer.parentNode) {
        for (var c = draggingAppContainer; c; c = c.nextElementSibling) {
          if (c == followingContainer) {
            followingContainer = followingContainer.nextElementSibling;
            break;
          }
        }
      }
    }

    // Move the container to the appropriate place on the page
    curPage.insertBefore(draggingAppContainer, followingContainer);
  }

  /**
   * Invoked when an app is dropped on the trash
   * @param {Grabber.Event} e The event from the Grabber indicating the drop.
   */
  function appTrash(e) {
    var appElement = e.grabbedElement;
    assert(appElement.classList.contains('app'));
    var appId = appElement.getAttribute('app-id');
    assert(appId);

    // Mark this drop as handled so that the catch-all drop handler
    // on the document doesn't see this event.
    e.stopPropagation();

    // Tell chrome to uninstall the app (prompting the user)
    chrome.send('uninstallApp', [appId]);
  }

  /**
   * Called when an app is dropped anywhere other than the trash can.  Commits
   * any movement that has occurred.
   * @param {Grabber.Event} e The event from the Grabber indicating the drop.
   */
  function appDrop(e) {
    if (!draggingAppContainer)
      // Drag was aborted (eg. due to an app update) - do nothing
      return;

    // If the app is dropped back into it's original position then do nothing
    assert(draggingAppOriginalPage);
    if (draggingAppContainer.parentNode == draggingAppOriginalPage &&
        draggingAppContainer.nextSibling == draggingAppOriginalPosition)
      return;

    // Determine which app was being dragged
    var appElement = e.grabbedElement;
    assert(appElement.classList.contains('app'));
    var appId = appElement.getAttribute('app-id');
    assert(appId);

    // Update the page index for the app if it's changed.  This doesn't trigger
    // a call to getAppsCallback so we want to do it before reorderApps
    var pageIndex = cardSlider.currentCard;
    assert(pageIndex >= 0 && pageIndex < appsPages.length,
           'page number out of range');
    if (appsPages[pageIndex] != draggingAppOriginalPage)
      chrome.send('setPageIndex', [appId, pageIndex]);

    // Put the app being dragged back into it's container
    draggingAppContainer.appendChild(appElement);

    // Create a list of all appIds in the order now present in the DOM
    var appIds = [];
    for (var page = 0; page < appsPages.length; page++) {
      var appsOnPage = appsPages[page].getElementsByClassName('app');
      for (var i = 0; i < appsOnPage.length; i++) {
        var id = appsOnPage[i].getAttribute('app-id');
        if (id)
          appIds.push(id);
      }
    }

    // We are going to commit this repositioning - clear the original position
    draggingAppOriginalPage = undefined;
    draggingAppOriginalPosition = undefined;

    // Tell chrome to update its database to persist this new order of apps This
    // will cause getAppsCallback to be invoked and the apps to be redrawn.
    chrome.send('reorderApps', [appId, appIds]);
    appMoved = true;
  }

  /**
   * Set to true if we're currently in rearrange mode and an app has
   * been successfully dropped to a new location.  This indicates that
   * a getAppsCallback call is pending and we can rely on the DOM being
   * updated by that.
   * @type {boolean}
   */
  var appMoved = false;

  /**
   * Invoked whenever some app is grabbed
   * @param {Grabber.Event} e The Grabber Grab event.
   */
  function enterRearrangeMode(e)
  {
    // Stop the slider from sliding for this touch
    cardSlider.cancelTouch();

    // Add an extra blank page in case the user wants to create a new page
    appendTilePage(new ntp4.AppsPage(''), true);
    var pageAdded = appsPages.length - 1;
    window.setTimeout(function() {
      dots[pageAdded].classList.remove('new');
    }, 0);

    updateSliderCards();

    // Cause the dot-list to grow
    getRequiredElement('footer').classList.add('rearrange-mode');

    assert(!appMoved, 'appMoved should not be set yet');
  }

  /**
   * Invoked whenever some app is released
   * @param {Grabber.Event} e The Grabber RELEASE event.
   */
  function leaveRearrangeMode(e)
  {
    // Return the dot-list to normal
    getRequiredElement('footer').classList.remove('rearrange-mode');

    // If we didn't successfully re-arrange an app, then we won't be
    // refreshing the app view in getAppCallback and need to explicitly remove
    // the extra empty page we added.  We don't want to do this in the normal
    // case because if we did actually drop an app there, we want to retain that
    // page as our current page number.
    if (!appMoved) {
      assert(appsPages[appsPages.length - 1].
             getElementsByClassName('app-container').length == 0,
             'Last app page should be empty');
      removePage(appsPages.length - 1);
    }
    appMoved = false;
  }

  /**
   * Remove the page with the specified index and update the slider.
   * @param {number} pageNo The index of the page to remove.
   */
  function removePage(pageNo) {
    pageList.removeChild(tilePages[pageNo]);

    // Remove the corresponding dot
    // Need to give it a chance to animate though
    var dot = dots[pageNo];
    dot.classList.add('new');
    window.setTimeout(function() {
      // If we've re-created the apps (eg. because an app was uninstalled) then
      // we will have removed the old dots from the document already, so skip.
      if (dot.parentNode)
        dot.parentNode.removeChild(dot);
    }, DEFAULT_TRANSITION_TIME);

    updateSliderCards();
  }

  // TODO(estade): remove |hasAttribution|.
  // TODO(estade): rename newtab.css to new_tab_theme.css
  function themeChanged(hasAttribution) {
    $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
  }

  function setRecentlyClosedTabs(dataItems) {
    $('recently-closed-menu-button').dataItems = dataItems;
  }

  function setMostVisitedPages(data, firstRun, hasBlacklistedUrls) {
    mostVisitedPage.data = data;
  }

  // Return an object with all the exports
  return {
    assert: assert,
    appsPrefChangeCallback: appsPrefChangeCallback,
    getAppsCallback: getAppsCallback,
    initialize: initialize,
    themeChanged: themeChanged,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setMostVisitedPages: setMostVisitedPages,
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
var mostVisitedPages = ntp4.setMostVisitedPages;

document.addEventListener('DOMContentLoaded', ntp4.initialize);
