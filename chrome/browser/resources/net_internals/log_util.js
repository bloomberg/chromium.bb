// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

logutil = (function() {
  /**
   * Creates a new log dump.  |events| is a list of all events, |polledData| is
   * an object containing the results of each poll, |tabData| is an object
   * containing data for individual tabs, and |date| is the time the dump was
   * created, as a formatted string.
   * Returns the new log dump as an object.  |date| may be null.
   *
   * TODO(eroman): Use javadoc notation for these parameters.
   *
   * Log dumps are just JSON objects containing five values:
   *
   *   |userComments| User-provided notes describing what this dump file is
   *                  about.
   *   |constants| needed to interpret the data.  This also includes some
   *               browser state information.
   *   |events| from the NetLog.
   *   |polledData| from each PollableDataHelper available on the source OS.
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
   * Creates a full log dump using |polledData| and the return value of each
   * tab's saveState function and passes it to |callback|.
   */
  function onUpdateAllCompleted(userComments, callback, polledData) {
    // Gather any tab-specific state information.
    var tabData = {};
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tabIds = categoryTabSwitcher.getAllTabIds();
    for (var i = 0; i < tabIds.length; ++i) {
      var view = categoryTabSwitcher.findTabById(tabIds[i]).contentView;
      if (view.saveState)
        tabData[tabIds[i]] = view.saveState();
    }

    var logDump = createLogDump(userComments,
                                Constants,
                                g_browser.sourceTracker.getAllCapturedEvents(),
                                polledData,
                                tabData,
                                (new Date()).toLocaleString());
    callback(JSON.stringify(logDump, null, ' '));
  }

  /**
   * Called to create a new log dump.  Must not be called once a dump has been
   * loaded.  Once a log dump has been created, |callback| is passed the dumped
   * text as a string.
   */
  function createLogDumpAsync(userComments, callback) {
    g_browser.updateAllInfo(
        onUpdateAllCompleted.bind(null, userComments, callback));
  }

  /**
   * Loads a full log dump.  Returns a string containing a log of the load.
   * The process goes like this:
   * 1)  Load constants.  If this fails, or the version number can't be handled,
   *     abort the load.  If this step succeeds, the load cannot be aborted.
   * 2)  Clear all events.  Any event observers are informed of the clear as
   *     normal.
   * 3)  Call onLoadLogStart(polledData, tabData) for each view with an
   *     onLoadLogStart function.  This allows tabs to clear any extra state
   *     that would affect the next step.  |polledData| contains the data polled
   *     for all helpers, but |tabData| contains only the data from that
   *     specific tab.
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

    if (logDump.constants.logFormatVersion != Constants.logFormatVersion) {
      return 'Unable to load different log version.' +
             ' Found ' + logDump.constants.logFormatVersion +
             ', Expected ' + Constants.logFormatVersion;
    }

    g_browser.receivedConstants(logDump.constants);

    // Prevent communication with the browser.  Once the constants have been
    // loaded, it's safer to continue trying to load the log, even in the case
    // of bad data.
    MainView.getInstance().onLoadLogFile(fileName);

    // Delete all events.  This will also update all logObservers.
    g_browser.sourceTracker.deleteAllSourceEntries();

    // Inform all the views that a log file is being loaded, and pass in
    // view-specific saved state, if any.
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
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
      // The try block eliminates the need for checking every single value
      // before trying to access it.
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

  /**
   * Loads a log dump from the string |logFileContents|, which can be either a
   * full net-internals dump, or a NetLog dump only.  Returns a string
   * containing a log of the load.
   */
  function loadLogFile(logFileContents, fileName) {
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
  }

  // Exports.
  return {
    createLogDumpAsync : createLogDumpAsync,
    loadLogFile : loadLogFile
  };
})();
