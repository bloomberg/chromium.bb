// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * tab APIs for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */

/**
 * Place-holder namespace-object for helper functions used in this file.
 * @private
 */
var CEEE_tabs_internal_ = CEEE_tabs_internal_ || {
  /** Reference to the instance object for the CEEE. @private */
  ceeeInstance_: null
};

/**
 * Routine initializaing the tabs module during onLoad message from the
 * browser.
 * @public
 */
CEEE_tabs_internal_.onLoad = function() {
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var tabContainer = mainBrowser.tabContainer;
  var tabs = this;
  tabContainer.addEventListener('TabOpen',
                                function(evt) {tabs.onTabOpened_(evt);},
                                false);
  tabContainer.addEventListener('TabMove',
                                function(evt) {tabs.onTabMoved_(evt);},
                                false);
  tabContainer.addEventListener('TabSelect',
                                function(evt) {tabs.onTabSelected_(evt);},
                                false);
  tabContainer.addEventListener('TabClose',
                                function(evt) {tabs.onTabClosed_(evt);},
                                false);
};

/**
 * Build an object that represents a "tab", as defined by the Google Chrome
 * extension API.  For newly created tabs, the optional URL parameter needs to
 * be specified since some part of tab creation are asynchronous.  Note also
 * that the title property of the returned "tab" may be '' for newly created
 * tabs too.
 *
 * @param {!Object} mainBrowser Firefox's main tabbed browser.
 * @param {!Object} tab The Firefox tab from which to build the description.
 * @param {string} url_opt Optional string contain URL.  IF not specified, the
 *     URL is taken from the tab's associated browser.
 * @return A tab description as defined by Google Chrome extension API.
 */
CEEE_tabs_internal_.buildTabValue = function(mainBrowser, tab, url_opt) {
  var browser = mainBrowser.getBrowserForTab(tab);
  var container = mainBrowser.tabContainer;
  var index = container.getIndexOfItem(tab);

  var ret = {};
  ret.id = CEEE_mozilla_tabs.getTabId(tab);
  ret.index = index;
  ret.windowId = CEEE_mozilla_windows.getWindowId(window);
  ret.selected = (tab == mainBrowser.selectedTab);
  ret.pinned = false;
  ret.url = url_opt || browser.currentURI.asciiSpec;
  ret.title = browser.contentTitle;
  ret.incognito = CEEE_globals.privateBrowsingService.isInPrivateBrowsing;

  return ret;
};

/**
 * Called when a new tab is created in this top-level window.
 *
 * @param evt DOM event object.
 * @private
 */
