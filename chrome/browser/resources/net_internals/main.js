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

  // Create a view which lets you tab between the different sub-views.
  var categoryTabSwitcher =
      new TabSwitcherView(new DivView('categoryTabHandles'));

  // Populate the main tabs.
  categoryTabSwitcher.addTab('requestsTab', requestsView);
  categoryTabSwitcher.addTab('proxyTab', new DivView('proxyTabContent'));
  categoryTabSwitcher.addTab('dnsTab', new DivView('dnsTabContent'));
  categoryTabSwitcher.addTab('socketsTab', new DivView('socketsTabContent'));
  categoryTabSwitcher.addTab('httpCacheTab',
                             new DivView('httpCacheTabContent'));

  // Select the requests tab as the default.
  categoryTabSwitcher.switchToTab('requestsTab');

  // Make this category tab widget the primary view, that fills the whole page.
  var windowView = new WindowView(categoryTabSwitcher);

  // Trigger initial layout.
  windowView.resetGeometry();

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
