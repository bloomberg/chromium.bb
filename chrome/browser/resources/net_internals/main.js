// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dictionary of constants (initialized by browser, updated on load log).
 */
var LogEventType = null;
var LogEventPhase = null;
var ClientInfo = null;
var LogSourceType = null;
var LogLevelType = null;
var NetError = null;
var LoadFlag = null;
var AddressFamily = null;

// Dictionary of all constants, used for saving log files.
var Constants = null;

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

  // Create the view which displays events lists, and lets you select, filter
  // and delete them.
  var eventsView = new EventsView('eventsListTableBody',
                                  'filterInput',
                                  'filterCount',
                                  'deleteSelected',
                                  'deleteAll',
                                  'selectAll',
                                  'sortById',
                                  'sortBySource',
                                  'sortByDescription',

                                  // IDs for the details view.
                                  'detailsTabHandles',
                                  'detailsLogTab',
                                  'detailsTimelineTab',
                                  'detailsLogBox',
                                  'detailsTimelineBox',

                                  // IDs for the layout boxes.
                                  'filterBox',
                                  'eventsBox',
                                  'actionBox',
                                  'splitterBoxForEventDetails');

  // Create a view which will display info on the proxy setup.
  var proxyView = new ProxyView('proxyTabContent',
                                'proxyOriginalSettings',
                                'proxyEffectiveSettings',
                                'proxyReloadSettings',
                                'badProxiesTableBody',
                                'clearBadProxies',
                                'proxyResolverLog');

  // Create a view which will display information on the host resolver.
  var dnsView = new DnsView('dnsTabContent',
                            'hostResolverCacheTbody',
                            'clearHostResolverCache',
                            'hostResolverDefaultFamily',
                            'hostResolverIPv6Disabled',
                            'hostResolverEnableIPv6',
                            'hostResolverCacheCapacity',
                            'hostResolverCacheTTLSuccess',
                            'hostResolverCacheTTLFailure');

  // Create a view which will display save/load options to control the
  // captured data.
  var dataView = new DataView('dataTabContent', 'dataViewDownloadIframe',
                              'dataViewSaveLogFile', 'dataViewSaveStatusText',
                              'securityStrippingCheckbox',
                              'byteLoggingCheckbox', 'passivelyCapturedCount',
                              'activelyCapturedCount', 'dataViewDeleteAll',
                              'dataViewDumpDataDiv',
                              'dataViewLoadedDiv',
                              'dataViewLoadedClientInfoText',
                              'dataViewDropTarget',
                              'dataViewLoadLogFile', 'dataViewLoadStatusText',
                              'dataViewCapturingTextSpan',
                              'dataViewLoggingTextSpan');

  // Create a view which will display the results and controls for connection
  // tests.
  var testView = new TestView('testTabContent', 'testUrlInput',
                              'connectionTestsForm', 'testSummary');

  // Create a view which allows the user to query and alter the HSTS database.
  var hstsView = new HSTSView('hstsTabContent',
                              'hstsQueryInput', 'hstsQueryForm',
                              'hstsQueryOutput',
                              'hstsAddInput', 'hstsAddForm', 'hstsCheckInput',
                              'hstsAddPins',
                              'hstsDeleteInput', 'hstsDeleteForm');

  var httpCacheView = new HttpCacheView('httpCacheTabContent',
                                        'httpCacheStats');

  var socketsView = new SocketsView('socketsTabContent',
                                    'socketPoolDiv',
                                    'socketPoolGroupsDiv',
                                    'socketPoolCloseIdleButton',
                                    'socketPoolFlushButton');

  var spdyView = new SpdyView('spdyTabContent',
                              'spdyEnabledSpan',
                              'spdyUseAlternateProtocolSpan',
                              'spdyForceAlwaysSpan',
                              'spdyForceOverSslSpan',
                              'spdyNextProtocolsSpan',
                              'spdyAlternateProtocolMappingsDiv',
                              'spdySessionNoneSpan',
                              'spdySessionLinkSpan',
                              'spdySessionDiv');

  var serviceView = new ServiceProvidersView('serviceProvidersTab',
                                             'serviceProvidersTabContent',
                                             'serviceProvidersTbody',
                                             'namespaceProvidersTbody');

  var httpThrottlingView = new HttpThrottlingView(
      'httpThrottlingTabContent', 'enableHttpThrottlingCheckbox');

  var logsView = new LogsView('logsTabContent',
                              'logTable',
                              'logsGlobalShowBtn',
                              'logsGlobalHideBtn',
                              'logsRefreshBtn');

  var prerenderView = new PrerenderView('prerenderTabContent',
                                        'prerenderEnabledSpan',
                                        'prerenderHistoryDiv',
                                        'prerenderActiveDiv');

  // Create a view which lets you tab between the different sub-views.
  // This view is a left (resizable) navigation bar.
  var categoryTabSwitcher = new TabSwitcherView();
  var tabSwitcherSplitView = new ResizableVerticalSplitView(
      new DivView('categoryTabHandles'),
      categoryTabSwitcher,
      new DivView('splitterBoxForMainTabs'));

  // By default the split for the left navbar will be at 50% of the entire
  // width. This is not aesthetically pleasing, so we will shrink it.
  // TODO(eroman): Should set this dynamically based on the largest tab
  //               name rather than using a fixed width.
  tabSwitcherSplitView.setLeftSplit(150);

  g_browser.setTabSwitcher(categoryTabSwitcher);

  // Populate the main tabs.  Even tabs that don't contain information for the
  // running OS should be created, so they can load log dumps from other
  // OSes.
  categoryTabSwitcher.addTab('eventsTab', eventsView, false, true);
  categoryTabSwitcher.addTab('proxyTab', proxyView, false, true);
  categoryTabSwitcher.addTab('dnsTab', dnsView, false, true);
  categoryTabSwitcher.addTab('socketsTab', socketsView, false, true);
  categoryTabSwitcher.addTab('spdyTab', spdyView, false, true);
  categoryTabSwitcher.addTab('httpCacheTab', httpCacheView, false, true);
  categoryTabSwitcher.addTab('dataTab', dataView, false, true);
  categoryTabSwitcher.addTab('serviceProvidersTab', serviceView, false,
                             g_browser.isPlatformWindows());
  categoryTabSwitcher.addTab('testTab', testView, false, true);
  categoryTabSwitcher.addTab('hstsTab', hstsView, false, true);
  categoryTabSwitcher.addTab('httpThrottlingTab', httpThrottlingView, false,
                             true);
  categoryTabSwitcher.addTab('logsTab', logsView, false,
                             g_browser.isChromeOS());
  categoryTabSwitcher.addTab('prerenderTab', prerenderView, false, true);

  // Build a map from the anchor name of each tab handle to its "tab ID".
  // We will consider navigations to the #hash as a switch tab request.
  var anchorMap = {};
  var tabIds = categoryTabSwitcher.getAllTabIds();
  for (var i = 0; i < tabIds.length; ++i) {
    var aNode = $(tabIds[i]);
    anchorMap[aNode.hash] = tabIds[i];
  }
  // Default the empty hash to the data tab.
  anchorMap['#'] = anchorMap[''] = 'dataTab';

  window.onhashchange = onUrlHashChange.bind(null, anchorMap,
                                             categoryTabSwitcher);

  // Cut out a small vertical strip at the top of the window, to display
  // a high level status (i.e. if we are capturing events, or displaying a
  // log file). Below it we will position the main tabs and their content
  // area.
  var statusView = new DivView('statusViewId');
  var verticalSplitView = new VerticalSplitView(statusView,
                                                tabSwitcherSplitView);
  var windowView = new WindowView(verticalSplitView);

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
  this.connectionTestsObservers_ = [];
  this.hstsObservers_ = [];
  this.httpThrottlingObservers_ = [];

  // This is set to true when a log file is being viewed to block all
  // communication with the browser.
  this.isViewingLogFile_ = false;

  this.pollableDataHelpers_ = {};
  this.pollableDataHelpers_.proxySettings =
      new PollableDataHelper('onProxySettingsChanged',
                             this.sendGetProxySettings.bind(this));
  this.pollableDataHelpers_.badProxies =
      new PollableDataHelper('onBadProxiesChanged',
                             this.sendGetBadProxies.bind(this));
  this.pollableDataHelpers_.httpCacheInfo =
      new PollableDataHelper('onHttpCacheInfoChanged',
                             this.sendGetHttpCacheInfo.bind(this));
  this.pollableDataHelpers_.hostResolverInfo =
      new PollableDataHelper('onHostResolverInfoChanged',
                             this.sendGetHostResolverInfo.bind(this));
  this.pollableDataHelpers_.socketPoolInfo =
      new PollableDataHelper('onSocketPoolInfoChanged',
                             this.sendGetSocketPoolInfo.bind(this));
  this.pollableDataHelpers_.spdySessionInfo =
      new PollableDataHelper('onSpdySessionInfoChanged',
                             this.sendGetSpdySessionInfo.bind(this));
  this.pollableDataHelpers_.spdyStatus =
      new PollableDataHelper('onSpdyStatusChanged',
                             this.sendGetSpdyStatus.bind(this));
  this.pollableDataHelpers_.spdyAlternateProtocolMappings =
      new PollableDataHelper('onSpdyAlternateProtocolMappingsChanged',
                             this.sendGetSpdyAlternateProtocolMappings.bind(
                                 this));
  if (this.isPlatformWindows()) {
    this.pollableDataHelpers_.serviceProviders =
        new PollableDataHelper('onServiceProvidersChanged',
                               this.sendGetServiceProviders.bind(this));
  }
  this.pollableDataHelpers_.prerenderInfo =
      new PollableDataHelper('onPrerenderInfoChanged',
                             this.sendGetPrerenderInfo.bind(this));

  // Cache of the data received.
  this.numPassivelyCapturedEvents_ = 0;
  this.capturedEvents_ = [];

  // Next unique id to be assigned to a log entry without a source.
  // Needed to simplify deletion, identify associated GUI elements, etc.
  this.nextSourcelessEventId_ = -1;

  // True when cookies and authentication information should be removed from
  // displayed events.  When true, such information should be hidden from
  // all pages.
  this.enableSecurityStripping_ = true;
}

