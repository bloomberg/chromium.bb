// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dictionary of constants (initialized by browser).
 */
var LogEventType = null;
var LogEventPhase = null;
var LogSourceType = null;

/**
 * Object to communicate between the renderer and the browser.
 * @type {!BrowserBridge}
 */
var g_browser = null;

/**
 * Main entry point. called once the page has loaded.
 */
function onLoaded() {
  g_browser = new BrowserBridge();

  // Create the view which displays requests lists, and lets you select, filter
  // and delete them.
  var requestsView = new RequestsView('requestsListTableBody',
                                      'filterInput',
                                      'filterCount',
                                      'deleteSelected',
                                      'selectAll',

                                      // IDs for the details view.
                                      "detailsTabHandles",
                                      "detailsLogTab",
                                      "detailsTimelineTab",
                                      "detailsLogBox",
                                      "detailsTimelineBox",

                                      // IDs for the layout boxes.
                                      "filterBox",
                                      "requestsBox",
                                      "actionBox",
                                      "splitterBox");

  // Create a view which will display info on the proxy setup.
  var proxyView = new ProxyView("proxyTabContent",
                                "proxyCurrentConfig",
                                "proxyReloadSettings",
                                "badProxiesTableBody",
                                "clearBadProxies");

  // Create a view which will display information on the host resolver.
  var dnsView = new DnsView("dnsTabContent",
                            "hostResolverCacheTbody",
                            "clearHostResolverCache",
                            "hostResolverCacheCapacity",
                            "hostResolverCacheTTLSuccess",
                            "hostResolverCacheTTLFailure");

  // Create a view which lets you tab between the different sub-views.
  var categoryTabSwitcher =
      new TabSwitcherView(new DivView('categoryTabHandles'));

  // Populate the main tabs.
  categoryTabSwitcher.addTab('requestsTab', requestsView, false);
  categoryTabSwitcher.addTab('proxyTab', proxyView, false);
  categoryTabSwitcher.addTab('dnsTab', dnsView, false);
  categoryTabSwitcher.addTab('socketsTab', new DivView('socketsTabContent'),
                             false);
  categoryTabSwitcher.addTab('httpCacheTab',
                             new DivView('httpCacheTabContent'), false);

  // Build a map from the anchor name of each tab handle to its "tab ID".
  // We will consider navigations to the #hash as a switch tab request.
  var anchorMap = {};
  var tabIds = categoryTabSwitcher.getAllTabIds();
  for (var i = 0; i < tabIds.length; ++i) {
    var aNode = document.getElementById(tabIds[i]);
    anchorMap[aNode.hash] = tabIds[i];
  }
  // Default the empty hash to the requests tab.
  anchorMap['#'] = anchorMap[''] = 'requestsTab';

  window.onhashchange = function() {
    var tabId = anchorMap[window.location.hash];
    if (tabId)
      categoryTabSwitcher.switchToTab(tabId);
  };

  // Make this category tab widget the primary view, that fills the whole page.
  var windowView = new WindowView(categoryTabSwitcher);

  // Trigger initial layout.
  windowView.resetGeometry();

  // Select the initial view based on the current URL.
  window.onhashchange();

  // Tell the browser that we are ready to start receiving log events.
  g_browser.sendReady();
}

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 *
 * @constructor
 */
function BrowserBridge() {
  // List of observers for various bits of browser state.
  this.logObservers_ = [];
  this.proxySettingsObservers_ = [];
  this.badProxiesObservers_ = [];
  this.hostResolverCacheObservers_ = [];

  // Map from observer method name (i.e. 'onProxySettingsChanged',
  // 'onBadProxiesChanged') to the previously received data for that type. Used
  // to tell if the data has actually changed since we last polled it.
  this.prevPollData_ = {};
}

/**
 * Delay in milliseconds between polling of certain browser information.
 */
BrowserBridge.POLL_INTERVAL_MS = 5000;

//------------------------------------------------------------------------------
// Messages sent to the browser
//------------------------------------------------------------------------------

BrowserBridge.prototype.sendReady = function() {
  chrome.send('notifyReady');

  // Some of the data we are interested is not currently exposed as a stream,
  // so we will poll the browser to find out when it changes and then notify
  // the observers.
  window.setInterval(
      this.doPolling_.bind(this), BrowserBridge.POLL_INTERVAL_MS);
};

BrowserBridge.prototype.sendGetProxySettings = function() {
  // The browser will call receivedProxySettings on completion.
  chrome.send('getProxySettings');
};

BrowserBridge.prototype.sendReloadProxySettings = function() {
  chrome.send('reloadProxySettings');
};

BrowserBridge.prototype.sendGetBadProxies = function() {
  // The browser will call receivedBadProxies on completion.
  chrome.send('getBadProxies');
};

BrowserBridge.prototype.sendGetHostResolverCache = function() {
  // The browser will call receivedHostResolverCache on completion.
  chrome.send('getHostResolverCache');
};

BrowserBridge.prototype.sendClearBadProxies = function() {
  chrome.send('clearBadProxies');
};

