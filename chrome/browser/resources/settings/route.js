// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Class for navigable routes. May only be instantiated within this file.
   * @constructor
   * @param {string} path
   * @private
   */
  var Route = function(path) {
    this.path = path;

    /** @type {?settings.Route} */
    this.parent = null;

    /** @type {number} */
    this.depth = 0;

    /**
     * @type {boolean} Whether this route corresponds to a navigable
     *     dialog. Those routes don't belong to a "section".
     */
    this.isNavigableDialog = false;

    // Below are all legacy properties to provide compatibility with the old
    // routing system.

    /** @type {string} */
    this.section = '';
  };

  Route.prototype = {
    /**
     * Returns a new Route instance that's a child of this route.
     * @param {string} path Extends this route's path if it doesn't contain a
     *     leading slash.
     * @return {!settings.Route}
     * @private
     */
    createChild: function(path) {
      assert(path);

      // |path| extends this route's path if it doesn't have a leading slash.
      // If it does have a leading slash, it's just set as the new route's URL.
      var newUrl = path[0] == '/' ? path : this.path + '/' + path;

      var route = new Route(newUrl);
      route.parent = this;
      route.section = this.section;
      route.depth = this.depth + 1;

      return route;
    },

    /**
     * Returns a new Route instance that's a child section of this route.
     * TODO(tommycli): Remove once we've obsoleted the concept of sections.
     * @param {string} path
     * @param {string} section
     * @return {!settings.Route}
     * @private
     */
    createSection: function(path, section) {
      var route = this.createChild(path);
      route.section = section;
      return route;
    },

    /**
     * Returns true if this route matches or is an ancestor of the parameter.
     * @param {!settings.Route} route
     * @return {boolean}
     */
    contains: function(route) {
      for (var r = route; r != null; r = r.parent) {
        if (this == r)
          return true;
      }
      return false;
    },

    /**
     * Returns true if this route is a subpage of a section.
     * @return {boolean}
     */
    isSubpage: function() {
      return !!this.parent && !!this.section &&
          this.parent.section == this.section;
    },
  };

  // Abbreviated variable for easier definitions.
  var r = Route;

  // Root pages.
  r.BASIC = new Route('/');
  r.ADVANCED = new Route('/advanced');
  r.ABOUT = new Route('/help');

  // Navigable dialogs. These are the only non-section children of root pages.
  // These are disfavored. If we add anymore, we should add explicit support.
  r.IMPORT_DATA = r.BASIC.createChild('/importData');
  r.IMPORT_DATA.isNavigableDialog = true;
  r.SIGN_OUT = r.BASIC.createChild('/signOut');
  r.SIGN_OUT.isNavigableDialog = true;
  r.CLEAR_BROWSER_DATA = r.ADVANCED.createChild('/clearBrowserData');
  r.CLEAR_BROWSER_DATA.isNavigableDialog = true;
  r.RESET_DIALOG = r.ADVANCED.createChild('/resetProfileSettings');
  r.RESET_DIALOG.isNavigableDialog = true;
  r.TRIGGERED_RESET_DIALOG =
      r.ADVANCED.createChild('/triggeredResetProfileSettings');
  r.TRIGGERED_RESET_DIALOG.isNavigableDialog = true;

  // <if expr="chromeos">
  r.INTERNET = r.BASIC.createSection('/internet', 'internet');
  r.INTERNET_NETWORKS = r.INTERNET.createChild('/networks');
  r.NETWORK_CONFIG = r.INTERNET.createChild('/networkConfig');
  r.NETWORK_DETAIL = r.INTERNET.createChild('/networkDetail');
  r.KNOWN_NETWORKS = r.INTERNET.createChild('/knownNetworks');
  r.BLUETOOTH = r.BASIC.createSection('/bluetooth', 'bluetooth');
  r.BLUETOOTH_DEVICES = r.BLUETOOTH.createChild('/bluetoothDevices');
  // </if>

  r.APPEARANCE = r.BASIC.createSection('/appearance', 'appearance');
  r.FONTS = r.APPEARANCE.createChild('/fonts');

  r.DEFAULT_BROWSER =
      r.BASIC.createSection('/defaultBrowser', 'defaultBrowser');

  r.SEARCH = r.BASIC.createSection('/search', 'search');
  r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines');

  // <if expr="chromeos">
  r.ANDROID_APPS = r.BASIC.createSection('/androidApps', 'androidApps');
  r.ANDROID_APPS_DETAILS = r.ANDROID_APPS.createChild('/androidApps/details');
  // </if>

  r.ON_STARTUP = r.BASIC.createSection('/onStartup', 'onStartup');

  r.PEOPLE = r.BASIC.createSection('/people', 'people');
  r.SYNC = r.PEOPLE.createChild('/syncSetup');
  // <if expr="not chromeos">
  r.MANAGE_PROFILE = r.PEOPLE.createChild('/manageProfile');
  // </if>
  // <if expr="chromeos">
  r.CHANGE_PICTURE = r.PEOPLE.createChild('/changePicture');
  r.ACCOUNTS = r.PEOPLE.createChild('/accounts');
  r.LOCK_SCREEN = r.PEOPLE.createChild('/lockScreen');
  r.FINGERPRINT = r.LOCK_SCREEN.createChild('/lockScreen/fingerprint');

  r.DEVICE = r.BASIC.createSection('/device', 'device');
  r.POINTERS = r.DEVICE.createChild('/pointer-overlay');
  r.KEYBOARD = r.DEVICE.createChild('/keyboard-overlay');
  r.STYLUS = r.DEVICE.createChild('/stylus');
  r.DISPLAY = r.DEVICE.createChild('/display');
  r.STORAGE = r.DEVICE.createChild('/storage');
  r.POWER = r.DEVICE.createChild('/power');
  // </if>

  r.PRIVACY = r.ADVANCED.createSection('/privacy', 'privacy');
  r.CERTIFICATES = r.PRIVACY.createChild('/certificates');

  r.SITE_SETTINGS = r.PRIVACY.createChild('/content');

  if (loadTimeData.getBoolean('enableSiteSettings')) {
    r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all');
    r.SITE_SETTINGS_SITE_DETAILS =
        r.SITE_SETTINGS_ALL.createChild('/content/siteDetails');
  } else if (loadTimeData.getBoolean('enableSiteDetails')) {
    // When there is no "All Sites", pressing 'back' from "Site Details" should
    // return to "Content Settings". This should only occur when |kSiteSettings|
    // is off and |kSiteDetails| is on.
    r.SITE_SETTINGS_SITE_DETAILS =
        r.SITE_SETTINGS.createChild('/content/siteDetails');
  }

  r.SITE_SETTINGS_HANDLERS = r.SITE_SETTINGS.createChild('/handlers');

  // TODO(tommycli): Find a way to refactor these repetitive category routes.
  r.SITE_SETTINGS_ADS = r.SITE_SETTINGS.createChild('ads');
  r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS =
      r.SITE_SETTINGS.createChild('automaticDownloads');
  r.SITE_SETTINGS_BACKGROUND_SYNC =
      r.SITE_SETTINGS.createChild('backgroundSync');
  r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild('camera');
  r.SITE_SETTINGS_COOKIES = r.SITE_SETTINGS.createChild('cookies');
  r.SITE_SETTINGS_DATA_DETAILS =
      r.SITE_SETTINGS_COOKIES.createChild('/cookies/detail');
  r.SITE_SETTINGS_IMAGES = r.SITE_SETTINGS.createChild('images');
  r.SITE_SETTINGS_JAVASCRIPT = r.SITE_SETTINGS.createChild('javascript');
  r.SITE_SETTINGS_LOCATION = r.SITE_SETTINGS.createChild('location');
  r.SITE_SETTINGS_MICROPHONE = r.SITE_SETTINGS.createChild('microphone');
  r.SITE_SETTINGS_NOTIFICATIONS = r.SITE_SETTINGS.createChild('notifications');
  r.SITE_SETTINGS_FLASH = r.SITE_SETTINGS.createChild('flash');
  r.SITE_SETTINGS_POPUPS = r.SITE_SETTINGS.createChild('popups');
  r.SITE_SETTINGS_UNSANDBOXED_PLUGINS =
      r.SITE_SETTINGS.createChild('unsandboxedPlugins');
  r.SITE_SETTINGS_MIDI_DEVICES = r.SITE_SETTINGS.createChild('midiDevices');
  r.SITE_SETTINGS_USB_DEVICES = r.SITE_SETTINGS.createChild('usbDevices');
  r.SITE_SETTINGS_ZOOM_LEVELS = r.SITE_SETTINGS.createChild('zoomLevels');
  r.SITE_SETTINGS_PDF_DOCUMENTS = r.SITE_SETTINGS.createChild('pdfDocuments');
  r.SITE_SETTINGS_PROTECTED_CONTENT =
      r.SITE_SETTINGS.createChild('protectedContent');

  // <if expr="chromeos">
  r.DATETIME = r.ADVANCED.createSection('/dateTime', 'dateTime');
  // </if>

  r.PASSWORDS =
      r.ADVANCED.createSection('/passwordsAndForms', 'passwordsAndForms');
  r.AUTOFILL = r.PASSWORDS.createChild('/autofill');
  r.MANAGE_PASSWORDS = r.PASSWORDS.createChild('/passwords');

  r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
  // <if expr="chromeos">
  r.INPUT_METHODS = r.LANGUAGES.createChild('/inputMethods');
  // </if>
  // <if expr="not is_macosx">
  r.EDIT_DICTIONARY = r.LANGUAGES.createChild('/editDictionary');
  // </if>

  r.DOWNLOADS = r.ADVANCED.createSection('/downloads', 'downloads');

  r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');
  r.CLOUD_PRINTERS = r.PRINTING.createChild('/cloudPrinters');
  // <if expr="chromeos">
  r.CUPS_PRINTERS = r.PRINTING.createChild('/cupsPrinters');
  r.CUPS_PRINTER_DETAIL = r.CUPS_PRINTERS.createChild('/cupsPrinterDetails');
  // </if>

  r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');
  // <if expr="chromeos">
  r.MANAGE_ACCESSIBILITY = r.ACCESSIBILITY.createChild('/manageAccessibility');
  // </if>

  r.SYSTEM = r.ADVANCED.createSection('/system', 'system');
  r.RESET = r.ADVANCED.createSection('/reset', 'reset');

  // <if expr="chromeos">
  // "About" is the only section in About, but we still need to create the route
  // in order to show the subpage on Chrome OS.
  r.ABOUT_ABOUT = r.ABOUT.createSection('/help/about', 'about');
  r.DETAILED_BUILD_INFO = r.ABOUT_ABOUT.createChild('/help/details');
  // </if>

  var routeObservers = new Set();

  /** @polymerBehavior */
  var RouteObserverBehavior = {
    /** @override */
    attached: function() {
      assert(!routeObservers.has(this));
      routeObservers.add(this);

      // Emulating Polymer data bindings, the observer is called when the
      // element starts observing the route.
      this.currentRouteChanged(currentRoute, undefined);
    },

    /** @override */
    detached: function() {
      assert(routeObservers.delete(this));
    },

    /**
     * @param {!settings.Route|undefined} opt_newRoute
     * @param {!settings.Route|undefined} opt_oldRoute
     * @abstract
     */
    currentRouteChanged: function(opt_newRoute, opt_oldRoute) {
      assertNotReached();
    },
  };

  /**
   * Regular expression that captures the leading slash, the content and the
   * trailing slash in three different groups.
   * @const {!RegExp}
   */
  var CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

  /**
   * @param {string} path
   * @return {?settings.Route} The matching canonical route, or null if none
   *     matches.
   */
  var getRouteForPath = function(path) {
    // Allow trailing slash in paths.
    var canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

    // TODO(tommycli): Use Object.values once Closure compilation supports it.
    var matchingKey = Object.keys(Route).find(function(key) {
      return Route[key].path == canonicalPath;
    });

    return !!matchingKey ? Route[matchingKey] : null;
  };

  /**
   * The current active route. This updated is only by settings.navigateTo or
   * settings.initializeRouteFromUrl.
   * @private {!settings.Route}
   */
  var currentRoute = Route.BASIC;

  /**
   * The current query parameters. This is updated only by settings.navigateTo
   * or settings.initializeRouteFromUrl.
   * @private {!URLSearchParams}
   */
  var currentQueryParameters = new URLSearchParams();

  /** @private {boolean} */
  var wasLastRouteChangePopstate = false;

  /** @private */
  var initializeRouteFromUrlCalled = false;

  /**
   * Initialize the route and query params from the URL.
   */
  var initializeRouteFromUrl = function() {
    assert(!initializeRouteFromUrlCalled);
    initializeRouteFromUrlCalled = true;

    var route = getRouteForPath(window.location.pathname);
    // Never allow direct navigation to ADVANCED.
    if (route && route != Route.ADVANCED) {
      currentRoute = route;
      currentQueryParameters = new URLSearchParams(window.location.search);
    } else {
      window.history.replaceState(undefined, '', Route.BASIC.path);
    }
  };

  function resetRouteForTesting() {
    initializeRouteFromUrlCalled = false;
    wasLastRouteChangePopstate = false;
    currentRoute = Route.BASIC;
    currentQueryParameters = new URLSearchParams();
  }

  /**
   * Helper function to set the current route and notify all observers.
   * @param {!settings.Route} route
   * @param {!URLSearchParams} queryParameters
   * @param {boolean} isPopstate
   */
  var setCurrentRoute = function(route, queryParameters, isPopstate) {
    var oldRoute = currentRoute;
    currentRoute = route;
    currentQueryParameters = queryParameters;
    wasLastRouteChangePopstate = isPopstate;
    routeObservers.forEach(function(observer) {
      observer.currentRouteChanged(currentRoute, oldRoute);
    });
  };

  /** @return {!settings.Route} */
  var getCurrentRoute = function() {
    return currentRoute;
  };

  /** @return {!URLSearchParams} */
  var getQueryParameters = function() {
    return new URLSearchParams(currentQueryParameters);  // Defensive copy.
  };

  /** @return {boolean} */
  var lastRouteChangeWasPopstate = function() {
    return wasLastRouteChangePopstate;
  };

  /**
   * Navigates to a canonical route and pushes a new history entry.
   * @param {!settings.Route} route
   * @param {URLSearchParams=} opt_dynamicParameters Navigations to the same
   *     URL parameters in a different order will still push to history.
   * @param {boolean=} opt_removeSearch Whether to strip the 'search' URL
   *     parameter during navigation. Defaults to false.
   */
  var navigateTo = function(route, opt_dynamicParameters, opt_removeSearch) {
    // The ADVANCED route only serves as a parent of subpages, and should not
    // be possible to navigate to it directly.
    if (route == settings.Route.ADVANCED)
      route = settings.Route.BASIC;

    var params = opt_dynamicParameters || new URLSearchParams();
    var removeSearch = !!opt_removeSearch;

    var oldSearchParam = getQueryParameters().get('search') || '';
    var newSearchParam = params.get('search') || '';

    if (!removeSearch && oldSearchParam && !newSearchParam)
      params.append('search', oldSearchParam);

    var url = route.path;
    var queryString = params.toString();
    if (queryString)
      url += '?' + queryString;

    // History serializes the state, so we don't push the actual route object.
    window.history.pushState(currentRoute.path, '', url);
    setCurrentRoute(route, params, false);
  };

  /**
   * Navigates to the previous route if it has an equal or lesser depth.
   * If there is no previous route in history meeting those requirements,
   * this navigates to the immediate parent. This will never exit Settings.
   */
  var navigateToPreviousRoute = function() {
    var previousRoute = window.history.state &&
        assert(getRouteForPath(/** @type {string} */ (window.history.state)));

    if (previousRoute && previousRoute.depth <= currentRoute.depth)
      window.history.back();
    else
      navigateTo(currentRoute.parent || Route.BASIC);
  };

  window.addEventListener('popstate', function(event) {
    // On pop state, do not push the state onto the window.history again.
    setCurrentRoute(
        getRouteForPath(window.location.pathname) || Route.BASIC,
        new URLSearchParams(window.location.search), true);
  });

  return {
    Route: Route,
    RouteObserverBehavior: RouteObserverBehavior,
    getRouteForPath: getRouteForPath,
    initializeRouteFromUrl: initializeRouteFromUrl,
    resetRouteForTesting: resetRouteForTesting,
    getCurrentRoute: getCurrentRoute,
    getQueryParameters: getQueryParameters,
    lastRouteChangeWasPopstate: lastRouteChangeWasPopstate,
    navigateTo: navigateTo,
    navigateToPreviousRoute: navigateToPreviousRoute,
  };
});