/*
 * Takes the current hash in form of "#tab&param1=value1&param2=value2&...".
 * Puts the parameters in an object, and passes the resulting object to
 * |categoryTabSwitcher|.  Uses tab and |anchorMap| to find a tab ID,
 * which it also passes to the tab switcher.
 *
 * Parameters and values are decoded with decodeURIComponent().
 */
function onUrlHashChange(anchorMap, categoryTabSwitcher) {
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

/**
 * Returns true if |constants| appears to be a valid constants object.
 */
BrowserBridge.prototype.areValidConstants = function(constants) {
  return typeof(constants) == 'object' &&
         typeof(constants.logEventTypes) == 'object' &&
         typeof(constants.clientInfo) == 'object' &&
         typeof(constants.logEventPhase) == 'object' &&
         typeof(constants.logSourceType) == 'object' &&
         typeof(constants.logLevelType) == 'object' &&
         typeof(constants.loadFlag) == 'object' &&
         typeof(constants.netError) == 'object' &&
         typeof(constants.addressFamily) == 'object' &&
         typeof(constants.timeTickOffset) == 'string' &&
         typeof(constants.logFormatVersion) == 'number';
};

/**
 * Attempts to load all constants from |constants|.  Returns false if one or
 * more entries are missing.  On failure, global dictionaries are not modified.
 */
BrowserBridge.prototype.loadConstants = function(constants) {
  if (!this.areValidConstants(constants))
    return false;

  LogEventType = constants.logEventTypes;
  ClientInfo = constants.clientInfo;
  LogEventPhase = constants.logEventPhase;
  LogSourceType = constants.logSourceType;
  LogLevelType = constants.logLevelType;
  LoadFlag = constants.loadFlag;
  NetError = constants.netError;
  AddressFamily = constants.addressFamily;
  this.timeTickOffset_ = constants.timeTickOffset;

  // Used for saving dumps.
  Constants = constants;

  return true;
};

/**
 * Delay in milliseconds between updates of certain browser information.
 */
BrowserBridge.POLL_INTERVAL_MS = 5000;

//------------------------------------------------------------------------------
// Messages sent to the browser
//------------------------------------------------------------------------------

/**
 * Wraps |chrome.send|.  Doesn't send anything when viewing a log file.
 */
BrowserBridge.prototype.send = function(value1, value2) {
  if (!this.isViewingLogFile_) {
    if (arguments.length == 1) {
      chrome.send(value1);
    } else if (arguments.length == 2) {
      chrome.send(value1, value2);
    } else {
      throw 'Unsupported number of arguments.';
    }
  }
};

BrowserBridge.prototype.sendReady = function() {
  this.send('notifyReady');

  // Some of the data we are interested is not currently exposed as a stream,
  // so we will poll the browser to find out when it changes and then notify
  // the observers.
  window.setInterval(this.checkForUpdatedInfo.bind(this, false),
                     BrowserBridge.POLL_INTERVAL_MS);
};

BrowserBridge.prototype.isPlatformWindows = function() {
  return /Win/.test(navigator.platform);
};

BrowserBridge.prototype.isChromeOS = function() {
  return /CrOS/.test(navigator.userAgent);
};

BrowserBridge.prototype.sendGetProxySettings = function() {
  // The browser will call receivedProxySettings on completion.
  this.send('getProxySettings');
};

BrowserBridge.prototype.sendReloadProxySettings = function() {
  this.send('reloadProxySettings');
};

BrowserBridge.prototype.sendGetBadProxies = function() {
  // The browser will call receivedBadProxies on completion.
  this.send('getBadProxies');
};

BrowserBridge.prototype.sendGetHostResolverInfo = function() {
  // The browser will call receivedHostResolverInfo on completion.
  this.send('getHostResolverInfo');
};

BrowserBridge.prototype.sendClearBadProxies = function() {
  this.send('clearBadProxies');
};

BrowserBridge.prototype.sendClearHostResolverCache = function() {
  this.send('clearHostResolverCache');
};

BrowserBridge.prototype.sendStartConnectionTests = function(url) {
  this.send('startConnectionTests', [url]);
};

BrowserBridge.prototype.sendHSTSQuery = function(domain) {
  this.send('hstsQuery', [domain]);
};

BrowserBridge.prototype.sendHSTSAdd = function(domain,
                                               include_subdomains,
                                               pins) {
  this.send('hstsAdd', [domain, include_subdomains, pins]);
};

BrowserBridge.prototype.sendHSTSDelete = function(domain) {
  this.send('hstsDelete', [domain]);
};

BrowserBridge.prototype.sendGetHttpCacheInfo = function() {
  this.send('getHttpCacheInfo');
};

BrowserBridge.prototype.sendGetSocketPoolInfo = function() {
  this.send('getSocketPoolInfo');
};

BrowserBridge.prototype.sendCloseIdleSockets = function() {
  this.send('closeIdleSockets');
};

BrowserBridge.prototype.sendFlushSocketPools = function() {
  this.send('flushSocketPools');
};

BrowserBridge.prototype.sendGetSpdySessionInfo = function() {
  this.send('getSpdySessionInfo');
};

BrowserBridge.prototype.sendGetSpdyStatus = function() {
  this.send('getSpdyStatus');
};

BrowserBridge.prototype.sendGetSpdyAlternateProtocolMappings = function() {
  this.send('getSpdyAlternateProtocolMappings');
};

BrowserBridge.prototype.sendGetServiceProviders = function() {
  this.send('getServiceProviders');
};

BrowserBridge.prototype.sendGetPrerenderInfo = function() {
  this.send('getPrerenderInfo');
};

BrowserBridge.prototype.enableIPv6 = function() {
  this.send('enableIPv6');
};

BrowserBridge.prototype.setLogLevel = function(logLevel) {
  this.send('setLogLevel', ['' + logLevel]);
};

BrowserBridge.prototype.enableHttpThrottling = function(enable) {
  this.send('enableHttpThrottling', [enable]);
};

BrowserBridge.prototype.refreshSystemLogs = function() {
  this.send('refreshSystemLogs');
};

BrowserBridge.prototype.getSystemLog = function(log_key, cellId) {
  this.send('getSystemLog', [log_key, cellId]);
};

//------------------------------------------------------------------------------
// Messages received from the browser.
//------------------------------------------------------------------------------

BrowserBridge.prototype.receive = function(command, params) {
  // Does nothing if viewing a log file.
  if (this.isViewingLogFile_)
    return;
  this[command](params);
};

BrowserBridge.prototype.receivedConstants = function(constants) {
  this.logFormatVersion_ = constants.logFormatVersion;

  this.loadConstants(constants);
};

BrowserBridge.prototype.receivedLogEntries = function(logEntries) {
  this.addLogEntries(logEntries);
};

BrowserBridge.prototype.receivedProxySettings = function(proxySettings) {
  this.pollableDataHelpers_.proxySettings.update(proxySettings);
};

BrowserBridge.prototype.receivedBadProxies = function(badProxies) {
  this.pollableDataHelpers_.badProxies.update(badProxies);
};

BrowserBridge.prototype.receivedHostResolverInfo =
function(hostResolverInfo) {
  this.pollableDataHelpers_.hostResolverInfo.update(hostResolverInfo);
};

BrowserBridge.prototype.receivedSocketPoolInfo = function(socketPoolInfo) {
  this.pollableDataHelpers_.socketPoolInfo.update(socketPoolInfo);
};

BrowserBridge.prototype.receivedSpdySessionInfo = function(spdySessionInfo) {
  this.pollableDataHelpers_.spdySessionInfo.update(spdySessionInfo);
};

BrowserBridge.prototype.receivedSpdyStatus = function(spdyStatus) {
  this.pollableDataHelpers_.spdyStatus.update(spdyStatus);
};

BrowserBridge.prototype.receivedSpdyAlternateProtocolMappings =
    function(spdyAlternateProtocolMappings) {
  this.pollableDataHelpers_.spdyAlternateProtocolMappings.update(
      spdyAlternateProtocolMappings);
};

BrowserBridge.prototype.receivedServiceProviders = function(serviceProviders) {
  this.pollableDataHelpers_.serviceProviders.update(serviceProviders);
};

BrowserBridge.prototype.receivedPassiveLogEntries = function(entries) {
  // Due to an expected race condition, it is possible to receive actively
  // captured log entries before the passively logged entries are received.
  //
  // When that happens, we create a copy of the actively logged entries, delete
  // all entries, and, after handling all the passively logged entries, add back
  // the deleted actively logged entries.
  var earlyActivelyCapturedEvents = this.capturedEvents_.slice(0);
  if (earlyActivelyCapturedEvents.length > 0)
    this.deleteAllEvents();

  this.numPassivelyCapturedEvents_ = entries.length;
  for (var i = 0; i < entries.length; ++i)
    entries[i].wasPassivelyCaptured = true;
  this.receivedLogEntries(entries);

  // Add back early actively captured events, if any.
  if (earlyActivelyCapturedEvents.length)
    this.receivedLogEntries(earlyActivelyCapturedEvents);
};


BrowserBridge.prototype.receivedStartConnectionTestSuite = function() {
  for (var i = 0; i < this.connectionTestsObservers_.length; ++i)
    this.connectionTestsObservers_[i].onStartedConnectionTestSuite();
};

BrowserBridge.prototype.receivedStartConnectionTestExperiment = function(
    experiment) {
  for (var i = 0; i < this.connectionTestsObservers_.length; ++i) {
    this.connectionTestsObservers_[i].onStartedConnectionTestExperiment(
        experiment);
  }
};

BrowserBridge.prototype.receivedCompletedConnectionTestExperiment =
function(info) {
  for (var i = 0; i < this.connectionTestsObservers_.length; ++i) {
    this.connectionTestsObservers_[i].onCompletedConnectionTestExperiment(
        info.experiment, info.result);
  }
};

BrowserBridge.prototype.receivedCompletedConnectionTestSuite = function() {
  for (var i = 0; i < this.connectionTestsObservers_.length; ++i)
    this.connectionTestsObservers_[i].onCompletedConnectionTestSuite();
};

BrowserBridge.prototype.receivedHSTSResult = function(info) {
  for (var i = 0; i < this.hstsObservers_.length; ++i)
    this.hstsObservers_[i].onHSTSQueryResult(info);
};

BrowserBridge.prototype.receivedHttpCacheInfo = function(info) {
  this.pollableDataHelpers_.httpCacheInfo.update(info);
};

BrowserBridge.prototype.receivedHttpThrottlingEnabledPrefChanged = function(
    enabled) {
  for (var i = 0; i < this.httpThrottlingObservers_.length; ++i) {
    this.httpThrottlingObservers_[i].onHttpThrottlingEnabledPrefChanged(
        enabled);
  }
};

BrowserBridge.prototype.receivedPrerenderInfo = function(prerenderInfo) {
  this.pollableDataHelpers_.prerenderInfo.update(prerenderInfo);
};

//------------------------------------------------------------------------------

BrowserBridge.prototype.categoryTabSwitcher = function() {
  return this.categoryTabSwitcher_;
};

BrowserBridge.prototype.logFormatVersion = function() {
  return this.logFormatVersion_;
};

BrowserBridge.prototype.isViewingLogFile = function() {
  return this.isViewingLogFile_;
};

/**
 * Prevents receiving/sending events to/from the browser, so loaded data will
 * not be mixed with current Chrome state.  Also hides any interactive HTML
 * elements that send messages to the browser.  Cannot be undone without
 * reloading the page.
 *
 * @param {String} fileName The name of the log file that has been loaded.
 */
BrowserBridge.prototype.onLoadLogFile = function(fileName) {
  if (!this.isViewingLogFile_) {
    this.isViewingLogFile_ = true;
    this.setSecurityStripping(false);

    // Swap out the status bar to indicate we have loaded from a file.
    setNodeDisplay($('statusViewForCapture'), false);
    setNodeDisplay($('statusViewForFile'), true);

    // Indicate which file is being displayed.
    $('statusViewDumpFileName').innerText = fileName;

    document.styleSheets[0].insertRule('.hideOnLoadLog { display: none; }');
  }
};

/**
 * Sets the |categoryTabSwitcher_| of BrowserBridge.  Since views depend on
 * g_browser being initialized, have to have a BrowserBridge prior to tab
 * construction.
 */
BrowserBridge.prototype.setTabSwitcher = function(categoryTabSwitcher) {
  this.categoryTabSwitcher_ = categoryTabSwitcher;
};

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
 * |proxySettings| is a dictionary with (up to) two properties:
 *
 *   "original"  -- The settings that chrome was configured to use
 *                  (i.e. system settings.)
 *   "effective" -- The "effective" proxy settings that chrome is using.
 *                  (decides between the manual/automatic modes of the
 *                  fetched settings).
 *
 * Each of these two configurations is formatted as a string, and may be
 * omitted if not yet initialized.
 *
 * TODO(eroman): send a dictionary instead.
 */
BrowserBridge.prototype.addProxySettingsObserver = function(observer) {
  this.pollableDataHelpers_.proxySettings.addObserver(observer);
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
BrowserBridge.prototype.addBadProxiesObserver = function(observer) {
  this.pollableDataHelpers_.badProxies.addObserver(observer);
};

/**
 * Adds a listener of the host resolver info. |observer| will be called back
 * when data is received, through:
 *
 *   observer.onHostResolverInfoChanged(hostResolverInfo)
 */
BrowserBridge.prototype.addHostResolverInfoObserver = function(observer) {
  this.pollableDataHelpers_.hostResolverInfo.addObserver(observer);
};

/**
 * Adds a listener of the socket pool. |observer| will be called back
 * when data is received, through:
 *
 *   observer.onSocketPoolInfoChanged(socketPoolInfo)
 */
BrowserBridge.prototype.addSocketPoolInfoObserver = function(observer) {
  this.pollableDataHelpers_.socketPoolInfo.addObserver(observer);
};

/**
 * Adds a listener of the SPDY info. |observer| will be called back
 * when data is received, through:
 *
 *   observer.onSpdySessionInfoChanged(spdySessionInfo)
 */
BrowserBridge.prototype.addSpdySessionInfoObserver = function(observer) {
  this.pollableDataHelpers_.spdySessionInfo.addObserver(observer);
};

/**
 * Adds a listener of the SPDY status. |observer| will be called back
 * when data is received, through:
 *
 *   observer.onSpdyStatusChanged(spdyStatus)
 */
BrowserBridge.prototype.addSpdyStatusObserver = function(observer) {
  this.pollableDataHelpers_.spdyStatus.addObserver(observer);
};

/**
 * Adds a listener of the AlternateProtocolMappings. |observer| will be called
 * back when data is received, through:
 *
 *   observer.onSpdyAlternateProtocolMappingsChanged(
 *       spdyAlternateProtocolMappings)
 */
BrowserBridge.prototype.addSpdyAlternateProtocolMappingsObserver =
    function(observer) {
  this.pollableDataHelpers_.spdyAlternateProtocolMappings.addObserver(observer);
};

/**
 * Adds a listener of the service providers info. |observer| will be called
 * back when data is received, through:
 *
 *   observer.onServiceProvidersChanged(serviceProviders)
 *
 * Will do nothing if on a platform other than Windows, as service providers are
 * only present on Windows.
 */
BrowserBridge.prototype.addServiceProvidersObserver = function(observer) {
  if (this.pollableDataHelpers_.serviceProviders)
    this.pollableDataHelpers_.serviceProviders.addObserver(observer);
};

/**
 * Adds a listener for the progress of the connection tests.
 * The observer will be called back with:
 *
 *   observer.onStartedConnectionTestSuite();
 *   observer.onStartedConnectionTestExperiment(experiment);
 *   observer.onCompletedConnectionTestExperiment(experiment, result);
 *   observer.onCompletedConnectionTestSuite();
 */
BrowserBridge.prototype.addConnectionTestsObserver = function(observer) {
  this.connectionTestsObservers_.push(observer);
};

/**
 * Adds a listener for the http cache info results.
 * The observer will be called back with:
 *
 *   observer.onHttpCacheInfoChanged(info);
 */
BrowserBridge.prototype.addHttpCacheInfoObserver = function(observer) {
  this.pollableDataHelpers_.httpCacheInfo.addObserver(observer);
};

/**
 * Adds a listener for the results of HSTS (HTTPS Strict Transport Security)
 * queries. The observer will be called back with:
 *
 *   observer.onHSTSQueryResult(result);
 */
BrowserBridge.prototype.addHSTSObserver = function(observer) {
  this.hstsObservers_.push(observer);
};

/**
 * Adds a listener for HTTP throttling-related events. |observer| will be called
 * back when HTTP throttling is enabled/disabled, through:
 *
 *   observer.onHttpThrottlingEnabledPrefChanged(enabled);
 */
BrowserBridge.prototype.addHttpThrottlingObserver = function(observer) {
  this.httpThrottlingObservers_.push(observer);
};

/**
 * Adds a listener for updated prerender info events
 * |observer| will be called back with:
 *
 *   observer.onPrerenderInfoChanged(prerenderInfo);
 */
BrowserBridge.prototype.addPrerenderInfoObserver = function(observer) {
  this.pollableDataHelpers_.prerenderInfo.addObserver(observer);
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

/**
 * Returns a list of all captured events.
 */
BrowserBridge.prototype.getAllCapturedEvents = function() {
  return this.capturedEvents_;
};

/**
 * Returns the number of events that were captured while we were
 * listening for events.
 */
BrowserBridge.prototype.getNumActivelyCapturedEvents = function() {
  return this.capturedEvents_.length - this.numPassivelyCapturedEvents_;
};

/**
 * Returns the number of events that were captured passively by the
 * browser prior to when the net-internals page was started.
 */
BrowserBridge.prototype.getNumPassivelyCapturedEvents = function() {
  return this.numPassivelyCapturedEvents_;
};

/**
 * Sends each entry to all log observers, and updates |capturedEvents_|.
 * Also assigns unique ids to log entries without a source.
 */
BrowserBridge.prototype.addLogEntries = function(logEntries) {
  for (var e = 0; e < logEntries.length; ++e) {
    var logEntry = logEntries[e];

    // Assign unique ID, if needed.
    if (logEntry.source.id == 0) {
      logEntry.source.id = this.nextSourcelessEventId_;
      --this.nextSourcelessEventId_;
    }
    this.capturedEvents_.push(logEntry);
    for (var i = 0; i < this.logObservers_.length; ++i)
      this.logObservers_[i].onLogEntryAdded(logEntry);
  }
};

/**
 * Deletes captured events with source IDs in |sourceIds|.
 */
BrowserBridge.prototype.deleteEventsBySourceId = function(sourceIds) {
  var sourceIdDict = {};
  for (var i = 0; i < sourceIds.length; i++)
    sourceIdDict[sourceIds[i]] = true;

  var newEventList = [];
  for (var i = 0; i < this.capturedEvents_.length; ++i) {
    var id = this.capturedEvents_[i].source.id;
    if (id in sourceIdDict) {
      if (this.capturedEvents_[i].wasPassivelyCaptured)
        --this.numPassivelyCapturedEvents_;
      continue;
    }
    newEventList.push(this.capturedEvents_[i]);
  }
  this.capturedEvents_ = newEventList;

  for (var i = 0; i < this.logObservers_.length; ++i)
    this.logObservers_[i].onLogEntriesDeleted(sourceIds);
};

/**
 * Deletes all captured events.
 */
BrowserBridge.prototype.deleteAllEvents = function() {
  this.capturedEvents_ = [];
  this.numPassivelyCapturedEvents_ = 0;
  for (var i = 0; i < this.logObservers_.length; ++i)
    this.logObservers_[i].onAllLogEntriesDeleted();
};

/**
 * Sets the value of |enableSecurityStripping_| and informs log observers
 * of the change.
 */
BrowserBridge.prototype.setSecurityStripping =
    function(enableSecurityStripping) {
  this.enableSecurityStripping_ = enableSecurityStripping;
  for (var i = 0; i < this.logObservers_.length; ++i) {
    if (this.logObservers_[i].onSecurityStrippingChanged)
      this.logObservers_[i].onSecurityStrippingChanged();
  }
};

/**
 * Returns whether or not cookies and authentication information should be
 * displayed for events that contain them.
 */
BrowserBridge.prototype.getSecurityStripping = function() {
  return this.enableSecurityStripping_;
};

/**
 * Returns true if a log file is currently being viewed.
 */
BrowserBridge.prototype.isViewingLogFile = function() {
  return this.isViewingLogFile_;
};

/**
 * If |force| is true, calls all startUpdate functions.  Otherwise, just
 * runs updates with active observers.
 */
BrowserBridge.prototype.checkForUpdatedInfo = function(force) {
  for (name in this.pollableDataHelpers_) {
    var helper = this.pollableDataHelpers_[name];
    if (force || helper.hasActiveObserver())
      helper.startUpdate();
  }
};

/**
 * Calls all startUpdate functions and, if |callback| is non-null,
 * calls it with the results of all updates.
 */
BrowserBridge.prototype.updateAllInfo = function(callback) {
  if (callback)
    new UpdateAllObserver(callback, this.pollableDataHelpers_);
  this.checkForUpdatedInfo(true);
};

/**
 * This is a helper class used by BrowserBridge, to keep track of:
 *   - the list of observers interested in some piece of data.
 *   - the last known value of that piece of data.
 *   - the name of the callback method to invoke on observers.
 *   - the update function.
 * @constructor
 */
function PollableDataHelper(observerMethodName, startUpdateFunction) {
  this.observerMethodName_ = observerMethodName;
  this.startUpdate = startUpdateFunction;
  this.observerInfos_ = [];
}

PollableDataHelper.prototype.getObserverMethodName = function() {
  return this.observerMethodName_;
};

PollableDataHelper.prototype.isObserver = function(object) {
  for (var i = 0; i < this.observerInfos_.length; ++i) {
    if (this.observerInfos_[i].observer == object)
      return true;
  }
  return false;
};

/**
 * This is a helper class used by PollableDataHelper, to keep track of
 * each observer and whether or not it has received any data.  The
 * latter is used to make sure that new observers get sent data on the
 * update following their creation.
 * @constructor
 */
function ObserverInfo(observer) {
  this.observer = observer;
  this.hasReceivedData = false;
}

PollableDataHelper.prototype.addObserver = function(observer) {
  this.observerInfos_.push(new ObserverInfo(observer));
};

PollableDataHelper.prototype.removeObserver = function(observer) {
  for (var i = 0; i < this.observerInfos_.length; ++i) {
    if (this.observerInfos_[i].observer === observer) {
      this.observerInfos_.splice(i, 1);
      return;
    }
  }
};

/**
 * Helper function to handle calling all the observers, but ONLY if the data has
 * actually changed since last time or the observer has yet to receive any data.
 * This is used for data we received from browser on an update loop.
 */
PollableDataHelper.prototype.update = function(data) {
  var prevData = this.currentData_;
  var changed = false;

  // If the data hasn't changed since last time, will only need to notify
  // observers that have not yet received any data.
  if (!prevData || JSON.stringify(prevData) != JSON.stringify(data)) {
    changed = true;
    this.currentData_ = data;
  }

  // Notify the observers of the change, as needed.
  for (var i = 0; i < this.observerInfos_.length; ++i) {
    var observerInfo = this.observerInfos_[i];
    if (changed || !observerInfo.hasReceivedData) {
      observerInfo.observer[this.observerMethodName_](this.currentData_);
      observerInfo.hasReceivedData = true;
    }
  }
};

/**
 * Returns true if one of the observers actively wants the data
 * (i.e. is visible).
 */
PollableDataHelper.prototype.hasActiveObserver = function() {
  for (var i = 0; i < this.observerInfos_.length; ++i) {
    if (this.observerInfos_[i].observer.isActive())
      return true;
  }
  return false;
};

/**
 * This is a helper class used by BrowserBridge to send data to
 * a callback once data from all polls has been received.
 *
 * It works by keeping track of how many polling functions have
 * yet to receive data, and recording the data as it it received.
 *
 * @constructor
 */
function UpdateAllObserver(callback, pollableDataHelpers) {
  this.callback_ = callback;
  this.observingCount_ = 0;
  this.updatedData_ = {};

  for (name in pollableDataHelpers) {
    ++this.observingCount_;
    var helper = pollableDataHelpers[name];
    helper.addObserver(this);
    this[helper.getObserverMethodName()] =
        this.onDataReceived_.bind(this, helper, name);
  }
}

UpdateAllObserver.prototype.isActive = function() {
  return true;
};

UpdateAllObserver.prototype.onDataReceived_ = function(helper, name, data) {
  helper.removeObserver(this);
  --this.observingCount_;
  this.updatedData_[name] = data;
  if (this.observingCount_ == 0)
    this.callback_(this.updatedData_);
};
