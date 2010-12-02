// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file contains global declarations used by the CEEE Firefox add-on.
 * Global variables defined in overlay.js are global only to a single top-level
 * window, and not to the entire Firefox app.
 *
 * This javascript file is loaded into each top-level window using a concept
 * called javascript code modules.  This injects javascript code into the
 * namespace of each top-level window, such that any objects declared are
 * shared.  See these links for more info:
 *
 * https://developer.mozilla.org/en/Using_JavaScript_code_modules
 * https://developer.mozilla.org/en/Components.utils.import
 *
 * @supported Firefox 3.x
 */

/**
 * Declares the objects exported from this code module to those that import it.
 */
var EXPORTED_SYMBOLS = ['CEEE_bookmarks',
                        'CEEE_cookieManager',
                        'CEEE_globals',
                        'CEEE_ioService',
                        'CEEE_json',
                        'CEEE_mozilla_tabs',
                        'CEEE_mozilla_windows'];

/**
 * Used to encode and decode JSON.
 * @public
 */
CEEE_json = Components.classes['@mozilla.org/dom/json;1']
    .createInstance(Components.interfaces.nsIJSON);

/**
 * Used to access the Mozilla places services.
 * @public
 */
CEEE_bookmarks = Components
    .classes['@mozilla.org/browser/nav-bookmarks-service;1']
    .getService(Components.interfaces.nsINavBookmarksService);

/**
 * Used to access the Mozilla cookies service.
 * @public
 */
CEEE_cookieManager = Components
    .classes['@mozilla.org/cookiemanager;1']
    .getService(Components.interfaces.nsICookieManager2);

/**
 * Used to access the Mozilla io services, for construction of url objects.
 * @public
 */
CEEE_ioService = Components
    .classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);

/**
 * Global object used as a place-holder for API routines accessing the Mozilla
 * windows interface.
 * @public
 */
var CEEE_mozilla_windows = CEEE_mozilla_windows || {
  /** @const */ WINDOW_TYPE: 'navigator:browser',
  /** @const */ WINDOW_ID: 'ceee_winid',

  /**
   * Used to access top-level browser windows.
   * @public
   */
  service: Components
      .classes['@mozilla.org/appshell/window-mediator;1']
      .getService(Components.interfaces.nsIWindowMediator)
};

/**
 * Get the unique id of the given window.  If the window does not have
 * one, it is assigned one now.
 * @public
 */
CEEE_mozilla_windows.getWindowId = function(win) {
  if (!win[this.WINDOW_ID]) {
    win[this.WINDOW_ID] = CEEE_globals.getNextWindowId();
  }
  return win[this.WINDOW_ID];
};

/**
 * Find the window with the given id.
 *
 * @param {number} id The Id of the window to find.  If id is null, then
 *     get last focused window.
 * @return The window whose Id matches the argument, or null if no window
 *     is found.
 * @public
 */
CEEE_mozilla_windows.findWindow = function(id) {
  if (id) {
    var e = this.service.getEnumerator(this.WINDOW_TYPE);
    while (e.hasMoreElements()) {
      var win = e.getNext();
      if (this.getWindowId(win) == id)
        return win;
    }
  } else {
    return this.service.getMostRecentWindow(this.WINDOW_TYPE);
  }
};

/**
 * Find a top level window from the given content window.  I don't know if
 * there is a more efficient method than this, so packaging this function up
 * as a method for now.
 *
 * @param {!Object} win Content window to use as start of search.
 * @private
 */
CEEE_mozilla_windows.findWindowFromContentWindow = function(win) {
  var d = win.document;
  var e = this.service.getEnumerator(this.WINDOW_TYPE);
  while (e.hasMoreElements()) {
    var w = e.getNext();
    var mainBrowser =
        w.document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
    if (mainBrowser) {
      var index = mainBrowser.getBrowserIndexForDocument(d);
      if (-1 != index) {
        return {
          'window': w,
          'tabBrowser': mainBrowser,
          'index': index
        };
      }
    }
  }
};

