// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr.js

cr.define('chrome.sync', function() {
  var currSearchId = 0;

  /**
   * Runs a search with the given query.
   *
   * @param {string} query The query to do the search with.
   * @param {Array.<{id: string, title: string, isFolder: boolean}>}
   *     callback The callback called with the search results; not
   *     called if doSearch() is called again while the search is
   *     running.
   */
  var doSearch = function(query, callback) {
    var searchId = ++currSearchId;
    chrome.sync.findNodesContainingString(query, function(ids) {
      if (currSearchId != searchId) {
        return;
      }
      chrome.sync.getNodeSummariesById(ids, function(nodeSummaries) {
        if (currSearchId != searchId) {
          return;
        }
        callback(nodeSummaries);
      });
    });
  };

  /**
   * Decorates the various search controls.
   *
   * @param {!HTMLInputElement} queryControl The <input> object of
   *     type=search where the user types in his query.
   * @param {!HTMLElement} statusControl The <span> object display the
   *     search status.
   * @param {!HTMLElement} listControl The <list> object which holds
   *     the list of returned results.
   * @param {!HTMLPreElement} detailsControl The <pre> object which
   *     holds the details of the selected result.
   */
  function decorateSearchControls(queryControl, statusControl,
                                  resultsControl, detailsControl) {
    var resultsDataModel = new cr.ui.ArrayDataModel([]);

    // Decorate search box.
    queryControl.onsearch = function() {
      var query = queryControl.value;
      statusControl.textContent = '';
      resultsDataModel.splice(0, resultsDataModel.length);
      if (!query) {
        return;
      }
      statusControl.textContent = 'Searching for ' + query + '...';
      var timer = chrome.sync.makeTimer();
      doSearch(query, function(nodeSummaries) {
        statusControl.textContent =
          'Found ' + nodeSummaries.length + ' nodes in '
          + timer.elapsedSeconds + 's';
        // TODO(akalin): Write a nicer list display.
        for (var i = 0; i < nodeSummaries.length; ++i) {
          nodeSummaries[i].toString = function() {
            return this.title;
          }.bind(nodeSummaries[i]);
        }
        resultsDataModel.push.apply(resultsDataModel, nodeSummaries);
        // Workaround for http://crbug.com/83452 .
        resultsControl.redraw();
      });
    };
    queryControl.value = '';

    // Decorate results list.
    cr.ui.List.decorate(resultsControl);
    resultsControl.dataModel = resultsDataModel;
    resultsControl.selectionModel.addEventListener('change', function(event) {
      detailsControl.textContent = '';
      var selected = resultsControl.selectedItem;
      if (selected) {
        chrome.sync.getNodeDetailsById([selected.id], function(nodeDetails) {
          var selectedNodeDetails = nodeDetails[0] || null;
          if (selectedNodeDetails) {
            detailsControl.textContent =
              JSON.stringify(selectedNodeDetails, null, 2);
          }
        });
      }
    });
  }

  return {
    decorateSearchControls: decorateSearchControls
  };
});
