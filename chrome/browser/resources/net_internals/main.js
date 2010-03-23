// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dictionary of constants (initialized by browser).
 */
var LogEntryType = null;
var LogEventType = null;
var LogEventPhase = null;
var LogSourceType = null;

/**
 * Main entry point. called once the page has loaded.
 */
function onLoaded() {
  // Layout the various DIVs in a vertically split fashion.
  new LayoutManager("filterBox",
                    "requestsBox",
                    "actionBox",
                    "splitterBox",
                    "detailsBox");

  // Create the view which displays information on the current selection.
  var detailsView = new DetailsView("detailsLogTabHandle",
                                    "detailsTimelineTabHandle",
                                    "detailsTabArea");

  // Create the view which displays requests lists, and lets you select, filter
  // and delete them.
  new RequestsView('requestsListTableBody',
                   'filterInput',
                   'filterCount',
                   'deleteSelected',
                   'selectAll',
                   detailsView);

  // Tell the browser that we are ready to start receiving log events.
  notifyApplicationReady();
}

//------------------------------------------------------------------------------
// Messages sent to the browser
//------------------------------------------------------------------------------

function notifyApplicationReady() {
  chrome.send('notifyReady');
}

//------------------------------------------------------------------------------
// Messages received from the browser
//------------------------------------------------------------------------------

function onLogEntryAdded(logEntry) {
  LogDataProvider.broadcast(logEntry);
}

function setLogEventTypeConstants(constantsMap) {
  LogEventType = constantsMap;
}

function setLogEventPhaseConstants(constantsMap) {
  LogEventPhase = constantsMap;
}

function setLogSourceTypeConstants(constantsMap) {
  LogSourceType = constantsMap;
}

function setLogEntryTypeConstants(constantsMap) {
  LogEntryType = constantsMap;
}

//------------------------------------------------------------------------------
// LogDataProvider
//------------------------------------------------------------------------------

var LogDataProvider = {}

LogDataProvider.observers_ = [];

LogDataProvider.broadcast = function(logEntry) {
  for (var i = 0; i < this.observers_.length; ++i) {
    this.observers_[i].onLogEntryAdded(logEntry);
  }
};

LogDataProvider.addObserver = function(observer) {
  this.observers_.push(observer);
};