/**
 * Find the window with the given a Chrome Frame session id.
 *
 * @param {number} id The Chrome Frame session Id of the window to find.
 * @return The window that contains the Chrome Frame instance with the
 *     specified session Id, or null if no window is found.
 * @public
 */
CEEE_mozilla_windows.findWindowFromCfSessionId = function(id) {
  var e = this.service.getEnumerator(this.WINDOW_TYPE);
  while (e.hasMoreElements()) {
    var win = e.getNext();
    var cf = win.document.getElementById(CEEE_globals.CHROME_FRAME_ID);
    if (cf.sessionid == id)
      return win;
  }
  return null;
};

/**
 * Global object used as a place-holder for API routines accessing the Mozilla
 * tabs interface.
 * @public
 */
var CEEE_mozilla_tabs = CEEE_mozilla_tabs || {
  /** @const */ TAB_ID: 'ceee_tabid'
};

/**
 * Get the unique id of the given tab.  If the tab does not have
 * one, it is assigned one now.
 * @public
 */
CEEE_mozilla_tabs.getTabId = function(tab) {
  if (!tab[this.TAB_ID]) {
    tab[this.TAB_ID] = CEEE_globals.getNextTabId();
  }
  return tab[this.TAB_ID];
};

/**
 * Find the tab with the given id.
 *
 * @param {!number} id The Id of the tab to find.
 * @return If a tab with the given id is found, returns an object with four
 *     properties:
 *     tab: points to the found tab
 *     window: window that contains the tab
 *     tabBrowser: tabbed browser element that contains the tab
 *     index: index of tab in the tabbed browser
 * @public
 */
CEEE_mozilla_tabs.findTab = function(id) {
  var windowsService = CEEE_mozilla_windows.service;
  var e = windowsService.getEnumerator(CEEE_mozilla_windows.WINDOW_TYPE);
  while (e.hasMoreElements()) {
    var win = e.getNext();
    var mainBrowser = win.document.getElementById(
        CEEE_globals.MAIN_BROWSER_ID);
    if (mainBrowser) {
      var tabs = mainBrowser.tabContainer;
      var count = tabs.itemCount;
      for (var i = 0; i < count; ++i) {
        var tab = tabs.getItemAtIndex(i);
        if (CEEE_mozilla_tabs.getTabId(tab) == id)
          return {
            'tab': tab,
            'tabBrowser': mainBrowser,
            'window': win,
            'index': i
          };
      }
    }
  }
};


/**
 * Object to manage the master Chrome Frame instance used for
 * automating Chrome APIs.
 * @constructor
 */
var CEEE_MasterChromeFrame = function() {
  /**
   * Comma-separated list of functions to automate via the master CF.
   * @type {string}
   * @private
   */
  this.functionsToAutomate_ = '';

  /**
   * The Chrome Frame instance that is currently being used
   * as a conduit for extension API calls, or null if there
   * is currently no such instance.
   * @type {?Object}
   * @private
   */
  this.currentMasterCf_ = null;

  /**
   * A list of all Chrome Frame instances currently ready for use.
   * @type {Array.<Object>}
   * @private
   */
  this.allCfs_ = [];
};

/**
 * Initialize this object.  Idempotent.
 *
 * @param {string} functionsToAutomate Comma-separated list of functions
 *     for which to enable automation.
 * @param {?function(string)} opt_logger Logger function to use for
 *     informational logging.
 */
CEEE_MasterChromeFrame.prototype.init = function(functionsToAutomate,
                                                 opt_logger) {
  if (opt_logger)
    opt_logger('masterCf.init(): ' + functionsToAutomate);
  this.functionsToAutomate_ = functionsToAutomate;
};

/**
 * Returns the current master Chrome Frame instance, or null.
 * @return {?Object}
 */
CEEE_MasterChromeFrame.prototype.get = function() {
  return this.currentMasterCf_;
};

/**
 * Call this every time a new CF instance becomes ready.
 * @param {!Object} cf The Chrome Frame instance.
 * @param {?function(string)} opt_logger Logger function to use for
 *     informational logging.
 */
