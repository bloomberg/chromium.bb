// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Globals:
/** @const */ var RESULTS_PER_PAGE = 150;

/**
 * Amount of time between pageviews that we consider a 'break' in browsing,
 * measured in milliseconds.
 * @const
 */
var BROWSING_GAP_TIME = 15 * 60 * 1000;

window.addEventListener('load', function() {
  chrome.send('queryHistory', ['', 0, 0, 0, RESULTS_PER_PAGE]);
});

/**
 * Listens for history-item being selected or deselected (through checkbox)
 * and changes the view of the top toolbar.
 */
window.addEventListener('history-checkbox-select', function(e) {
  $('toolbar').count += e.detail.countAddition;
});

/**
 * Listens for call to cancel selection and loops through all items to set
 * checkbox to be unselected.
 */
window.addEventListener('unselect-all', function() {
  $('history-list').unselectAllItems($('toolbar').count);
  $('toolbar').count = 0;
});

/**
 * Listens for call to delete all selected items and loops through all items to
 * to determine which ones are selected and deletes these.
 */
window.addEventListener('delete-selected', function() {
  if (!loadTimeData.getBoolean('allowDeletingHistory')) {
    return;
  }

  // TODO(hsampson): add a popup to check whether the user definitely wants to
  // delete the selected items.

  var toBeRemoved =
      $('history-list').getSelectedItems($('toolbar').count);
  chrome.send('removeVisits', toBeRemoved);
});

/**
 * Listens for any keyboard presses which will close the overflow menu.
 */
window.addEventListener('keydown', function(e) {
  // Escape button on keyboard
  if (e.keyCode == 27) {
    $('history-list').closeMenu();
  }
});

/**
 * Resizing browser window will cause the overflow menu to close.
 */
window.addEventListener('resize', function() {
  $('history-list').closeMenu();
});

// Chrome Callbacks-------------------------------------------------------------

/**
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  $('history-list').addNewResults(results);
  if (info.finished)
    $('history-list').disableResultLoading();
}

/**
 * Called by the history backend when deletion was succesful.
 */
function deleteComplete() {
  $('history-list').removeDeletedHistory($('toolbar').count);
  $('toolbar').count = 0;
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
