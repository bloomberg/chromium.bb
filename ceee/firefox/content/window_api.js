// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * windows APIs for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */

var CEEE_window_internal_ = CEEE_window_internal_ || {
  /**
    * Reference to the instance object for the CEEE.
    * @private
    */
  ceeeInstance_: null,

  /**
   * Log an informational message to the Firefox error console.
   * @private
   */
  logInfo_: null,

  /**
   * Log an error message to the Firefox error console.
   * @private
   */
  logError_: null,

  /**
   * List of additional window sources. For example, infobars API is supposed
   * to return window objects which don't correspond to top-level Firefox
   * windows. Each element is a function which takes window id and returns a
   * window-like object.
   * @type Array.<function(number):Object>
   */
  fakeWindowLookup_: []
};

/**
 * Build an object that represents a "window", as defined by the Google Chrome
 * extension API.
 *
 * @param {!Object} win Firefox chrome window.
 * @param {boolean} opt_populate If specified and true, populate the returned
 *     info object with information about the tabs in this window.
 */
CEEE_window_internal_.buildWindowValue = function(win, opt_populate) {
  var winRecent = CEEE_mozilla_windows.service.getMostRecentWindow(
      CEEE_mozilla_windows.WINDOW_TYPE);
  var info = {};
  info.id = CEEE_mozilla_windows.getWindowId(win);
  info.focused = (win ==
      winRecent.QueryInterface(Components.interfaces.nsIDOMWindow));
  info.top = win.screenY;
  info.left = win.screenX;
  info.width = win.outerWidth;
  info.height = win.outerHeight;
  info.incognito = CEEE_globals.privateBrowsingService.isInPrivateBrowsing;
  // Type can be any of 'normal', 'popup' or 'app'. Firefox doesn't support app
  // windows, and we use chrome to create popups so Firefox doesn't know about
  // them (i.e. all windows will be of type 'normal').
  // TODO(mad@chromium.org): detect popups now that we support
  // them. Not sure how though...
  info.type = 'normal';

  if (opt_populate) {
    info.tabs = [];
    var mainBrowser =
        win.document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
    var container = mainBrowser.tabContainer;

    for (var i = 0; i < container.itemCount; ++i) {
      var tab = container.getItemAtIndex(i);
      var tabInfo = this.ceeeInstance_.getTabModule().buildTabValue(
          mainBrowser, tab);
      info.tabs.push(tabInfo);
    }
  }

  return info;
};

/**
 * Called when a new top-level window is created.
 *
 * @param w The top-level window that was created.
 * @private
 */