CEEE_MasterChromeFrame.prototype.onChromeFrameReady = function(cf, opt_logger) {
  if (!opt_logger)
    opt_logger = function(msg) {};
  opt_logger('masterCf.onChromeFrameReady: ' + cf);
  this.allCfs_.push(cf);
  if (!this.currentMasterCf_) {
    opt_logger('masterCf.onChromeFrameReady initing: ' +
               this.functionsToAutomate_);
    this.currentMasterCf_ = cf;
    cf.enableExtensionAutomation(this.functionsToAutomate_);
  }
};

/**
 * Call this every time a CF instance is going away.
 * @param {!Object} cf The Chrome Frame instance.
 * @param {?function(string)} opt_logger Logger function to use for
 *     informational logging.
 */
CEEE_MasterChromeFrame.prototype.onChromeFrameGoing = function(cf, opt_logger) {
  if (!opt_logger)
    opt_logger = function(msg) {};
  opt_logger('masterCf.onChromeFrameGoing: ' + cf);
  for (var i = 0; i < this.allCfs_.length; ++i) {
    if (this.allCfs_[i] == cf) {
      opt_logger('masterCf.onChromeFrameGoing, removing');
      this.allCfs_.splice(i, 1);
      break;
    }
  }

  if (cf == this.currentMasterCf_ && cf != null) {
    this.currentMasterCf_ = null;
    if (this.allCfs_.length > 0) {
      this.currentMasterCf_ = this.allCfs_[0];
    }

    // Reset automation in the previous master that's going away, and if
    // a new master was chosen, enable automation through it.
    opt_logger('masterCf.onChromeFrameGoing, resetting');
    cf.enableExtensionAutomation();
    if (this.currentMasterCf_) {
      opt_logger('masterCf.onChromeFrameGoing, initing new: ' +
                 this.functionsToAutomate_);
      this.currentMasterCf_.enableExtensionAutomation(
          this.functionsToAutomate_);
    }
  }
};

/**
 * Class to handle Private Browsing notifications. Follows example at
 * https://developer.mozilla.org/En/Supporting_private_browsing_mode.
 * @constructor
 */
var CEEE_PrivateBrowsingService = function() {
  /**
   * Observer for private browsing notifications.
   * @type {Object}
   * @private
   */
  this.observerService_ = null;

  /**
   * Whether we are in Private Browsing mode.
   * @type {boolean}
   */
  this.isInPrivateBrowsing = false;

  // Register for notifications.
  this.init();
};

/**
 * Initializes Private Browsing Listener.
 */
CEEE_PrivateBrowsingService.prototype.init = function() {
  this.observerService_ = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
  this.observerService_.addObserver(this, "private-browsing", false);
  this.observerService_.addObserver(this, "quit-application", false);
  try {
    var privateBrowsingService =
        Components.classes["@mozilla.org/privatebrowsing;1"]
        .getService(Components.interfaces.nsIPrivateBrowsingService);
    this.isInPrivateBrowsing = privateBrowsingService.privateBrowsingEnabled;
  } catch(ex) {
    // Ignore exceptions in older versions of Firefox.
  }
};

/**
 * Callback for private-browsing notifications.
 * @param {string} subject Event subject.
 * @param {string} topic Event topic.
 * @param {string} data Event data.
 */
CEEE_PrivateBrowsingService.prototype.observe = function(subject, topic,
                                                         data) {
  if (topic == "private-browsing") {
    if (data == "enter") {
      this.isInPrivateBrowsing = true;
    } else if (data == "exit") {
      this.isInPrivateBrowsing = false;
    }
  } else if (topic == "quit-application") {
    this.observerService_.removeObserver(this, "quit-application");
    this.observerService_.removeObserver(this, "private-browsing");
  }
};


/**
 * A class to hold all global variables and methods.
 */
