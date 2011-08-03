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

/**
 * Object to communicate between the renderer and the browser.
 * @type {!BrowserBridge}
 */
var g_browser = null;

/**
 * The browser gives us times in terms of "time ticks" in milliseconds.
 * This function converts the tick count to a Date() object.
 *
 * @param {String} timeTicks.
 * @returns {Date} The time that |timeTicks| represents.
 */
var convertTimeTicksToDate;

/**
 * Called to create a new log dump.  Must not be called once a dump has been
 * loaded.  Once a log dump has been created, |callback| is passed the dumped
 * text as a string.
 */
var createLogDumpAsync;

/**
 * Loads a log dump from the string |logFileContents|, which can be either a
 * full net-internals dump, or a NetLog dump only.  Returns a string containing
 * a log of the load.
 */
var loadLogFile;

// Start of annonymous namespace.
(function() {

var categoryTabSwitcher;

/**
 * Main entry point. called once the page has loaded.
 */
onLoaded = function() {
  g_browser = new BrowserBridge();

  // Create a view which lets you tab between the different sub-views.
  // This view is a left (resizable) navigation bar.
  categoryTabSwitcher = new TabSwitcherView();
  var tabSwitcherSplitView = new ResizableVerticalSplitView(
      new DivView('category-tab-handles'),
      categoryTabSwitcher,
      new DivView('splitter-box-for-main-tabs'));

  // By default the split for the left navbar will be at 50% of the entire
  // width. This is not aesthetically pleasing, so we will shrink it.
  // TODO(eroman): Should set this dynamically based on the largest tab
  //               name rather than using a fixed width.
  tabSwitcherSplitView.setLeftSplit(150);

  // Populate the main tabs.  Even tabs that don't contain information for the
  // running OS should be created, so they can load log dumps from other
  // OSes.
  categoryTabSwitcher.addTab('tab-handle-events', EventsView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-proxy', ProxyView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-dns', DnsView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-sockets', SocketsView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-spdy', SpdyView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-http-cache',
                             HttpCacheView.getInstance(), false, true);
  categoryTabSwitcher.addTab('tab-handle-import', ImportView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-export', ExportView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-capture', CaptureView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-service-providers',
                             ServiceProvidersView.getInstance(),
                             false, cr.isWindows);
  categoryTabSwitcher.addTab('tab-handle-tests', TestView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-hsts', HSTSView.getInstance(),
                             false, true);
  categoryTabSwitcher.addTab('tab-handle-http-throttling',
                             HttpThrottlingView.getInstance(), false, true);
  categoryTabSwitcher.addTab('tab-handle-logs', LogsView.getInstance(), false,
                             cr.isChromeOS);
  categoryTabSwitcher.addTab('tab-handle-prerender',
                             PrerenderView.getInstance(), false, true);

  // Build a map from the anchor name of each tab handle to its "tab ID".
  // We will consider navigations to the #hash as a switch tab request.
  var anchorMap = {};
  var tabIds = categoryTabSwitcher.getAllTabIds();
  for (var i = 0; i < tabIds.length; ++i) {
    var aNode = $(tabIds[i]);
    anchorMap[aNode.hash] = tabIds[i];
  }
  // Default the empty hash to the data tab.
  anchorMap['#'] = anchorMap[''] = 'tab-handle-export';

  window.onhashchange = onUrlHashChange.bind(null, anchorMap);

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

  g_browser.addConstantsObserver(new ConstantsObserver());

  // Tell the browser that we are ready to start receiving log events.
  g_browser.sendReady();
};

/*
 * Takes the current hash in form of "#tab&param1=value1&param2=value2&...".
 * Puts the parameters in an object, and passes the resulting object to
 * |categoryTabSwitcher|.  Uses tab and |anchorMap| to find a tab ID,
 * which it also passes to the tab switcher.
 *
 * Parameters and values are decoded with decodeURIComponent().
 */
function onUrlHashChange(anchorMap) {
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
 * Prevents receiving/sending events to/from the browser, so loaded data will
 * not be mixed with current Chrome state.  Also hides any interactive HTML
 * elements that send messages to the browser.  Cannot be undone without
 * reloading the page.
 *
 * @param {String} fileName The name of the log file that has been loaded.
 */
function onLoadLogFile(fileName) {
   // Swap out the status bar to indicate we have loaded from a file.
  setNodeDisplay($('statusViewForCapture'), false);
  setNodeDisplay($('statusViewForFile'), true);

  // Indicate which file is being displayed.
  $('statusViewDumpFileName').innerText = fileName;

  document.styleSheets[0].insertRule('.hideOnLoadLog { display: none; }');

  g_browser.sourceTracker.setSecurityStripping(false);
  g_browser.disable();
}

/**
 * Returns true if |constants| appears to be a valid constants object.
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

/**
 * Dictionary of all constants, used for saving log files.
 */
var constants = null;

/**
 * Offset needed to convert event times to Date objects.
 */
var timeTickOffset = 0;

function ConstantsObserver() {}

/**
 * Attempts to load all constants from |constants|.  Returns false if one or
 * more entries are missing.  On failure, global dictionaries are not modified.
 */
ConstantsObserver.prototype.onReceivedConstants = function(receivedConstants) {
  if (!areValidConstants(receivedConstants))
    return false;

  constants = receivedConstants;

  LogEventType = constants.logEventTypes;
  ClientInfo = constants.clientInfo;
  LogEventPhase = constants.logEventPhase;
  LogSourceType = constants.logSourceType;
  LogLevelType = constants.logLevelType;
  LoadFlag = constants.loadFlag;
  NetError = constants.netError;
  AddressFamily = constants.addressFamily;

  // Note that the subtraction by 0 is to cast to a number (probably a float
  // since the numbers are big).
  timeTickOffset = constants.timeTickOffset - 0;

  return true;
};

convertTimeTicksToDate = function(timeTicks) {
  var timeStampMs = timeTickOffset + (timeTicks - 0);
  return new Date(timeStampMs);
};

/**
 * Creates a new log dump.  |events| is a list of all events, |polledData| is an
 * object containing the results of each poll, |tabData| is an object containing
 * data for individual tabs, and |date| is the time the dump was created, as a
 * formatted string.
 * Returns the new log dump as an object.  |date| may be null.
 *
 * TODO(eroman): Use javadoc notation for these parameters.
 *
 * Log dumps are just JSON objects containing four values:
 *
 *   |userComments| User-provided notes describing what this dump file is about.
 *   |constants| needed to interpret the data.  This also includes some browser
 *               state information
 *   |events| from the NetLog,
 *   |polledData| from each PollableDataHelper available on the source OS,
 *   |tabData| containing any tab-specific state that's not present in
 *             |polledData|.
 *
 * |polledData| and |tabData| may be empty objects, or may be missing data for
 * tabs not present on the OS the log is from.
 */
function createLogDump(userComments, constants, events, polledData, tabData,
                       date) {
  if (g_browser.sourceTracker.getSecurityStripping())
    events = events.map(stripCookiesAndLoginInfo);

  var logDump = {
    'userComments': userComments,
    'constants': constants,
    'events': events,
    'polledData': polledData,
    'tabData': tabData
  };

  // Not technically client info, but it's used at the same point in the code.
  if (date && constants.clientInfo)
    constants.clientInfo.date = date;

  return logDump;
}

/**
 * Creates a full log dump using |polledData| and the return value of each tab's
 * saveState function and passes it to |callback|.
 */
function onUpdateAllCompleted(userComments, callback, polledData) {
  // Gather any tab-specific state information.
  var tabData = {};
  var tabIds = categoryTabSwitcher.getAllTabIds();
  for (var i = 0; i < tabIds.length; ++i) {
    var view = categoryTabSwitcher.findTabById(tabIds[i]).contentView;
    if (view.saveState)
      tabData[tabIds[i]] = view.saveState();
  }

  var logDump = createLogDump(userComments,
                              constants,
                              g_browser.sourceTracker.getAllCapturedEvents(),
                              polledData,
                              tabData,
                              (new Date()).toLocaleString());
  callback(JSON.stringify(logDump, null, ' '));
}

createLogDumpAsync = function(userComments, callback) {
  g_browser.updateAllInfo(
      onUpdateAllCompleted.bind(null, userComments, callback));
};

/**
 * Loads a full log dump.  Returns a string containing a log of the load.
 * The process goes like this:
 * 1)  Load constants.  If this fails, or the version number can't be handled,
 *     abort the load.  If this step succeeds, the load cannot be aborted.
 * 2)  Clear all events.  Any event observers are informed of the clear as
 *     normal.
 * 3)  Call onLoadLogStart(polledData, tabData) for each view with an
 *     onLoadLogStart function.  This allows tabs to clear any extra state that
 *     would affect the next step.  |polledData| contains the data polled for
 *     all helpers, but |tabData| contains only the data from that specific tab.
 * 4)  Add all events from the log file.
 * 5)  Call onLoadLogFinish(polledData, tabData) for each view with an
 *     onLoadLogFinish function.  The arguments are the same as in step 3.  If
 *     there is no onLoadLogFinish function, it throws an exception, or it
 *     returns false instead of true, the data dump is assumed to contain no
 *     valid data for the tab, so the tab is hidden.  Otherwise, the tab is
 *     shown.
 */
function loadLogDump(logDump, fileName) {
  // Perform minimal validity check, and abort if it fails.
  if (typeof(logDump) != 'object')
    return 'Load failed.  Top level JSON data is not an object.';

  // String listing text summary of load errors, if any.
  var errorString = '';

  if (!areValidConstants(logDump.constants))
    errorString += 'Invalid constants object.\n';
  if (typeof(logDump.events) != 'object')
    errorString += 'NetLog events missing.\n';
  if (typeof(logDump.constants.logFormatVersion) != 'number')
    errorString += 'Invalid version number.\n';

  if (errorString.length > 0)
    return 'Load failed:\n\n' + errorString;

  if (typeof(logDump.polledData) != 'object')
    logDump.polledData = {};
  if (typeof(logDump.tabData) != 'object')
    logDump.tabData = {};

  if (logDump.constants.logFormatVersion != constants.logFormatVersion) {
    return 'Unable to load different log version.' +
           ' Found ' + logDump.constants.logFormatVersion +
           ', Expected ' + constants.logFormatVersion;
  }
  if (!areValidConstants(logDump.constants))
    return 'Expected constants not found in log file.';

  g_browser.receivedConstants(logDump.constants);

  // Prevent communication with the browser.  Once the constants have been
  // loaded, it's safer to continue trying to load the log, even in the case of
  // bad data.
  onLoadLogFile(fileName);

  // Delete all events.  This will also update all logObservers.
  g_browser.sourceTracker.deleteAllSourceEntries();

  // Inform all the views that a log file is being loaded, and pass in
  // view-specific saved state, if any.
  var tabIds = categoryTabSwitcher.getAllTabIds();
  for (var i = 0; i < tabIds.length; ++i) {
    var view = categoryTabSwitcher.findTabById(tabIds[i]).contentView;
    view.onLoadLogStart(logDump.polledData, logDump.tabData[tabIds[i]]);
  }

  // Check for validity of each log entry, and then add the ones that pass.
  // Since the events are kept around, and we can't just hide a single view
  // on a bad event, we have more error checking for them than other data.
  var validPassiveEvents = [];
  var validActiveEvents = [];
  for (var eventIndex = 0; eventIndex < logDump.events.length; ++eventIndex) {
    var event = logDump.events[eventIndex];
    if (typeof(event) == 'object' && typeof(event.source) == 'object' &&
        typeof(event.time) == 'string' &&
        getKeyWithValue(LogEventType, event.type) != '?' &&
        getKeyWithValue(LogSourceType, event.source.type) != '?' &&
        getKeyWithValue(LogEventPhase, event.phase) != '?') {
      if (event.wasPassivelyCaptured) {
        validPassiveEvents.push(event);
      } else {
        validActiveEvents.push(event);
      }
    }
  }
  g_browser.sourceTracker.onReceivedPassiveLogEntries(validPassiveEvents);
  g_browser.sourceTracker.onReceivedLogEntries(validActiveEvents);

  var numInvalidEvents = logDump.events.length
                             - validPassiveEvents.length
                             - validActiveEvents.length;
  if (numInvalidEvents > 0) {
    errorString += 'Unable to load ' + numInvalidEvents +
                   ' events, due to invalid data.\n\n';
  }

  // Update all views with data from the file.  Show only those views which
  // successfully load the data.
  for (var i = 0; i < tabIds.length; ++i) {
    var view = categoryTabSwitcher.findTabById(tabIds[i]).contentView;
    var showView = false;
    // The try block eliminates the need for checking every single value before
    // trying to access it.
    try {
      if (view.onLoadLogFinish(logDump.polledData,
                               logDump.tabData[tabIds[i]],
                               logDump.userComments)) {
        showView = true;
      }
    } catch (error) {
    }
    categoryTabSwitcher.showTabHandleNode(tabIds[i], showView);
  }

  return errorString + 'Log loaded.';
}

loadLogFile = function(logFileContents, fileName) {
  // Try and parse the log dump as a single JSON string.  If this succeeds,
  // it's most likely a full log dump.  Otherwise, it may be a dump created by
  // --log-net-log.
  var parsedDump = null;
  try {
    parsedDump = JSON.parse(logFileContents);
  } catch (error) {
    try {
      // We may have a --log-net-log=blah log dump.  If so, remove the comma
      // after the final good entry, and add the necessary close brackets.
      var end = Math.max(logFileContents.lastIndexOf(',\n'),
                         logFileContents.lastIndexOf(',\r'));
      if (end != -1)
        parsedDump = JSON.parse(logFileContents.substring(0, end) + ']}');
    }
    catch (error) {
    }
  }

  if (!parsedDump)
    return 'Unable to parse log dump as JSON file.';
  return loadLogDump(parsedDump, fileName);
};

// End of anonymous namespace.
})();
