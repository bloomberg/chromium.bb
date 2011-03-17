// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Touch-based new tab page
 * This is the main code for the new tab page used by touch-enabled Chrome
 * browsers.  For now this is still a prototype.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
var ntp = (function() {
  'use strict';

  /**
   * The Slider object to use for changing app pages.
   * @type {Slider|undefined}
   */
  var slider;

  /**
   * Template to use for creating new 'apps-page' elements
   * @type {!Element|undefined}
   */
  var appsPageTemplate;

  /**
   * Template to use for creating new 'app-container' elements
   * @type {!Element|undefined}
   */
  var appTemplate;

  /**
   * Template to use for creating new 'dot' elements
   * @type {!Element|undefined}
   */
  var dotTemplate;

  /**
   * The 'apps-page-list' element.
   * @type {!Element}
   */
  var appsPageList = getRequiredElement('apps-page-list');

  /**
   * A list of all 'apps-page' elements.
   * @type {!NodeList|undefined}
   */
  var appsPages;

  /**
   * The 'dots-list' element.
   * @type {!Element}
   */
  var dotList = getRequiredElement('dot-list');

  /**
   * A list of all 'dots' elements.
   * @type {!NodeList|undefined}
   */
  var dots;

  /**
   * The 'trash' element.  Note that technically this is unnecessary,
   * JavaScript creates the object for us based on the id.  But I don't want
   * to rely on the ID being the same, and JSCompiler doesn't know about it.
   * @type {!Element}
   */
  var trash = getRequiredElement('trash');

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in newtab.css.  Unfortunately there's no better way to try to time
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
   * Holds all event handlers tied to apps (and so subject to removal when the
   * app list is refreshed)
   * @type {!EventTracker}
   */
  var appEvents = new EventTracker();

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function initializeNtp() {
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
    appTemplate = getRequiredElement('app-template');
    appTemplate.id = null;

    appsPages = appsPageList.getElementsByClassName('apps-page');
    assert(appsPages.length == 1,
           'Expected exactly one apps-page in the apps-page-list.');
    appsPageTemplate = appsPages[0];
    appsPageList.removeChild(appsPages[0]);

    dots = dotList.getElementsByClassName('dot');
    assert(dots.length == 1,
           'Expected exactly one dot in the dots-list.');
    dotTemplate = dots[0];
    dotList.removeChild(dots[0]);

    // Initialize the slider without any cards at the moment
    var appsFrame = getRequiredElement('apps-frame');
    slider = new Slider(appsFrame, appsPageList, [], 0, appsFrame.offsetWidth);
    slider.initialize();

    // Ensure the slider is resized appropriately with the window
    window.addEventListener('resize', function() {
      slider.resize(appsFrame.offsetWidth);
    });

    // Handle the page being changed
    appsPageList.addEventListener(
        Slider.EventType.CARD_CHANGED,
        function(e) {
          // Update the active dot
          var curDot = dotList.getElementsByClassName('selected')[0];
          if (curDot)
            curDot.classList.remove('selected');
          var newPageIndex = e.slider.currentCard;
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
    appsFrame.addEventListener(Grabber.EventType.GRAB, enterRearrangeMode);
    appsFrame.addEventListener(Grabber.EventType.RELEASE, leaveRearrangeMode);

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
   * Remove all children of an element which have a given class in
   * their classList.
   * @param {!Element} element The parent element to examine.
   * @param {string} className The class to look for.
   */
  function removeChildrenByClassName(element, className) {
    for (var child = element.firstElementChild; child;) {
      var prev = child;
      child = child.nextElementSibling;
      if (prev.classList.contains(className))
        element.removeChild(prev);
    }
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
  function getAppsCallback(data)
  {
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
    appEvents.removeAll();

    // Clear any existing apps pages and dots.
    // TODO(rbyers): It might be nice to preserve animation of dots after an
    // uninstall. Could we re-use the existing page and dot elements?  It seems
    // unfortunate to have Chrome send us the entire apps list after an
    // uninstall.
    removeChildrenByClassName(appsPageList, 'apps-page');
    removeChildrenByClassName(dotList, 'dot');

    // Get the array of apps and add any special synthesized entries
    var apps = data.apps;
    apps.push(makeWebstoreApp());

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
        createAppPage();
        // Confirm that appsPages is a live object, updated when a new page is
        // added (otherwise we'd have an infinite loop)
        assert(appsPages.length == origPageCount + 1, 'expected new page');
      }
      appendApp(appsPages[pageIndex], app);
    }

    // Tell the slider about the pages
    updateSliderCards();

    // Mark the current page
    dots[slider.currentCard].classList.add('selected');
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
    // Allow standalone_hack.js to hook this mapping (since chrome:// URLs
    // won't work for a standalone page)
    if (typeof themeUrlMapper == 'function') {
      var u = themeUrlMapper(resourceName);
      if (u)
        return u;
    }
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
    var pageNo = slider.currentCard;
    if (pageNo >= appsPages.length)
      pageNo = appsPages.length - 1;
    var pageArray = [];
    for (var i = 0; i < appsPages.length; i++)
      pageArray[i] = appsPages[i];
    slider.setCards(pageArray, pageNo);
  }

  /**
   * Create a new app element and attach it to the end of the specified app
   * page.
   * @param {!Element} parent The element where the app should be inserted.
   * @param {!Object} app The application object to create an app for.
   */
  function appendApp(parent, app) {
    // Make a deep copy of the template and clear its ID
    var containerElement = appTemplate.cloneNode(true);
    var appElement = containerElement.getElementsByClassName('app')[0];
    assert(appElement, 'Expected app-template to have an app child');
    assert(typeof(app.id) == 'string',
           'Expected every app to have an ID or empty string');
    appElement.setAttribute('app-id', app.id);

    // Find the span element (if any) and fill it in with the app name
    var span = appElement.querySelector('span');
    if (span)
      span.textContent = app.name;

    // Fill in the image
    // We use a mask of the same image so CSS rules can highlight just the image
    // when it's touched.
    var appImg = appElement.querySelector('img');
    if (appImg) {
      appImg.src = app.icon_big;
      appImg.style.webkitMaskImage = url(app.icon_big);
      // We put a click handler just on the app image - so clicking on the
      // margins between apps doesn't do anything
      if (app.id) {
        appEvents.add(appImg, 'click', appClick, false);
      } else {
        // Special case of synthesized apps - can't launch directly so just
        // change the URL as if we clicked a link.  We may want to eventually
        // support tracking clicks with ping messages, but really it seems it
        // would be better for the back-end to just create virtual apps for such
        // cases.
        appEvents.add(appImg, 'click', function(e) {
          window.location = app.launch_url;
        }, false);
      }
    }

    // Only real apps with back-end storage (for their launch index, etc.) can
    // be rearranged.
    if (app.id) {
      // Create a grabber to support moving apps around
      // Note that we move the app rather than the container. This is so that an
      // element remains in the original position so we can detect when an app
      // is dropped in its starting location.
      var grabber = new Grabber(appElement);
      grabbers.push(grabber);

      // Register to be made aware of when we are dragged
      appEvents.add(appElement, Grabber.EventType.DRAG_START, appDragStart,
                    false);
      appEvents.add(appElement, Grabber.EventType.DRAG_END, appDragEnd,
                    false);

      // Register to be made aware of any app drags on top of our container
      appEvents.add(containerElement, Grabber.EventType.DRAG_ENTER,
          appDragEnter, false);
    } else {
      // Prevent any built-in drag-and-drop support from activating for the
      // element.
      appEvents.add(appElement, 'dragstart', function(e) {
        e.preventDefault();
      }, true);
    }

    // Insert at the end of the provided page
    parent.appendChild(containerElement);
  }

  /**
   * Creates a new page for apps
   *
   * @return {!Element} The apps-page element created.
   * @param {boolean=} opt_animate If true, add the class 'new' to the created
   *        dot.
   */
  function createAppPage(opt_animate)
  {
    // Make a shallow copy of the app page template.
    var newPage = appsPageTemplate.cloneNode(false);
    appsPageList.appendChild(newPage);

    // Make a deep copy of the dot template to add a new one.
    var dotCount = dots.length;
    var newDot = dotTemplate.cloneNode(true);
    if (opt_animate)
      newDot.classList.add('new');
    dotList.appendChild(newDot);

    // Add click handler to the dot to change the page.
    // TODO(rbyers): Perhaps this should be TouchHandler.START_EVENT_ (so we
    // don't rely on synthesized click events, and the change takes effect
    // before releasing). However, click events seems to be synthesized for a
    // region outside the border, and a 10px box is too small to require touch
    // events to fall inside of. We could get around this by adding a box around
    // the dot for accepting the touch events.
    function switchPage(e) {
      slider.selectCard(dotCount, true);
      e.stopPropagation();
    }
    appEvents.add(newDot, 'click', switchPage, false);

    // Change pages whenever an app is dragged over a dot.
    appEvents.add(newDot, Grabber.EventType.DRAG_ENTER, switchPage, false);

    return newPage;
  }

  /**
   * Invoked when an app is clicked
   * @param {Event} e The click event.
   */
  function appClick(e) {
    var target = e.currentTarget;
    var app = getParentByClassName(target, 'app');
    assert(app, 'appClick should have been on a descendant of an app');

    var appId = app.getAttribute('app-id');
    assert(appId, 'unexpected app without appId');

    // Tell chrome to launch the app.
    var NTP_APPS_MAXIMIZED = 0;
    chrome.send('launchApp', [appId, NTP_APPS_MAXIMIZED]);

    // Don't allow the click to trigger a link or anything
    e.preventDefault();
  }

  /**
   * Search an elements ancestor chain for the nearest element that is a member
   * of the specified class.
   * @param {!Element} element The element to start searching from.
   * @param {string} className The name of the class to locate.
   * @return {Element} The first ancestor of the specified class or null.
   */
  function getParentByClassName(element, className)
  {
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
    // Pull the element out to the appsFrame using fixed positioning. This
    // ensures that the app is not affected (remains under the finger) if the
    // slider changes cards and is translated.  An alternate approach would be
    // to use fixed positioning for the slider (so that changes to its position
    // don't affect children that aren't positioned relative to it), but we
    // don't yet have GPU acceleration for this.  Note that we use the appsFrame
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
    getRequiredElement('apps-frame').appendChild(element);
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

    var curPage = appsPages[slider.currentCard];
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
    var pageIndex = slider.currentCard;
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
    slider.cancelTouch();

    // Add an extra blank page in case the user wants to create a new page
    createAppPage(true);
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
  function removePage(pageNo)
  {
    var page = appsPages[pageNo];

    // Remove the page from the DOM
    page.parentNode.removeChild(page);

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

  // Return an object with all the exports
  return {
    assert: assert,
    appsPrefChangeCallback: appsPrefChangeCallback,
    getAppsCallback: getAppsCallback,
    initialize: initializeNtp
  };
})();

// publish ntp globals
var assert = ntp.assert;
var getAppsCallback = ntp.getAppsCallback;
var appsPrefChangeCallback = ntp.appsPrefChangeCallback;

// Initialize immediately once globals are published (there doesn't seem to be
// any need to wait for DOMContentLoaded)
ntp.initialize();
