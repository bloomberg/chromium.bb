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
   *    item from.
   * @param {function} doneCallback The callback called when the
   * search is finished; not called if doSearch() is called again
   * while the search is running.
   */
  var doSearch = function(query, callback) {
    var searchId = ++currSearchId;
    chrome.sync.findNodesContainingString(query, function(ids) {
      if (currSearchId != searchId) {
        return;
      }
      chrome.sync.getNodesById(ids, function(nodeInfos) {
        if (currSearchId != searchId) {
          return;
        }
        callback(nodeInfos);
      });
    });
  };

  /**
   * Decorates the various search controls.
   *
   * @param {object} queryControl The <input> object of type=search
   * where the user types in his query.
   * @param {object} statusControl The <span> object display the
   * search status.
   * @param {object} listControl The <list> object which holds the
   * list of returned results.
   * @param {object} detailsControl The <pre> object which holds the
   * details of the selected result.
   */
  function decorateSearchControls(queryControl, statusControl,
                                  resultsControl, detailsControl) {
    var resultsDataModel = new cr.ui.ArrayDataModel([]);

    // Decorate search box.
    queryControl.onsearch = function() {
      var query = queryControl.value;
      statusControl.innerText = '';
      resultsDataModel.splice(0, resultsDataModel.length);
      if (!query) {
        return;
      }
      statusControl.innerText = 'Searching for ' + query + '...';
      var timer = chrome.sync.makeTimer();
      doSearch(query, function(nodeInfos) {
        statusControl.innerText =
          'Found ' + nodeInfos.length + ' nodes in '
          + timer.elapsedSeconds + 's';
        // TODO(akalin): Write a nicer list display.
        for (var i = 0; i < nodeInfos.length; ++i) {
          nodeInfos[i].toString = function() {
            return this.title;
          }.bind(nodeInfos[i]);
        }
        resultsDataModel.push.apply(resultsDataModel, nodeInfos);
        // Workaround for http://crbug.com/83452 .
        resultsControl.redraw();
      });
    };
    queryControl.value = '';

    // Decorate results list.
    cr.ui.List.decorate(resultsControl);
    resultsControl.dataModel = resultsDataModel;
    resultsControl.selectionModel.addEventListener('change', function(event) {
      detailsControl.innerText = '';
      var selected = resultsControl.selectedItem;
      if (selected) {
        detailsControl.innerText = JSON.stringify(selected, null, 2);
      }
    });
  }

  return {
    decorateSearchControls: decorateSearchControls
  };
});