BrowserBridge.prototype.sendClearHostResolverCache = function() {
  chrome.send('clearHostResolverCache');
};

//------------------------------------------------------------------------------
// Messages received from the browser
//------------------------------------------------------------------------------

BrowserBridge.prototype.receivedLogEntry = function(logEntry) {
  for (var i = 0; i < this.logObservers_.length; ++i)
    this.logObservers_[i].onLogEntryAdded(logEntry);
};

BrowserBridge.prototype.receivedLogEventTypeConstants = function(constantsMap) {
  LogEventType = constantsMap;
};

BrowserBridge.prototype.receivedLogEventPhaseConstants =
function(constantsMap) {
  LogEventPhase = constantsMap;
};

BrowserBridge.prototype.receivedLogSourceTypeConstants =
function(constantsMap) {
  LogSourceType = constantsMap;
};

BrowserBridge.prototype.receivedTimeTickOffset = function(timeTickOffset) {
  this.timeTickOffset_ = timeTickOffset;
};

BrowserBridge.prototype.receivedProxySettings = function(proxySettings) {
  this.dispatchToObserversFromPoll_(
      this.proxySettingsObservers_, 'onProxySettingsChanged', proxySettings);
};

BrowserBridge.prototype.receivedBadProxies = function(badProxies) {
  this.dispatchToObserversFromPoll_(
      this.badProxiesObservers_, 'onBadProxiesChanged', badProxies);
};

BrowserBridge.prototype.receivedHostResolverCache =
function(hostResolverCache) {
  this.dispatchToObserversFromPoll_(
      this.hostResolverCacheObservers_, 'onHostResolverCacheChanged',
      hostResolverCache);
};

BrowserBridge.prototype.receivedPassiveLogEntries = function(entries) {
  for (var i = 0; i < entries.length; ++i)
    this.receivedLogEntry(entries[i]);
};

//------------------------------------------------------------------------------

/**
 * Adds a listener of log entries. |observer| will be called back when new log
 * data arrives, through:
 *
 *   observer.onLogEntryAdded(logEntry)
 */
BrowserBridge.prototype.addLogObserver = function(observer) {
  this.logObservers_.push(observer);
};

/**
 * Adds a listener of the proxy settings. |observer| will be called back when
 * data is received, through:
 *
 *   observer.onProxySettingsChanged(proxySettings)
 *
 * |proxySettings| is a formatted string describing the settings.
 * TODO(eroman): send a dictionary instead.
 */
BrowserBridge.prototype.addProxySettingsObserver = function(observer) {
  this.proxySettingsObservers_.push(observer);
};

/**
 * Adds a listener of the proxy settings. |observer| will be called back when
 * data is received, through:
 *
 *   observer.onBadProxiesChanged(badProxies)
 *
 * |badProxies| is an array, where each entry has the property:
 *   badProxies[i].proxy_uri: String identify the proxy.
 *   badProxies[i].bad_until: The time when the proxy stops being considered
 *                            bad. Note the time is in time ticks.
 */
BrowserBridge.prototype.addBadProxiesObsever = function(observer) {
  this.badProxiesObservers_.push(observer);
};

/**
 * Adds a listener of the host resolver cache. |observer| will be called back
 * when data is received, through:
 *
 *   observer.onHostResolverCacheChanged(hostResolverCache)
 */
BrowserBridge.prototype.addHostResolverCacheObserver = function(observer) {
  this.hostResolverCacheObservers_.push(observer);
};

/**
 * The browser gives us times in terms of "time ticks" in milliseconds.
 * This function converts the tick count to a Date() object.
 *
 * @param {String} timeTicks.
 * @returns {Date} The time that |timeTicks| represents.
 */
BrowserBridge.prototype.convertTimeTicksToDate = function(timeTicks) {
  // Note that the subtraction by 0 is to cast to a number (probably a float
  // since the numbers are big).
  var timeStampMs = (this.timeTickOffset_ - 0) + (timeTicks - 0);
  var d = new Date();
  d.setTime(timeStampMs);
  return d;
};

BrowserBridge.prototype.doPolling_ = function() {
  // TODO(eroman): Optimize this by using a separate polling frequency for the
  // data consumed by the currently active view. Everything else can be on a low
  // frequency poll since it won't impact the display.
  this.sendGetProxySettings();
  this.sendGetBadProxies();
  this.sendGetHostResolverCache();
};

/**
 * Helper function to handle calling all the observers, but ONLY if the data has
 * actually changed since last time. This is used for data we received from
 * browser on a poll loop.
 */
BrowserBridge.prototype.dispatchToObserversFromPoll_ = function(
    observerList, method, data) {
  var prevData = this.prevPollData_[method];

  // If the data hasn't changed since last time, no need to notify observers.
  if (prevData && JSON.stringify(prevData) == JSON.stringify(data))
    return;

  this.prevPollData_[method] = data;

  // Ok, notify the observers of the change.
  for (var i = 0; i < observerList.length; ++i)
    observerList[i][method](data);
};
