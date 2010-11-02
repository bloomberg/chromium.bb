// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * sidebars API for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 * The overlay.xul file contains the necessary DOM structure for the sidebar
 * implementation.
 *
 * This is a preliminary version, not all the functions implemented properly.
 * - sidebar minitab icon is fixed;
 * - badgeText is not used;
 * - others...
 * TODO(akoub@google.com): implement all the features.
 *
 * @supported Firefox 3.x
 */

/**
 * Place-holder namespace-object for helper functions used in this file.
 * @private
 */
var CEEE_sidebar_internal_ = CEEE_sidebar_internal_ || {
  /** Reference to the instance object for the ceee toolstrip. @private */
  ceeeInstance_: null,

  /** Commands. */
  /** @const */ CMD_SHOW: 'experimental.sidebar.show',
  /** @const */ CMD_HIDE: 'experimental.sidebar.hide',
  /** @const */ CMD_EXPAND: 'experimental.sidebar.expand',
  /** @const */ CMD_COLLAPSE: 'experimental.sidebar.collapse',
  /** @const */ CMD_NAVIGATE: 'experimental.sidebar.navigate',
  /** @const */ CMD_GET_STATE: 'experimental.sidebar.getState',
  /** @const */ CMD_SET_ICON: 'experimental.sidebar.setIcon',
  /** @const */ CMD_SET_TITLE: 'experimental.sidebar.setTitle',
  /** @const */ CMD_SET_BADGE_TEXT: 'experimental.sidebar.setBadgeText',

  /** Tab attribute that contains all the information about sidebar state.
   * @type {string}
   * @private
   */
  /** @const */ SIDEBAR_INFO: 'ceee_sidebar',
  /** @const */ TAB_ID: 'ceee_tabid',

  /** Overlay elements IDs that represent the sidebar.
   * None of the elements are visible in the 'hidden' state.
   * Only ICON_BOX_ID and enclosed ICON_ID elements are visible in the 'shown'
   * and 'active' states. SPLITTER_ID and CONTAINER_ID (with the enclosed
   * BROWSER_ID and LABEL_ID) are visible in the 'active' state only.
   * @type {string}
   * @private
   */
  /** @const */ ICON_BOX_ID: 'ceee-sidebar-icon-box',
  /** @const */ SPLITTER_ID: 'ceee-sidebar-splitter',
  /** @const */ CONTAINER_ID: 'ceee-sidebar-container',
  /** @const */ BROWSER_ID: 'ceee-sidebar-browser',
  /** @const */ ICON_ID: 'ceee-sidebar-icon',
  /** @const */ LABEL_ID: 'ceee-sidebar-label',

  /** Sidebar states.
   * No sidebar elements are visible in the 'hidden' state.
   * Icon box and the icon are visible in the 'shown' state.
   * Icon box, icon, and the sidebar content are visible in 'active' state.
   * @type {string}
   * @private
   */
  /** @const */ STATE_HIDDEN: 'hidden',
  /** @const */ STATE_SHOWN: 'shown',
  /** @const */ STATE_ACTIVE: 'active',

  /** scrollbars widths.
   * @type {number}
   * @private
   */
   scrollWidth_: 17
};

/**
 * Provides a CEEE Toolstrip extension Id.
 * @return {string} CEEE Toolstrip extension Id.
 * @private
 */
CEEE_sidebar_internal_.getExtensionId_ = function() {
  return this.ceeeInstance_.getToolstripExtensionId();
};

/**
 * Get a full URL for the given relative path.  Assumed that the user
 * path is relative to the Chrome Extension root.
 *
 * @param {!string} path Relative path of the user resource.
 * @return {string} Absolute URL for the user resource.
 * @private
 */
CEEE_sidebar_internal_.getResourceUrl_ = function(path) {
  return 'chrome-extension://' + this.getExtensionId_() + '/' + path;
};

/**
 * Creates a sidebar object that is linked to a tab and reflects its state.
 * @private
 */
CEEE_sidebar_internal_.createNewSidebar_ = function() {
  var sidebar = {
    state: this.STATE_HIDDEN,
    title: '',
    badgeText: '',
    url: 'about:blank',
    icon: ''
  };
  return sidebar;
};

/**
 * Shows the sidebar in its current state. Normally this function is called
 * on a TabSelect event, but it is also called whenever the sidebar status
 * is changed.
 * @param {nsIDOMXULElement} tab XUL tab element to show/hide the sidebar with.
 * @private
 */
