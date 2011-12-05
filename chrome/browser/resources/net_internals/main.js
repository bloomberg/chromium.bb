// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dictionary of constants (Initialized soon after loading by data from browser,
 * updated on load log).
 */
var LogEventType = null;
var LogEventPhase = null;
var ClientInfo = null;
var LogSourceType = null;
var LogLevelType = null;
var NetError = null;
var LoadFlag = null;
var AddressFamily = null;

/**
 * Dictionary of all constants, used for saving log files.
 */
var Constants = null;

/**
 * Object to communicate between the renderer and the browser.
 * @type {!BrowserBridge}
 */
var g_browser = null;

/**
 * This class is the root view object of the page.  It owns all the other
 * views, and manages switching between them.  It is also responsible for
 * initializing the views and the BrowserBridge.
 */
var MainView = (function() {
  'use strict';

  // We inherit from ResizableVerticalSplitView.
  var superClass = ResizableVerticalSplitView;

  /**
   * Main entry point. Called once the page has loaded.
   *  @constructor
   */
  function MainView() {
    assertFirstConstructorCall(MainView);

    // This must be initialized before the tabs, so they can register as
    // observers.
    g_browser = BrowserBridge.getInstance();

    // This must be the first constants observer, so other constants observers
    // can safely use the globals, rather than depending on walking through
    // the constants themselves.
    g_browser.addConstantsObserver(new ConstantsObserver());

    // This view is a left (resizable) navigation bar.
    this.categoryTabSwitcher_ = new TabSwitcherView();
    var tabs = this.categoryTabSwitcher_;

    // Call superclass's constructor, initializing the view which lets you tab
    // between the different sub-views.
    superClass.call(this,
                    new DivView(MainView.CATEGORY_TAB_HANDLES_ID),
                    tabs,
                    new DivView(MainView.SPLITTER_BOX_FOR_MAIN_TABS_ID));

    // By default the split for the left navbar will be at 50% of the entire
    // width. This is not aesthetically pleasing, so we will shrink it.
    // TODO(eroman): Should set this dynamically based on the largest tab
    //               name rather than using a fixed width.
    this.setLeftSplit(150);

    // Populate the main tabs.  Even tabs that don't contain information for the
    // running OS should be created, so they can load log dumps from other
    // OSes.
    tabs.addTab(CaptureView.TAB_HANDLE_ID, CaptureView.getInstance(),
                false, true);
    tabs.addTab(ExportView.TAB_HANDLE_ID, ExportView.getInstance(),
                false, true);
    tabs.addTab(ImportView.TAB_HANDLE_ID, ImportView.getInstance(),
                false, true);
    tabs.addTab(ProxyView.TAB_HANDLE_ID, ProxyView.getInstance(),
                false, true);
    tabs.addTab(EventsView.TAB_HANDLE_ID, EventsView.getInstance(),
                false, true);
    tabs.addTab(TimelineView.TAB_HANDLE_ID, TimelineView.getInstance(),
                false, true);
    tabs.addTab(DnsView.TAB_HANDLE_ID, DnsView.getInstance(),
                false, true);
    tabs.addTab(SocketsView.TAB_HANDLE_ID, SocketsView.getInstance(),
                false, true);
    tabs.addTab(SpdyView.TAB_HANDLE_ID, SpdyView.getInstance(), false, true);
    tabs.addTab(HttpCacheView.TAB_HANDLE_ID, HttpCacheView.getInstance(),
                false, true);
    tabs.addTab(HttpThrottlingView.TAB_HANDLE_ID,
                HttpThrottlingView.getInstance(), false, true);
    tabs.addTab(ServiceProvidersView.TAB_HANDLE_ID,
                ServiceProvidersView.getInstance(), false, cr.isWindows);
    tabs.addTab(TestView.TAB_HANDLE_ID, TestView.getInstance(), false, true);
    tabs.addTab(HSTSView.TAB_HANDLE_ID, HSTSView.getInstance(), false, true);
    tabs.addTab(LogsView.TAB_HANDLE_ID, LogsView.getInstance(),
                false, cr.isChromeOS);
    tabs.addTab(PrerenderView.TAB_HANDLE_ID, PrerenderView.getInstance(),
                false, true);
    tabs.addTab(CrosView.TAB_HANDLE_ID, CrosView.getInstance(),
                false, cr.isChromeOS);

    // Build a map from the anchor name of each tab handle to its "tab ID".
    // We will consider navigations to the #hash as a switch tab request.
    var anchorMap = {};
    var tabIds = tabs.getAllTabIds();
    for (var i = 0; i < tabIds.length; ++i) {
      var aNode = $(tabIds[i]);
      anchorMap[aNode.hash] = tabIds[i];
    }
    // Default the empty hash to the data tab.
    anchorMap['#'] = anchorMap[''] = ExportView.TAB_HANDLE_ID;

    window.onhashchange = onUrlHashChange.bind(null, tabs, anchorMap);

    // Cut out a small vertical strip at the top of the window, to display
    // a high level status (i.e. if we are capturing events, or displaying a
    // log file). Below it we will position the main tabs and their content
    // area.
    var statusView = new DivView(MainView.STATUS_VIEW_ID);
    var verticalSplitView = new VerticalSplitView(statusView, this);
    var windowView = new WindowView(verticalSplitView);

    // Trigger initial layout.
    windowView.resetGeometry();

    // Select the initial view based on the current URL.
    window.onhashchange();

    // Tell the browser that we are ready to start receiving log events.
    g_browser.sendReady();
  }

  // IDs for special HTML elements in index.html
  MainView.CATEGORY_TAB_HANDLES_ID = 'category-tab-handles';
  MainView.SPLITTER_BOX_FOR_MAIN_TABS_ID = 'splitter-box-for-main-tabs';
  MainView.STATUS_VIEW_ID = 'status-view';
  MainView.STATUS_VIEW_FOR_CAPTURE_ID = 'status-view-for-capture';
  MainView.STATUS_VIEW_FOR_FILE_ID = 'status-view-for-file';
  MainView.STATUS_VIEW_DUMP_FILE_NAME_ID = 'status-view-dump-file-name';

  cr.addSingletonGetter(MainView);

  // Tracks if we're viewing a loaded log file, so views can behave
  // appropriately.  Global so safe to call during construction.
  var isViewingLoadedLog = false;

  MainView.isViewingLoadedLog = function() {
    return isViewingLoadedLog;
  };

  MainView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    // This is exposed both so the log import/export code can enumerate all the
    // tabs, and for testing.
    categoryTabSwitcher: function() {
      return this.categoryTabSwitcher_;
    },

    /**
     * Prevents receiving/sending events to/from the browser, so loaded data
     * will not be mixed with current Chrome state.  Also hides any interactive
     * HTML elements that send messages to the browser.  Cannot be undone
     * without reloading the page.  Must be called before passing loaded data
     * to the individual views.
     *
     * @param {String} fileName The name of the log file that has been loaded.
     */
    onLoadLogFile: function(fileName) {
      isViewingLoadedLog = true;

      // Swap out the status bar to indicate we have loaded from a file.
      setNodeDisplay($(MainView.STATUS_VIEW_FOR_CAPTURE_ID), false);
      setNodeDisplay($(MainView.STATUS_VIEW_FOR_FILE_ID), true);

      // Indicate which file is being displayed.
      $(MainView.STATUS_VIEW_DUMP_FILE_NAME_ID).innerText = fileName;

      document.styleSheets[0].insertRule('.hideOnLoadLog { display: none; }');

      g_browser.sourceTracker.setSecurityStripping(false);
      g_browser.disable();
    },
  };

  /*
   * Takes the current hash in form of "#tab&param1=value1&param2=value2&...".
   * Puts the parameters in an object, and passes the resulting object to
   * |categoryTabSwitcher|.  Uses tab and |anchorMap| to find a tab ID,
   * which it also passes to the tab switcher.
   *
   * Parameters and values are decoded with decodeURIComponent().
   */
  function onUrlHashChange(categoryTabSwitcher, anchorMap) {
    var parameters = window.location.hash.split('&');

    var tabId = anchorMap[parameters[0]];
    if (!tabId)
      return;

    // Split each string except the first around the '='.
    var paramDict = null;
    for (var i = 1; i < parameters.length; i++) {
      var paramStrings = parameters[i].split('=');
      if (paramStrings.length != 2)
        continue;
      if (paramDict == null)
        paramDict = {};
      var key = decodeURIComponent(paramStrings[0]);
      var value = decodeURIComponent(paramStrings[1]);
      paramDict[key] = value;
    }

    categoryTabSwitcher.switchToTab(tabId, paramDict);
  }

  return MainView;
})();

