// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send the history query immediately. This allows the query to process during
// the initial page startup.
chrome.send('queryHistory', ['', 0, 0, 0, RESULTS_PER_PAGE]);
chrome.send('getForeignSessions');

/**
 * @param {HTMLElement} element
 * @return {!Promise} Resolves once a Polymer element has been fully upgraded.
 */
function waitForUpgrade(element) {
  return new Promise(function(resolve, reject) {
    if (window.Polymer && Polymer.isInstance && Polymer.isInstance(element))
      resolve();
    else
      $('bundle').addEventListener('load', resolve);
  });
}

/**
 * Listens for history-item being selected or deselected (through checkbox)
 * and changes the view of the top toolbar.
 * @param {{detail: {countAddition: number}}} e
 */
window.addEventListener('history-checkbox-select', function(e) {
  var toolbar = /** @type {HistoryToolbarElement} */($('toolbar'));
  toolbar.count += e.detail.countAddition;
});

/**
 * Listens for call to cancel selection and loops through all items to set
 * checkbox to be unselected.
 */
window.addEventListener('unselect-all', function() {
  var historyList = /** @type {HistoryListElement} */($('history-list'));
  var toolbar = /** @type {HistoryToolbarElement} */($('toolbar'));
  historyList.unselectAllItems(toolbar.count);
  toolbar.count = 0;
});

/**
 * Listens for call to delete all selected items and loops through all items to
 * to determine which ones are selected and deletes these.
 */
window.addEventListener('delete-selected', function() {
  if (!loadTimeData.getBoolean('allowDeletingHistory'))
    return;

  // TODO(hsampson): add a popup to check whether the user definitely wants to
  // delete the selected items.

  var historyList = /** @type {HistoryListElement} */($('history-list'));
  var toolbar = /** @type {HistoryToolbarElement} */($('toolbar'));
  var toBeRemoved = historyList.getSelectedItems(toolbar.count);
  chrome.send('removeVisits', toBeRemoved);
});

/**
 * When the search is changed refresh the results from the backend. Ensures that
 * the search bar is updated with the new search term.
 * @param {{detail: {search: string}}} e
 */
window.addEventListener('search-changed', function(e) {
  $('toolbar').setSearchTerm(e.detail.search);
  /** @type {HistoryListElement} */($('history-list')).setLoading();
  /** @type {HistoryToolbarElement} */($('toolbar')).searching = true;
  chrome.send('queryHistory', [e.detail.search, 0, 0, 0, RESULTS_PER_PAGE]);
});

/**
 * Switches between displaying history data and synced tabs data for the page.
 */
window.addEventListener('switch-display', function(e) {
  $('history-synced-device-manager').hidden =
      e.detail.display != 'synced-tabs-button';
  $('history-list').hidden = e.detail.display != 'history-button';
});

// Chrome Callbacks-------------------------------------------------------------

/**
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {!Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  var listElem = $('history-list');
  waitForUpgrade(listElem).then(function() {
    var list = /** @type {HistoryListElement} */(listElem);
    list.addNewResults(results, info.term);
    /** @type {HistoryToolbarElement} */$('toolbar').searching = false;
    if (info.finished)
      list.disableResultLoading();
    // TODO(tsergeant): Showing everything as soon as the list is ready is not
    // ideal, as the sidebar can still pop in after. Fix this to show everything
    // at once.
    document.body.classList.remove('loading');
  });
}

/**
 * Called by the history backend after receiving results and after discovering
 * the existence of other forms of browsing history.
 * @param {boolean} hasSyncedResults Whether there are synced results.
 * @param {boolean} includeOtherFormsOfBrowsingHistory Whether to include
 *     a sentence about the existence of other forms of browsing history.
 */
function showNotification(
    hasSyncedResults, includeOtherFormsOfBrowsingHistory) {
  // TODO(msramek): Implement the joint notification about web history and other
  // forms of browsing history for the MD history page.
}

/**
 * Receives the synced history data. An empty list means that either there are
 * no foreign sessions, or tab sync is disabled for this profile.
 * |isTabSyncEnabled| makes it possible to distinguish between the cases.
 *
 * @param {!Array<!ForeignSession>} sessionList Array of objects describing the
 *     sessions from other devices.
 * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
 */
function setForeignSessions(sessionList, isTabSyncEnabled) {
  // TODO(calamity): Add a 'no synced devices' message when sessions are empty.
  $('history-side-bar').hidden = !isTabSyncEnabled;
  var syncedDeviceElem = $('history-synced-device-manager');
  waitForUpgrade(syncedDeviceElem).then(function() {
    var syncedDeviceManager =
        /** @type {HistorySyncedDeviceManagerElement} */(syncedDeviceElem);
    if (isTabSyncEnabled) {
      syncedDeviceManager.setSyncedHistory(sessionList);
      /** @type {HistoryToolbarElement} */($('toolbar')).hasSidebar = true;
    }
  });
}

/**
 * Called by the history backend when deletion was succesful.
 */
function deleteComplete() {
  var historyList = /** @type {HistoryListElement} */($('history-list'));
  var toolbar = /** @type {HistoryToolbarElement} */($('toolbar'));
  historyList.removeDeletedHistory(toolbar.count);
  toolbar.count = 0;
}

/**
 * Called by the history backend when the deletion failed.
 */
function deleteFailed() {
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
}