CEEE_sidebar_internal_.showSidebar_ = function(tab) {
  var ceee = this.ceeeInstance_;

  var sidebar = tab[this.SIDEBAR_INFO];
  if (!sidebar) {
    ceee.logError('NO SIDEBAR INFO linked to the tab');
    return;
  }

  ceee.logInfo('sidebar.showSidebar_ state = ' + sidebar.state +
               '; tabId = ' + tab[this.TAB_ID] +
               '; url = ' + sidebar.url +
               '; title = ' + sidebar.title);

  // Show/hide the sidebar elements and possibly fill in the contents.
  var sidebarIconBox = document.getElementById(this.ICON_BOX_ID);
  var sidebarSplitter = document.getElementById(this.SPLITTER_ID);
  var sidebarContainer = document.getElementById(this.CONTAINER_ID);
  var sidebarBrowser = document.getElementById(this.BROWSER_ID);
  var sidebarIcon = document.getElementById(this.ICON_ID);
  var sidebarLabel = document.getElementById(this.LABEL_ID);
  if (this.STATE_SHOWN == sidebar.state) {
    sidebarIconBox.style.display = 'block';
    sidebarSplitter.setAttribute('collapsed', 'true');
    sidebarContainer.setAttribute('collapsed', 'true');
  } else if (this.STATE_ACTIVE == sidebar.state) {
    sidebarIconBox.style.display = 'block';
    sidebarSplitter.setAttribute('collapsed', 'false');
    sidebarContainer.setAttribute('collapsed', 'false');
    if (sidebarBrowser.src != sidebar.url) {
      sidebarBrowser.src = sidebar.url;
    }
    sidebarLabel.setAttribute('value', sidebar.title);
  } else {
    // STATE_HIDDEN:
    sidebarIconBox.style.display = 'none';
    sidebarSplitter.setAttribute('collapsed', 'true');
    sidebarContainer.setAttribute('collapsed', 'true');
  }
  this.onWindowResize_(null);
};

/**
 * Routine initializaing the sidebar module during onLoad message from the
 * browser. Performed for every browser window.
 * @public
 */
CEEE_sidebar_internal_.onLoad = function() {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.onLoad');

  // Add listeners to Tabs events.
  var mainBrowser = document.getElementById(
      CEEE_globals.MAIN_BROWSER_ID);
  var tabContainer = mainBrowser.tabContainer;
  var sidebar = this;
  tabContainer.addEventListener('TabOpen',
                                function(evt) { sidebar.onTabOpen_(evt); },
                                false);
  tabContainer.addEventListener('TabMove',
                                function(evt) { sidebar.onTabMove_(evt); },
                                false);
  tabContainer.addEventListener('TabSelect',
                                function(evt) { sidebar.onTabSelect_(evt); },
                                false);
  tabContainer.addEventListener('TabClose',
                                function(evt) { sidebar.onTabClose_(evt); },
                                false);

  // Create a sidebar object for each open tab.
  for (var i = 0; i < tabContainer.itemCount; ++i) {
    var tab = tabContainer.getItemAtIndex(i);
    tab[this.SIDEBAR_INFO] = this.createNewSidebar_();
    if (tab.selected) {
      this.showSidebar_(tab);
    }
  }

  this.scrollWidth_ = this.getScrollbarsWidth_();

  window.addEventListener(
      'resize', CEEE_globals.hitch(this.onWindowResize_, this), false);
};

/**
 * Calculates current width of a scroll bar. The function makes it with
 * direct modelling of an element in the browser's document.
 * Does anybody know a better way?
 * @return {number} Scrollbars width.
 * @private
 */
CEEE_sidebar_internal_.getScrollbarsWidth_ = function() {
  var doc = gBrowser.getBrowserForTab(gBrowser.selectedTab).contentDocument;

  if (doc && doc.body) {
    var outer = doc.createElement('div');
    outer.style.position = 'fixed';
    outer.style.left = '-1000px';
    outer.style.top = '-1000px';
    outer.style.width = '100px';
    outer.style.height = '100px';
    outer.style.overflowY = 'hidden';

    var inner = doc.createElement('div');
    inner.width = '100%';
    inner.height = '200px';
    outer.appendChild(inner);
    doc.body.appendChild(outer);

    var widthNoBar = inner.offsetWidth;
    outer.style.overflowY = 'scroll';
    var widthWithBar = inner.offsetWidth;
    doc.body.removeChild(outer);

    return widthNoBar - widthWithBar;
  } else {
    return 17;
  }
};