function ConstantsObserver() {}

/**
 * Attempts to load all constants from |constants|.  Returns false if one or
 * more entries are missing.  On failure, global dictionaries are not
 * modified.
 */
ConstantsObserver.prototype.onReceivedConstants =
    function(receivedConstants) {
  if (!areValidConstants(receivedConstants))
    return false;

  Constants = receivedConstants;

  LogEventType = Constants.logEventTypes;
  ClientInfo = Constants.clientInfo;
  LogEventPhase = Constants.logEventPhase;
  LogSourceType = Constants.logSourceType;
  LogLevelType = Constants.logLevelType;
  LoadFlag = Constants.loadFlag;
  NetError = Constants.netError;
  AddressFamily = Constants.addressFamily;

  timeutil.setTimeTickOffset(Constants.timeTickOffset);
};

/**
 * Returns true if it's given a valid-looking constants object.
 */
function areValidConstants(receivedConstants) {
  return typeof(receivedConstants) == 'object' &&
         typeof(receivedConstants.logEventTypes) == 'object' &&
         typeof(receivedConstants.clientInfo) == 'object' &&
         typeof(receivedConstants.logEventPhase) == 'object' &&
         typeof(receivedConstants.logSourceType) == 'object' &&
         typeof(receivedConstants.logLevelType) == 'object' &&
         typeof(receivedConstants.loadFlag) == 'object' &&
         typeof(receivedConstants.netError) == 'object' &&
         typeof(receivedConstants.addressFamily) == 'object' &&
         typeof(receivedConstants.timeTickOffset) == 'string' &&
         typeof(receivedConstants.logFormatVersion) == 'number';
}
