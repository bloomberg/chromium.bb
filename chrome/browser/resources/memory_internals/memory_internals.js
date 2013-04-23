// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_main_view = null;

/**
 * This class is the root view object of the page.
 */
var MainView = (function() {
  'use strict';

  /**
   * @constructor
   */
  function MainView() {
    $('button-update').onclick = function() {
      chrome.send('update');
    };
  };

  MainView.prototype = {
    /**
     * Receiving notification to display memory snapshot.
     * @param {Object}  Information about memory in JSON format.
     */
    onSetSnapshot: function(browser) {
      var html = '';
      html += '<table><tr>' +
              '<th>PID' +
              '<th>Name' +
              '<th>Private Memory [KB]';
      var processes = browser['processes'];
      for (var p in processes) {
        var process = processes[p];
        html += '<tr>';
        html += '<td class="pid">' + process['pid'];
        html += '<td class="type">' + process['type'] + '<br>' +
            process['titles'].join('<br>');
        html += '<td class="memory">' + process['memory_private'];
      }
      html += '</table>';

      $('snapshot_view').innerHTML = html;
      $('json').innerHTML = JSON.stringify(browser);
      $('json').style.visibility = 'visible';
    }
  };

  return MainView;
})();

/**
 * Initialize everything once we have access to chrome://memory-internals.
 */
document.addEventListener('DOMContentLoaded', function() {
  g_main_view = new MainView();
});