/**
 * Positioning sidebar icon on window resize.
 * TODO(akoub@google.com): meet RTL languages layout.
 * @param {nsIDOMEvent} evt Resize event.
 */
CEEE_sidebar_internal_.onWindowResize_ = function(evt) {
  var iconbox = document.getElementById(this.ICON_BOX_ID);
  if (iconbox.style.display == 'none') {
    return;
  }
  var right = 0;
  var bottom = 0;

  // Place the icon over status-bar if visible
  var statusBar = document.getElementById('status-bar');
  var statusBarStyles = window.getComputedStyle(statusBar, '');
  var statusBarVisible = statusBarStyles.display != 'none';
  if (statusBarVisible) {
    bottom += parseInt(statusBarStyles.height, 10);
  }

  // Place icon on the left of the sidebar if visible.
  var splitter = document.getElementById(this.SPLITTER_ID);
  var splitterVisible = splitter.getAttribute('collapsed') == 'false';
  if (splitterVisible) {
    var splitterWidth =
        parseInt(window.getComputedStyle(splitter, '').width, 10) +
        parseInt(window.getComputedStyle(splitter, '').borderLeftWidth, 10) +
        parseInt(window.getComputedStyle(splitter, '').borderRightWidth, 10);
    var browser = document.getElementById(this.CONTAINER_ID);
    var browserWidth =
        parseInt(window.getComputedStyle(browser, '').width, 10);
    right += splitterWidth + browserWidth;
  }

  // Shift more to the left/top when scrollbars are visible.
  var innerDoc =
      gBrowser.getBrowserForTab(gBrowser.selectedTab).contentDocument;
  var innerWindow = innerDoc && innerDoc.defaultView;
  if (innerDoc.body) {
    var innerStyles = innerWindow.getComputedStyle(innerDoc.body, '');
    if (innerStyles.overflow == 'scroll' ||
        innerStyles.overflowY == 'scroll' ||
        innerStyles.overflow != 'hidden' &&
        innerStyles.overflowY != 'hidden' &&
        innerDoc.body.clientHeight < innerDoc.body.scrollHeight) {
      right += this.scrollWidth_;
    }
    if (innerStyles.overflow == 'scroll' ||
        innerStyles.overflowX == 'scroll' ||
        innerStyles.overflow != 'hidden' &&
        innerStyles.overflowX != 'hidden' &&
        innerDoc.body.clientWidth < innerDoc.body.scrollWidth) {
      bottom += this.scrollWidth_;
    }
  }

  iconbox.style.right = right + 'px';
  iconbox.style.bottom = bottom + 'px';
  iconbox.style.zIndex = '25';
};

/**
 * TabOpen event handler. Creates a sidebar status object and link it
 * to a newly created tab.
 * @param {nsIDOMEvent} evt Event object.
 * @private
 */
CEEE_sidebar_internal_.onTabOpen_ = function(evt) {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.TabOpen');
  var tab = evt.target;
  tab[this.SIDEBAR_INFO] = this.createNewSidebar_();
};

/**
 * TabClose event handler. NOTHING TO DO.
 * @param {nsIDOMEvent} evt Event object.
 * @private
 */
CEEE_sidebar_internal_.onTabClose_ = function(evt) {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.TabClose');
};

/**
 * TabSelect event handler. Updates the sidebar elements state according to
 * the selected tab.
 * @param {nsIDOMEvent} evt Event object.
 * @private
 */
CEEE_sidebar_internal_.onTabSelect_ = function(evt) {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.TabSelect');
  this.showSidebar_(evt.target);
};

/**
 * TabMove event handler. NOTHING TO DO.
 * @param {nsIDOMEvent} evt Event object.
 * @private
 */
CEEE_sidebar_internal_.onTabMove_ = function(evt) {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.TabMove');
};

/**
 * Deinitialization of the module. NOTHING TO DO.
 * @public
 */
CEEE_sidebar_internal_.onUnload = function() {
  var ceee = this.ceeeInstance_;
  ceee.logInfo('sidebar.onUnload');
};

