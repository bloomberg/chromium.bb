// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains tests for creating and loading log files.
 * It also tests the stop capturing button, since it both creates and then loads
 * a log dump.
 */

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * A Task that create a log dump and then loads it.
 * @param {string} userComments User comments to copy to the ExportsView before
 *     creating the log dump.
 * @extends {NetInternalsTest.Task}
 */
function CreateAndLoadLogTask(userComments) {
  this.userComments_ = userComments;
  NetInternalsTest.Task.call(this);
  // Not strictly necessary, but since we complete from a call to the observer,
  // seems reasonable to complete asynchronously.
  this.setCompleteAsync(true);
}

CreateAndLoadLogTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Start creating the log dump.  Use ExportView's function as it supports
   * both creating completely new logs and using existing logs to create new
   * ones, depending on whether or not we're currently viewing a log.
   */
  start: function() {
    this.initialPrivacyStripping_ =
        SourceTracker.getInstance().getPrivacyStripping();
    $(ExportView.USER_COMMENTS_TEXT_AREA_ID).value = this.userComments_;
    ExportView.getInstance().createLogDump_(this.onLogDumpCreated.bind(this),
                                            true);
  },

  /**
   * Callback passed to |createLogDumpAsync|.  Tries to load the dumped log
   * text.  Checks the status bar and user comments after loading the log.
   * @param {string} logDumpText Log dump, as a string.
   */
  onLogDumpCreated: function(logDumpText) {
    expectEquals(this.initialPrivacyStripping_,
                 SourceTracker.getInstance().getPrivacyStripping());
    expectEquals('Log loaded.', log_util.loadLogFile(logDumpText, 'log.txt'));
    expectFalse(SourceTracker.getInstance().getPrivacyStripping());

    NetInternalsTest.expectStatusViewNodeVisible(LoadedStatusView.MAIN_BOX_ID);

    expectEquals(this.userComments_,
                 $(ExportView.USER_COMMENTS_TEXT_AREA_ID).value);
    // Make sure the DIV on the import tab containing the comments is visible
    // before checking the displayed text.
    expectTrue(NetInternalsTest.nodeIsVisible($(ImportView.LOADED_DIV_ID)));
    expectEquals(this.userComments_,
                 $(ImportView.LOADED_INFO_USER_COMMENTS_ID).innerText);

    this.onTaskDone();
  }
};

/**
 * A Task that waits we receive, then asynchronously completes.
 * @extends {NetInternalsTest.Task}
 */
function WaitForConstantsTask() {
  NetInternalsTest.Task.call(this);
  this.setCompleteAsync(true);
}

WaitForConstantsTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Starts watching for constants.
   */
  start: function() {
    g_browser.addConstantsObserver(this);
  },

  /**
   * Resumes the test once we receive constants.
   */
  onReceivedConstants: function() {
    if (!this.isDone())
      this.onTaskDone();
  }
};

/**
 * Checks the visibility of each view after loading a freshly created log dump.
 * Also checks that the BrowserBridge is disabled.
 */
function checkViewsAfterLogLoaded() {
  expectTrue(g_browser.isDisabled());
  var tabVisibilityState = {
    capture: false,
    export: true,
    import: true,
    proxy: true,
    events: true,
    timeline: true,
    dns: true,
    sockets: true,
    spdy: true,
    quic: true,
    httpPipeline: false,
    httpCache: true,
    serviceProviders: cr.isWindows,
    tests: false,
    hsts: false,
    logs: false,
    prerender: true,
    bandwidth: true,
    chromeos: false
  };
  NetInternalsTest.checkTabHandleVisibility(tabVisibilityState, false);
}

function checkPrivacyStripping(expectedValue) {
  expectEquals(expectedValue,
               SourceTracker.getInstance().getPrivacyStripping());
}

/**
 * Checks the currently active view.
 * @param {string} id ID of the view that should be active.
 */
function checkActiveView(id) {
  expectEquals(id, NetInternalsTest.getActiveTabId());
}

/**
 * Exports a log dump to a string and loads it.  Makes sure no errors occur,
 * and checks visibility of tabs aftwards.  Does not actually save the log to a
 * file.
 * TODO(mmenke):  Add some checks for the import view.
 * TODO(mmenke):  Add a test for a log created with --log-net-log.
 */
TEST_F('NetInternalsTest', 'netInternalsExportImportDump', function() {
  expectFalse(g_browser.isDisabled());
  expectTrue(SourceTracker.getInstance().getPrivacyStripping());
  NetInternalsTest.expectStatusViewNodeVisible(CaptureStatusView.MAIN_BOX_ID);

  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new CreateAndLoadLogTask('Detailed explanation.'));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.run(true);
});

/**
 * Exports a log dump to a string and loads it, and then repeats, making sure
 * we can export loaded logs.
 */
TEST_F('NetInternalsTest', 'netInternalsExportImportExportImport', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new CreateAndLoadLogTask('Random comment on the weather.'));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.addTask(new CreateAndLoadLogTask('Detailed explanation.'));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.run(true);
});

/**
 * Checks pressing the stop capturing button.
 */
TEST_F('NetInternalsTest', 'netInternalsStopCapturing', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  // Switching to stop capturing mode will load a log dump, which will update
  // the constants.
  taskQueue.addTask(new WaitForConstantsTask());

  taskQueue.addFunctionTask(
      NetInternalsTest.expectStatusViewNodeVisible.bind(
          null, HaltedStatusView.MAIN_BOX_ID));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.addFunctionTask(checkPrivacyStripping.bind(null, true));
  taskQueue.addFunctionTask(checkActiveView.bind(null,
                                                 ExportView.TAB_HANDLE_ID));
  taskQueue.run();

  // Simulate a click on the stop capturing button.
  $(CaptureStatusView.STOP_BUTTON_ID).click();
});

/**
 * Switches to stop capturing mode, then exports and imports a log dump.
 */
TEST_F('NetInternalsTest', 'netInternalsStopCapturingExportImport', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  // Switching to stop capturing mode will load a log dump, which will update
  // the constants.
  taskQueue.addTask(new WaitForConstantsTask());

  taskQueue.addFunctionTask(
      NetInternalsTest.expectStatusViewNodeVisible.bind(
          null, HaltedStatusView.MAIN_BOX_ID));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.addFunctionTask(checkPrivacyStripping.bind(null, true));
  taskQueue.addTask(new CreateAndLoadLogTask('Detailed explanation.'));
  taskQueue.addFunctionTask(checkViewsAfterLogLoaded);
  taskQueue.run();

  // Simulate a click on the stop capturing button.
  $(CaptureStatusView.STOP_BUTTON_ID).click();
});

})();  // Anonymous namespace
