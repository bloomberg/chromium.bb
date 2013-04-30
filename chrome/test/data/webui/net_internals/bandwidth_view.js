// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * The task fetches a page as a way of determining if network statistics
 * (such as the aggregate received content length) change accordingly.
 * The URL is received from the previous task.
 *
 * Checks that we see all relevant events and update the corresponding table.
 *
 * @param {int} expectedLength The length of the content being loaded.
 * @param {int} faviconLength The length a favicon that is received.
 * @extends {NetInternalsTest.Task}
 * @constructor
 */
function BandwidthTask(expectedLength, faviconLength) {
  NetInternalsTest.Task.call(this);
  this.url_ = null;
  this.expectedLength_ = expectedLength;
  this.faviconLength_ = faviconLength;
  this.sessionVerified = false;
  this.historicVerified = false;
}

BandwidthTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Switches to the bandwidth tab, loads a page in the background, and waits
   * for the arrival of network statistics.
   *
   * @param {string} url URL to be fetched.
   */
  start: function(url) {
    assertEquals('string', typeof url);
    this.url_ = url;
    g_browser.addSessionNetworkStatsObserver(this, true);
    g_browser.addHistoricNetworkStatsObserver(this, true);
    NetInternalsTest.switchToView('bandwidth');
    chrome.send('loadPage', [this.url_]);
  },

  /**
   * Returns the float value the specified cell of the bandwidth table.
   */
  getBandwidthTableCell_: function(row, col) {
    return parseFloat(NetInternalsTest.getTbodyText(
        BandwidthView.MAIN_BOX_ID, row, col));
  },

  /**
   * Confirms that the bandwidth usage table displays the expected values.
   * Does not check exact displayed values, to avoid races, but makes sure
   * values are high enough after an event of interest.
   *
   * @param {number} col The column of the table to validate, either 1 or 2.
   * @param {number} expectedReceived Expected received content length.
   * @param {number} expectedOriginal Expected original content length.
   */
  validateBandwidthTableColumn_: function(
      col, expectedReceived, expectedOriginal) {
    var row1 = this.getBandwidthTableCell_(0, col);
    var row2 = this.getBandwidthTableCell_(1, col);
    var row3 = this.getBandwidthTableCell_(2, col);
    var row4 = this.getBandwidthTableCell_(3, col);

    var expectedReceivedKB = (expectedReceived / 1024).toFixed(1);
    var expectedOriginalKB = (expectedOriginal / 1024).toFixed(1);

    expectLE(expectedOriginalKB, row1);
    expectLE(expectedReceivedKB, row2);
    expectFalse(isNaN(row3));
    expectFalse(isNaN(row4));
  },

  /**
   * A task is complete only when session and historic counters have been
   * verified to reflect the expected number of bytes received.
   */
  completeIfDone: function() {
    if (this.historicVerified && this.sessionVerified) {
      // Check number of rows in the table.
      NetInternalsTest.checkTbodyRows(BandwidthView.MAIN_BOX_ID, 4);
      this.onTaskDone();
    }
  },

  /**
   * SessionNetworkStatsObserver function.  Sanity checks the received data
   * and constructed table.

   * @param {object} networkStats State of the network session.
   */
  onSessionNetworkStatsChanged: function(networkStats) {
    if (this.isDone())
      return;
    // Wait until the received content length is at least the size of
    // our test page and favicon.
    var expectedLength = this.expectedLength_ + this.faviconLength_;
    if (networkStats.session_received_content_length >= expectedLength) {
      expectLE(expectedLength, networkStats.session_original_content_length);
      // Column 1 contains session information.
      this.validateBandwidthTableColumn_(1, expectedLength, expectedLength);
      this.sessionVerified = true;
      this.completeIfDone();
    }
  },

  /**
   * HistoricNetworkStatsObserver function.  Sanity checks the received data
   * and constructed table.

   * @param {object} networkStats State of the network session.
   */
  onHistoricNetworkStatsChanged: function(networkStats) {
    if (this.isDone())
      return;
    // Wait until the received content length is at least the size of
    // our test page and favicon.
    var expectedLength = this.expectedLength_ + this.faviconLength_;
    if (networkStats.historic_received_content_length >= expectedLength) {
      expectLE(expectedLength, networkStats.historic_original_content_length);
      // Column 2 contains historic information, and it should be the same as
      // the session information, because previously there was no history.
      this.validateBandwidthTableColumn_(2, expectedLength, expectedLength);
      this.historicVerified = true;
      this.completeIfDone();
    }
  }
};

/**
 * Loads a page and checks bandwidth statistics.
 */
TEST_F('NetInternalsTest', 'netInternalsSessionBandwidthSucceed', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask('files/title1.html'));
  // Load a page with a content length of 66 bytes and a 45-byte favicon.
  taskQueue.addTask(new BandwidthTask(66, 45));
  taskQueue.run();
});

})();  // Anonymous namespace