/**
 * Finds a tab associated with the given tabId.
 * Returns current tab if the tabId is undefined.
 * @param {?number} tabId Tab identifier.
 * @return {?nsIDOMXULElement} Associated tab XUL DOM element.
 */
CEEE_sidebar_internal_.findTab_ = function(tabId) {
  if ('undefined' == typeof tabId) {
    var tabbrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
    return tabbrowser.selectedTab;
  } else if ('number' == typeof tabId) {
    var tabObject = CEEE_mozilla_tabs.findTab(tabId);
    if (!tabObject) {
      this.ceeeInstance_.logError('Tab with tabId=' + tabId + ' not found');
    } else {
      return tabObject.tab;
    }
  } else {
    this.ceeeInstance_.logError('Not valid tabId value: ' + tabId);
  }
};

/**
 * Sets a new state to the sidebar.
 * @param {nsIDOMXULElement} tab Tab associated with this sidebar.
 * @param {string} newState New state to assign to the sidebar.
 */
CEEE_sidebar_internal_.setState_ = function(tab, newState) {
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  if (sidebarInfo.state != newState) {
    sidebarInfo.state = newState;
    this.onStateChanged_(tab);
    if (tab.selected) {
      this.showSidebar_(tab);
    }
  }
};

/**
 * Implementation of experimental.sidebar.show method.
 * It performs HIDDEN -> SHOWN sidebar's state transition.
 * Has no effect if the state is already SHOWN or ACTIVE.
 * @param {string} cmd Command name, 'experimental.sidebar.show' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.show_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];

  // We don't wanna change the state if it is already ACTIVE.
  if (this.STATE_HIDDEN == sidebarInfo.state) {
    this.setState_(tab, this.STATE_SHOWN);
  }

  ceee.logInfo('sidebar.show done');
};

/**
 * Implementation of experimental.sidebar.hide method.
 * It effectively hides the sidebar changing its state to HIDDEN.
 * Has no effect if the state is already HIDDEN.
 * @param {string} cmd Command name, 'experimental.sidebar.hide' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.hide_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;

  var tab = this.findTab_(tabId);
  this.setState_(tab, this.STATE_HIDDEN);

  ceee.logInfo('sidebar.hide done');
};

/**
 * Implementation of experimental.sidebar.expand method.
 * It performs SHOWN -> ACTIVE sidebar's state transition.
 * It's not permitted to expand a hidden sidebar, the 'show' function should
 * have been called before.
 * @param {string} cmd Command name, 'experimental.sidebar.expand' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.expand_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];

  // The function cannot be invoked when the sidebar in its 'hidden' state.
  if (this.STATE_HIDDEN == sidebarInfo.state) {
    ceee.logError('HIDDEN -> ACTIVE state transition not permitted.');
  } else {
    this.setState_(tab, this.STATE_ACTIVE);
  }

  ceee.logInfo('sidebar.expand done');
};

/**
 * Implementation of experimental.sidebar.collapse method.
 * It performs ACTIVE -> SHOWN sidebar's state transition.
 * Has no effect if the state is already SHOWN or HIDDEN.
 * @param {string} cmd Command name, 'experimental.sidebar.collapse' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.collapse_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];

  // No effect if the sidebar in its 'hidden' state.
  if (this.STATE_ACTIVE == sidebarInfo.state) {
    this.setState_(tab, this.STATE_SHOWN);
  }

  ceee.logInfo('sidebar.collapse done');
};

/**
 * Implementation of experimental.sidebar.navigateTo method.
 * @param {string} cmd Command name, 'experimental.sidebar.navigate' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.navigate_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;
  var url = args.url;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  sidebarInfo.url = url;

  if (this.STATE_ACTIVE == sidebarInfo.state && tab.selected) {
    this.showSidebar_(tab);
  }

  ceee.logInfo('sidebar.navigate done');
};

/**
 * Implementation of experimental.sidebar.setIcon method.
 * @param {string} cmd Command name, 'experimental.sidebar.setIcon' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.setIcon_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;
  var imagePath = args.imagePath;
  var imageData = args.imageData;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  if ('string' == typeof imagePath) {
    sidebarInfo.icon = CEEE_sidebar_getUserScriptUrl(imagePath);
  }

  if (this.STATE_HIDDEN != sidebarInfo.state && tab.selected) {
    this.showSidebar_(tab);
  }

  ceee.logInfo('sidebar.setIcon done');
};

/**
 * Implementation of experimental.sidebar.setTitle method.
 * @param {string} cmd Command name, 'experimental.sidebar.setTitle' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.setTitle_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;
  var title = args.title;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  sidebarInfo.title = title;

  if (this.STATE_HIDDEN != sidebarInfo.state && tab.selected) {
    this.showSidebar_(tab);
  }

  ceee.logInfo('sidebar.setTitle done');
};

/**
 * Implementation of experimental.sidebar.setBadgeText method.
 * @param {string} cmd Command name, 'experimental.sidebar.setBadgeText'
 *     expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.setBadgeText_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;
  var badgeText = args.badgeText;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  sidebarInfo.badgeText = badgeText;

  if (this.STATE_HIDDEN != sidebarInfo.state && tab.selected) {
    this.showSidebar_(tab);
  }

  ceee.logInfo('sidebar.setAttributes done');
};

/**
 * Implementation of experimental.sidebar.getState method.
 * @param {string} cmd Command name, 'experimental.sidebar.getState' expected.
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_sidebar_internal_.getState_ = function(cmd, data) {
  var ceee = this.ceeeInstance_;

  var args = CEEE_json.decode(data.args);
  var tabId = args.tabId;

  var tab = this.findTab_(tabId);
  var sidebarInfo = tab[this.SIDEBAR_INFO];

  ceee.logInfo('sidebar.getState(' + tabId + ') = ' + sidebarInfo.state);
  return sidebarInfo.state;
};

/**
 * Sends an event message to Chrome API when a sidebar state has changed.
 * @param {nsIDOMXULElement} tab Where the associated sidebar state has changed.
 * @private
 */