CEEE_window_internal_.onWindowOpened_ = function(w) {
  // Send the event notification to ChromeFrame.
  var info = [this.buildWindowValue(w)];
  var msg = ['windows.onCreated', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Called when a new top-level window is closed.
 *
 * @param w The top-level window that was closed.
 * @private
 */
CEEE_window_internal_.onWindowRemoved_ = function(w) {
  // Send the event notification to ChromeFrame.
  var info = [CEEE_mozilla_windows.getWindowId(w)];
  var msg = ['windows.onRemoved', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Called when a top-level window gets focus.
 *
 * @param w The top-level window that got focus.
 * @private
 */
CEEE_window_internal_.onWindowFocused_ = function(w) {
  // Send the event notification to ChromeFrame.
  var info = [CEEE_mozilla_windows.getWindowId(w)];
  var msg = ['windows.onFocusChanged', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

CEEE_window_internal_.CMD_GET_WINDOW = 'windows.get';
CEEE_window_internal_.getWindow_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var win = CEEE_mozilla_windows.findWindow(id);
  if (!win) {
    throw(new Error(CEEE_window_internal_.CMD_GET_WINDOW +
                    ': invalid window id=' + id));
  }

  return this.buildWindowValue(win);
};

CEEE_window_internal_.CMD_GET_CURRENT_WINDOW = 'windows.getCurrent';
CEEE_window_internal_.getCurrentWindow_ = function(cmd, data) {
  var tab = data.tab;
  if (tab) {
    var win = CEEE_mozilla_windows.findWindowFromCfSessionId(tab.id);
    if (win)
      return this.buildWindowValue(win);
  }
};

CEEE_window_internal_.CMD_GET_LAST_FOCUSED_WINDOW =
    'windows.getLastFocused';
CEEE_window_internal_.getLastFocusedWindow_ = function(cmd, data) {
  var win = CEEE_mozilla_windows.service.getMostRecentWindow(
      CEEE_mozilla_windows.WINDOW_TYPE);
  return this.buildWindowValue(win);
};

CEEE_window_internal_.CMD_GET_ALL_WINDOWS = 'windows.getAll';
CEEE_window_internal_.getAllWindows_ = function(cmd, data) {
  var args = data.args && CEEE_json.decode(data.args);
  var populate = args && args[0] && args[0].populate;
  var e = CEEE_mozilla_windows.service.getEnumerator(
      CEEE_mozilla_windows.WINDOW_TYPE);
  var ret = [];
  while (e.hasMoreElements()) {
    var win = e.getNext();
    var info = this.buildWindowValue(win, populate);
    ret.push(info);
  }

  return ret;
};

CEEE_window_internal_.CMD_CREATE_WINDOW = 'windows.create';
CEEE_window_internal_.createWindow_ = function(cmd, data) {
  var args_list = data.args && CEEE_json.decode(data.args);
  var args = args_list && args_list[0];
  var url = '';
  if (args && args.url)
    url = args.url;

  // TODO(rogerta@chromium.org): need to properly validate URL string.
  // I noticed that if its a stripped down string, like 'google.ca',
  // it tries to open a file from the local hard disk like
  // jar:file:///C:/Program%20Files%20(x86)/Mozilla%20Firefox/chrome/
  //     browser.jar!/content/browser/google.ca

  // To avoid flickering, we set the position, size and popup attributes
  // as part of the features string of the open function.

  // Unfortunately, the behavior is a little weird here.
  // FF documentation excerpt:
  // https://developer.mozilla.org/En/DOM/Window.open
  //  "If you define the strWindowFeatures parameter, then the features
  //   that are not listed, requested in the string will be disabled or
  //   removed (except titlebar and close which are by default yes)."
  // So we must specify the default values ourselves based on current
  // window state.

  // We tried to find a way to dynamically enumerate the window
  // properties that have a visible sub-property but failed. This would
  // have allowed us to support new visibility features as they get added.
  // One of the problems with this approach is that the feature name is not
  // always the same as the window property name, like location[bar] or
  // status[bar]. Ho Well...

  // Build the feature object with some default values.
  var features = {
    toolbar: window.toolbar.visible ? 'yes' : 'no',
    location: window.locationbar.visible ? 'yes' : 'no',
    menubar: window.menubar.visible ? 'yes' : 'no',
    statusbar: window.menubar.visible ? 'yes' : 'no',
    personalbar: window.personalbar.visible ? 'yes' : 'no',
    scrollbars: window.scrollbars.visible ? 'yes' : 'no',
    resizable: 'yes'  // We can't get the current state of the window.
  };

  if (args) {
    if (args.top)
      features.top = args.top;
    if (args.left)
      features.left = args.left;
    if (args.width)
      features.outerWidth = args.width;
    if (args.height)
      features.outerHeight = args.height;
    if (args.type == 'popup') {
      features.toolbar = 'no';
      features.location = 'no';
      features.menubar = 'no';
      features.status = 'no';
    }
  }

  var options = [];
  for (var i in features) {
    options.push(i + '=' + features[i]);
  }
  options = options.join(',');

  // Window name of '_blank' means 'always open a new window'.  Note that the
  // return value of open() is the content window used to display the specified
  // URL, and not the top level navigator window we actually want here.  Hence
  // we need to map from the former to the latter.  I have verified that this
  // always opens a new top level window, even if the user has specified the
  // option "New pages should be opened in a new tab".
  var win = open(url, '_blank', options);
  var winTopLevel =
      CEEE_mozilla_windows.findWindowFromContentWindow(win).window;
  if (winTopLevel) {
    return this.buildWindowValue(winTopLevel);
  }

  throw(new Error(CEEE_window_internal_.CMD_CREATE_WINDOW +
                  ': error url=' + url));
};

CEEE_window_internal_.CMD_UPDATE_WINDOW = 'windows.update';
CEEE_window_internal_.updateWindow_ = function(cmd, data) {
  var args = data.args && CEEE_json.decode(data.args);
  if (!args || args.length < 2 || !args[1]) {
    throw(new Error(CEEE_window_internal_.CMD_UPDATE_WINDOW +
                    ': invalid arguments'));
  }

  var id = args[0];
  var win = CEEE_mozilla_windows.findWindow(id);
  if (!win) {
    throw(new Error(CEEE_window_internal_.CMD_UPDATE_WINDOW +
                    ': invalid window id=' + id));
  }

  var info = args[1];

  if (info.top)
    win.screenY = info.top;

  if (info.left)
    win.screenX = info.left;

  if (info.width)
    win.outerWidth = info.width;

  if (info.height)
    win.outerHeight = info.height;

  return this.buildWindowValue(win);
};

CEEE_window_internal_.CMD_REMOVE_WINDOW = 'windows.remove';
CEEE_window_internal_.removeWindow_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var win = CEEE_mozilla_windows.findWindow(id);
  var lookupList = CEEE_window_internal_.fakeWindowLookup_;
  for (var i = 0; !win && i < lookupList.length; ++i) {
    win = lookupList[i](id);
  }
  if (!win) {
    throw(new Error(CEEE_window_internal_.CMD_REMOVE_WINDOW +
                    ': invalid window id=' + id));
  }

  win.close();
};

/**
 * Add lookup function for fake windows (e.g. infobar). Lookup function is
 * passed window id and should return window-like object if there is a
 * correspondence.
 * @param {function(number):Object} lookup
 */
CEEE_window_internal_.registerLookup = function(lookup) {
  CEEE_window_internal_.fakeWindowLookup_.push(lookup);
};

/**
  * Initialization routine for the CEEE Windows API module.
  * @param {!Object} ceeeInstance Reference to the global ceee instance.
  * @return {Object} Reference to the window module.
  * @public
  */
function CEEE_initialize_windows(ceeeInstance) {
  CEEE_window_internal_.ceeeInstance_ = ceeeInstance;
  var windows = CEEE_window_internal_;

  // Register the extension handling functions with the CEEE instance.
  ceeeInstance.registerExtensionHandler(windows.CMD_GET_WINDOW,
                                        windows,
                                        windows.getWindow_);
  ceeeInstance.registerExtensionHandler(windows.CMD_GET_CURRENT_WINDOW,
                                        windows,
                                        windows.getCurrentWindow_);
  ceeeInstance.registerExtensionHandler(windows.CMD_GET_LAST_FOCUSED_WINDOW,
                                        windows,
                                        windows.getLastFocusedWindow_);
  ceeeInstance.registerExtensionHandler(windows.CMD_GET_ALL_WINDOWS,
                                        windows,
                                        windows.getAllWindows_);
  ceeeInstance.registerExtensionHandler(windows.CMD_CREATE_WINDOW,
                                        windows,
                                        windows.createWindow_);
  ceeeInstance.registerExtensionHandler(windows.CMD_UPDATE_WINDOW,
                                        windows,
                                        windows.updateWindow_);
  ceeeInstance.registerExtensionHandler(windows.CMD_REMOVE_WINDOW,
                                        windows,
                                        windows.removeWindow_);

  // Install the error/status reporting methods for this module from the
  // owning CEEE instance.
  windows.logInfo_ = ceeeInstance.logInfo;
  windows.logError_ = ceeeInstance.logError;

  return windows;
}