CEEE_tabs_internal_.onTabOpened_ = function(evt) {
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser.tabContainer;
  var browser = evt.target && evt.target.linkedBrowser;
  if (!mainBrowser || !container || !browser)
    return;

  // Build a tab info object that corresponds to the new tab.
  var index = mainBrowser.getBrowserIndexForDocument(browser.contentDocument);
  var tab = container.getItemAtIndex(index);
  var info = [this.buildTabValue(mainBrowser, tab)];

  // Send the event notification to ChromeFrame.
  var msg = ['tabs.onCreated', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Called when a tab is moved in this top-level window.
 *
 * @param evt DOM event object.
 * @private
 */
CEEE_tabs_internal_.onTabMoved_ = function(evt) {
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser.tabContainer;
  var browser = evt.target && evt.target.linkedBrowser;
  if (!mainBrowser || !container || !browser)
    return;

  // Build info corresponding to moved tab.
  //
  // Through experimentation, I've discovered that the detail property of the
  // evt object contains the old index of the tab.  This was discovered with
  // Firefox 3.0.11.  According to the docs at
  // https://developer.mozilla.org/en/DOM/event.detail, this property
  // specifies extra details about the event, and its value is event-specific.
  var oldIndex = evt.detail;
  var index = mainBrowser.getBrowserIndexForDocument(browser.contentDocument);
  if (oldIndex == index)
    return;

  var tab = container.getItemAtIndex(index);

  var info = [CEEE_mozilla_tabs.getTabId(tab), {
    windowId: CEEE_mozilla_windows.getWindowId(window),
    fromIndex: oldIndex,
    toIndex: index
  }];

  // Send the event notification to ChromeFrame.
  var msg = ['tabs.onMoved', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Called when a tab is selected.
 *
 * @param evt DOM event object.
 * @private
 */
CEEE_tabs_internal_.onTabSelected_ = function(evt) {
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser.tabContainer;
  var browser = evt.target && evt.target.linkedBrowser;
  if (!mainBrowser || !container || !browser)
    return;

  // Build info corresponding to selected tab.
  var index = mainBrowser.getBrowserIndexForDocument(browser.contentDocument);
  var tab = container.getItemAtIndex(index);
  var info = [CEEE_mozilla_tabs.getTabId(tab),
              {windowId: CEEE_mozilla_windows.getWindowId(window)}];

  // Send the event notification to ChromeFrame.
  var msg = ['tabs.onSelectionChanged', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Called when a tab is closed in this top-level window.
 *
 * @param evt DOM event object.
 * @private
 */
CEEE_tabs_internal_.onTabClosed_ = function(evt) {
  // This function is called *before* the tab is actually removed from the
  // tab container.  Therefore, we can call methods like
  // getBrowserIndexForDocument() and still expect them to find the tab that
  // has been closed.
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser.tabContainer;
  var browser = evt.target && evt.target.linkedBrowser;
  if (!mainBrowser || !container || !browser)
    return;

  // Send the event notification to ChromeFrame.
  var index = mainBrowser.getBrowserIndexForDocument(browser.contentDocument);
  var tab = container.getItemAtIndex(index);
  var info = [CEEE_mozilla_tabs.getTabId(tab)];
  var msg = ['tabs.onRemoved', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Send a status change notification to the extension.
 *
 * @param doc The content document object for the tab.
 * @param status New status of the tab.
 * @public
 */
CEEE_tabs_internal_.onTabStatusChanged = function(doc, status) {
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser.tabContainer;
  if (!mainBrowser || !container)
    return;

  // Build a tab info object that corresponds to the new status.  If we can't
  // find the document, then this means its an embedded frame.  We don't want
  // to generate a status change in this case anyway.
  var index = mainBrowser.getBrowserIndexForDocument(doc);
  if (-1 == index)
    return;

  var tab = container.getItemAtIndex(index);
  var info = [CEEE_mozilla_tabs.getTabId(tab),
    {status: status},
    this.buildTabValue(mainBrowser, tab)
  ];

  // Send the event notification to ChromeFrame.
  var msg = ['tabs.onUpdated', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

CEEE_tabs_internal_.CMD_GET_TAB = 'tabs.get';
CEEE_tabs_internal_.getTab_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var r = CEEE_mozilla_tabs.findTab(id);
  if (!r) {
    throw(new Error(CEEE_tabs_internal_.CMD_GET_TAB +
                    ': invalid tab id=' + id));
  }

  return this.buildTabValue(r.tabBrowser, r.tab);
};

CEEE_tabs_internal_.CMD_GET_SELECTED_TAB = 'tabs.getSelected';
CEEE_tabs_internal_.getSelectedTab_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var win = CEEE_mozilla_windows.findWindow(id);
  if (!win) {
    throw(new Error(CEEE_tabs_internal_.CMD_GET_SELECTED_TAB +
                    ': invalid window id=' + id));
  }

  var mainBrowser = win.document.getElementById(
      CEEE_globals.MAIN_BROWSER_ID);
  if (!mainBrowser) {
    throw(new Error(CEEE_tabs_internal_.CMD_GET_SELECTED_TAB +
                    ': cannot find main browser win id=' + id));
  }

  var tab = mainBrowser.selectedTab;
  return this.buildTabValue(mainBrowser, tab);
};

CEEE_tabs_internal_.CMD_GET_CURRENT_TAB = 'tabs.getCurrent';
CEEE_tabs_internal_.getCurrentTab_ = function(cmd, data) {
  // TODO(rogerta@chromium.org): Revisit this implementation.  I'm not sure
  // it strictly obeys the chrome extension API spec.
  var cfTab = data.tab;
  if (cfTab) {
    var win = CEEE_mozilla_windows.findWindowFromCfSessionId(cfTab.id);
    if (win) {
      var mainBrowser = win.document.getElementById(
          CEEE_globals.MAIN_BROWSER_ID);
      var tab = mainBrowser.selectedTab;
      return this.buildTabValue(mainBrowser, tab);
    }
  }
};

CEEE_tabs_internal_.CMD_GET_ALL_TABS_IN_WINDOW = 'tabs.getAllInWindow';
CEEE_tabs_internal_.getAllTabsInWindow_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var win = CEEE_mozilla_windows.findWindow(id);
  if (!win || !win.document) {
    throw(new Error(CEEE_tabs_internal_.CMD_GET_ALL_TABS_IN_WINDOW +
                    ': invalid window id=' + id));
  }

  var mainBrowser = win.document.getElementById(
      CEEE_globals.MAIN_BROWSER_ID);
  if (!mainBrowser) {
    throw(new Error(CEEE_tabs_internal_.CMD_GET_ALL_TABS_IN_WINDOW +
                    ': cannot find main browser win id=' + id));
  }

  var ret = [];
  var tabs = mainBrowser.tabContainer;
  var count = tabs.itemCount;
  var selectedTab = mainBrowser.selectedTab;
  for (var i = 0; i < count; ++i) {
    var tab = tabs.getItemAtIndex(i);
    var t = this.buildTabValue(mainBrowser, tab);
    ret.push(t);
  }

  return ret;
};

CEEE_tabs_internal_.CMD_CREATE_TAB = 'tabs.create';
CEEE_tabs_internal_.createTab_ = function(cmd, data) {
  var args_list = CEEE_json.decode(data.args);
  var args = args_list[0];
  var win = CEEE_mozilla_windows.findWindow(args.windowId);
  if (!win) {
    throw(new Error(CEEE_tabs_internal_.CMD_CREATE_TAB +
                    ': invalid window id=' + args.windowId));
  }

  var mainBrowser = win.document.getElementById(
      CEEE_globals.MAIN_BROWSER_ID);
  if (!mainBrowser) {
    throw(new Error(CEEE_tabs_internal_.CMD_CREATE_TAB +
                    ': cannot find main browser win id=' + args.windowId));
  }

  var tab = mainBrowser.addTab(args.url);

  if (args.index)
    mainBrowser.moveTabTo(tab, args.index);

  if (!('selected' in args) || args.selected)
    mainBrowser.selectedTab = tab;

  return this.buildTabValue(mainBrowser, tab, args.url);
};

CEEE_tabs_internal_.CMD_UPDATE_TAB = 'tabs.update';
CEEE_tabs_internal_.updateTab_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var t = CEEE_mozilla_tabs.findTab(id);
  if (!t) {
    throw(new Error(CEEE_tabs_internal_.CMD_UPDATE_TAB +
                    ': invalid tab id=' + id));
  }

  var info = args[1];
  if (info && info.url)
    t.tabBrowser.getBrowserForTab(t.tab).loadURI(info.url);

  if (info && info.selected)
    t.tabBrowser.selectedTab = t.tab;

  return this.buildTabValue(t.tabBrowser, t.tab, info.url);
};

CEEE_tabs_internal_.CMD_MOVE_TAB = 'tabs.move';
CEEE_tabs_internal_.moveTab_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var t = CEEE_mozilla_tabs.findTab(id);
  if (!t) {
    throw(new Error(CEEE_tabs_internal_.CMD_MOVE_TAB +
                    ': invalid tab id=' + id));
  }

  var newIndex = args[1].index;
  var oldIndex = t.index;
  if (oldIndex != newIndex)
    t.tabBrowser.moveTabTo(t.tab, newIndex);

  return this.buildTabValue(t.tabBrowser, t.tab);
};

CEEE_tabs_internal_.CMD_REMOVE_TAB = 'tabs.remove';
CEEE_tabs_internal_.removeTab_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var r = CEEE_mozilla_tabs.findTab(id);
  if (!r) {
    throw(new Error(CEEE_tabs_internal_.CMD_REMOVE_TAB +
                    ': invalid tab id=' + id));
  }

  r.tabBrowser.removeTab(r.tab);
};

CEEE_tabs_internal_.CMD_EXECUTE_SCRIPT = 'tabs.executeScript';
CEEE_tabs_internal_.executeScript_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var scriptDef = args[1];
  var t;

  if (id) {
    t = CEEE_mozilla_tabs.findTab(id);
    if (!t) {
      throw(new Error(CEEE_tabs_internal_.CMD_EXECUTE_SCRIPT +
                      ': invalid tab id=' + id));
    }
  } else {
    var win = CEEE_mozilla_windows.findWindow(null);
    if (!win) {
      throw(new Error(CEEE_tabs_internal_.CMD_EXECUTE_SCRIPT +
                      ': no window'));
    }

    var mainBrowser = win.document.getElementById(
        CEEE_globals.MAIN_BROWSER_ID);
    if (!mainBrowser) {
      throw(new Error(CEEE_tabs_internal_.CMD_EXECUTE_SCRIPT +
                      ': cannot find main browser'));
    }

    t = {
      'tab': mainBrowser.selectedTab,
      'tabBrowser': mainBrowser
    };
  }

  var w = t.tabBrowser.getBrowserForTab(t.tab).contentWindow;
  return this.ceeeInstance_.getUserScriptsModule().executeScript(w, scriptDef);
};

CEEE_tabs_internal_.CMD_INSERT_CSS = 'tabs.insertCSS';
CEEE_tabs_internal_.insertCss_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var id = args[0];
  var details = args[1];
  var t;

  if (id) {
    t = CEEE_mozilla_tabs.findTab(id);
    if (!t) {
      throw new Error(CEEE_tabs_internal_.CMD_INSERT_CSS +
                      ': invalid tab id=' + id);
    }
  } else {
    var win = CEEE_mozilla_windows.findWindow(null);
    if (!win) {
      throw new Error(CEEE_tabs_internal_.CMD_INSERT_CSS + ': no window');
    }

    var mainBrowser = win.document.getElementById(
        CEEE_globals.MAIN_BROWSER_ID);
    if (!mainBrowser) {
      throw new Error(CEEE_tabs_internal_.CMD_INSERT_CSS +
                      ': cannot find main browser');
    }

    t = {
      tab: mainBrowser.selectedTab,
      tabBrowser: mainBrowser
    };
  }

  var w = t.tabBrowser.getBrowserForTab(t.tab).contentWindow;
  return this.ceeeInstance_.getUserScriptsModule().insertCss(w, details);
};

/**
  * Initialization routine for the CEEE tabs API module.
  * @param {!Object} ceeeInstance Reference to the global ceee instance.
  * @return {Object} Reference to the tabs module.
  * @public
  */
function CEEE_initialize_tabs(ceeeInstance) {
  CEEE_tabs_internal_.ceeeInstance_ = ceeeInstance;
  var tabs = CEEE_tabs_internal_;
  ceeeInstance.registerExtensionHandler(tabs.CMD_GET_TAB,
                                        tabs,
                                        tabs.getTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_GET_SELECTED_TAB,
                                        tabs,
                                        tabs.getSelectedTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_GET_CURRENT_TAB,
                                        tabs,
                                        tabs.getCurrentTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_GET_ALL_TABS_IN_WINDOW,
                                        tabs,
                                        tabs.getAllTabsInWindow_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_CREATE_TAB,
                                        tabs,
                                        tabs.createTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_UPDATE_TAB,
                                        tabs,
                                        tabs.updateTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_MOVE_TAB,
                                        tabs,
                                        tabs.moveTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_REMOVE_TAB,
                                        tabs,
                                        tabs.removeTab_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_EXECUTE_SCRIPT,
                                        tabs,
                                        tabs.executeScript_);
  ceeeInstance.registerExtensionHandler(tabs.CMD_INSERT_CSS,
                                        tabs,
                                        tabs.insertCss_);

  return tabs;
}