CEEE_sidebar_internal_.onStateChanged_ = function(tab) {
  // Send the event notification to ChromeFrame.
  var sidebarInfo = tab[this.SIDEBAR_INFO];
  var info = [{
    tabId: tab[this.TAB_ID],
    state: sidebarInfo.state
  }];
  var msg = ['experimental.sidebar.onStateChanged', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};

/**
 * Initialization routine for the CEEE sidebar API module.
 * @param {!Object} ceeeInstance Reference to the global CEEE toolstrip object.
 * @return {Object} Reference to the sidebar module.
 * @public
 */
function CEEE_initialize_sidebar(ceeeInstance) {
  // NOTE: this function must be called after the CF instance used for the
  // toolbar is created, in order to make sure that the chrome process started
  // uses the correct command line arguments.

  // Create ChromeFrame instances to host the sidebar content.
  var cf = document.createElementNS('http://www.w3.org/1999/xhtml', 'embed');
  cf.id = 'ceee-sidebar-browser';
  cf.setAttribute('flex', '1');
  cf.setAttribute('type', 'application/chromeframe');
  document.getElementById('ceee-sidebar-container').appendChild(cf);

  // Create ChromeFrame instances to host the sidebar icon.
  cf = document.createElementNS('http://www.w3.org/1999/xhtml', 'embed');
  cf.id = 'ceee-sidebar-icon';
  cf.setAttribute('width', '16');
  cf.setAttribute('height', '16');
  cf.setAttribute('type', 'application/chromeframe');
  cf.setAttribute('src', 'http://www.google.com/favicon.ico');
  document.getElementById('ceee-sidebar-icon-box').appendChild(cf);

  var sidebar = CEEE_sidebar_internal_;
  sidebar.ceeeInstance_ = ceeeInstance;
  ceeeInstance.registerExtensionHandler(sidebar.CMD_SHOW,
                                        sidebar,
                                        sidebar.show_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_HIDE,
                                        sidebar,
                                        sidebar.hide_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_EXPAND,
                                        sidebar,
                                        sidebar.expand_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_COLLAPSE,
                                        sidebar,
                                        sidebar.collapse_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_NAVIGATE,
                                        sidebar,
                                        sidebar.navigate_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_SET_ICON,
                                        sidebar,
                                        sidebar.setIcon_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_SET_TITLE,
                                        sidebar,
                                        sidebar.setTitle_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_SET_BADGE_TEXT,
                                        sidebar,
                                        sidebar.setBadgeText_);
  ceeeInstance.registerExtensionHandler(sidebar.CMD_GET_STATE,
                                        sidebar,
                                        sidebar.getState_);

  ceeeInstance.logInfo('CEEE_initialize_sidebar done');
  return sidebar;
}
