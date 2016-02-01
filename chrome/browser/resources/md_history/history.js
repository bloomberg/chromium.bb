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
 * Our history system calls this function with results from searches.
 * @param {HistoryQuery} info An object containing information about the query.
 * @param {Array<HistoryEntry>} results A list of results.
 */
function historyResult(info, results) {
  $('history-card-manager').addNewResults(results);
}

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
  $('history-card-manager').unselectAllItems($('toolbar').count);
  $('toolbar').count = 0;
});

/**
 * Listens for any keyboard presses which will close the overflow menu.
 */
window.addEventListener('keydown', function(e) {
  // Escape button on keyboard
  if (e.keyCode == 27) {
    $('history-card-manager').closeMenu();
  }
});

/**
 * Resizing browser window will cause the overflow menu to close.
 */
window.addEventListener('resize', function() {
  $('history-card-manager').closeMenu();
});