var CEEE_globals = CEEE_globals || {
  /** @const */ MAIN_BROWSER_ID: 'content',
  /** @const */ ADDON_ID: 'ceee@google.com',

  /**
   * Value used for id attribute of the ChromeFrame <embed> element.
   * @const
   */
  CHROME_FRAME_ID: 'ceee-browser',

  /** The integer id for the next window. @private */
  nextWindowId_: 0,

  /** The integer id for the next tab. @private */
  nextTabId_: 0,

  /** The integer id for the next tab. @private */
  nextPortId_: 0,

  /** Preferences object for CEEE. @private */
  prefs_: Components.classes['@mozilla.org/preferences-service;1']
      .getService(Components.interfaces.nsIPrefService)
      .getBranch('extensions.ceee.'),

  /**
   * The ChromeFrame instance used for all communication of API messages
   * to and from Chrome.  This instance is shared among all the top level
   * windows open in Firefox, and may potentially be switched on the fly
   * if the window containing this ChromeFrame is closed.
   */
  masterCf: new CEEE_MasterChromeFrame(),

  /**
   * Map of all message ports opened from users scripts to the extension.
   * Maps a port Id, as returned by getNextPortId() below, to an object
   * containing context information about the port.  See userscript_api.js
   * for a description of the information.
   */
  ports: {},

  /**
   * An array of "content_scripts", as defined by the Chrome Extension API.
   * Other properties may also be added by CEEE.
   */
  scripts: [],

  /**
   * Receives notifications on Firefox Private Browsing changes.
   * @type {CEEE_PrivateBrowsingService}
   */
  privateBrowsingService: new CEEE_PrivateBrowsingService()
};

/**
 * Returns an id to be used for a top-level window.  This id is guaranteed to
 * be unique from any other window id returned.
 */
CEEE_globals.getNextWindowId = function() {
  return ++this.nextWindowId_;
};

/**
 * Returns an id to be used for a tab, regardless of the window in which that
 * tab exists.  This id is guaranteed to be unique from any other tab id
 * returned.
 */
CEEE_globals.getNextTabId = function() {
  return ++this.nextTabId_;
};

/**
 * Returns an id to be used for a port, regardless of tab, extension, or user
 * script.  This id is guaranteed to be unique from any other port id returned.
 */
CEEE_globals.getNextPortId = function() {
  return ++this.nextPortId_;
};

/**
 * Get a CEEE string preference.
 * @param {string} name The name of the string preference.
 * @return {string} The value of the named preference.
 */
CEEE_globals.getCharPreference = function(name) {
  return this.prefs_.getCharPref(name);
};

/**
 * Get a CEEE integer preference.
 * @param {string} name The name of the preference.
 * @return {integer} The value of the named preference.
 */
CEEE_globals.getIntPreference = function(name) {
  return this.prefs_.getIntPref(name);
};

/**
 * Get a CEEE boolean preference.
 * @param {string} name The name of the string preference.
 * @return {boolean} The value of the named preference.
 */
CEEE_globals.getBoolPreference = function(name) {
  return this.prefs_.getBoolPref(name);
};

/**
 * Set a CEEE string preference.
 * @param {string} name The name of the preference.
 * @param {string} value The value of the preference.
 */
CEEE_globals.setCharPreference = function(name, value) {
  return this.prefs_.setCharPref(name, value);
};

/**
 * Set a CEEE integer preference.
 * @param {string} name The name of the preference.
 * @param {integer} value The value of the preference.
 */
CEEE_globals.setIntPreference = function(name, value) {
  return this.prefs_.setIntPref(name, value);
};

/**
 * Set a CEEE bool preference.
 * @param {string} name The name of the preference.
 * @param {boolean} value The value of the preference.
 */
CEEE_globals.setBoolPreference = function(name, value) {
  return this.prefs_.setBoolPref(name, value);
};

/**
 * Creates a closure for a given function against a given 'this' object
 * and additional arguments (same as Function.prototype.bind in Gecko 1.8.5).
 * Actual agruments of the call are the initial arguments given at 'hitch' call
 * plus agruments given on the actual closure call.
 * @param {function(?)} func Function to create a closure of.
 * @param {Object} obj 'this' object for the function.
 * @return {function(?)} A closure of a first argument.
 */
CEEE_globals.hitch = function(func, obj /* , arg1, arg2,... */) {
  var args = Array.prototype.slice.call(arguments, 2);
  return function() {
    var a = Array.prototype.slice.call(arguments);
    return func.apply(obj, args.concat(a));
  };
};
