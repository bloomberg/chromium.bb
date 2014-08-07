// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * Navigates to the test tab, runs the test suite once, using a URL
 * either passed to the constructor or passed in from the previous task, and
 * expects to see |expectedResult| from the first experiment.  If
 * |expectedResult| indicates failure, all experiments are expected to fail.
 * Otherwise, results of all experiments but the first are ignored.
 * Checks the order of events and several fields of the events received.
 *
 * Note that it is not safe for one RunTestSuiteTasks to run right after
 * another.  Otherwise, the second Task would receive the final observer
 * message from the first task.
 *
 * @param {int} expectedResult Expected result of first test.
 * @param {string} url URL to test.  If null, will use parameter passed to
 *     start instead.
 * @extends {NetInternalsTest.Task}
 * @constructor
 */
function RunTestSuiteTask(expectedResult, url) {
  NetInternalsTest.Task.call(this);
  // Complete asynchronously, so two RunTestSuiteTask can be run in a row.
  this.setCompleteAsync(true);

  this.url_ = url;
  this.expectedResult_ = expectedResult;
  this.seenStartSuite_ = false;
  this.seenStartExperiment_ = false;
  this.experimentsRun_ = 0;
}

RunTestSuiteTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Switches to the test view, and simulates entering |url| and submitting the
   * form.  Also adds the task as a ConnectionTestObserver.
   * @param {string} url URL to run the test suite on.
   */
  start: function(url) {
    NetInternalsTest.switchToView('tests');
    if (this.url_ === null) {
      assertEquals('string', typeof(url));
      this.url_ = url;
    }

    g_browser.addConnectionTestsObserver(this);

    $(TestView.URL_INPUT_ID).value = this.url_;
    $(TestView.SUBMIT_BUTTON_ID).click();
  },

  /**
   * Checks that the table was created/cleared, and that no experiment is
   * currently running.
   */
  onStartedConnectionTestSuite: function() {
    if (this.isDone())
      return;
    expectFalse(this.seenStartSuite_, 'Suite started more than once.');
    checkTestTableRows(0);
    expectEquals(this.experimentsRun_, 0);
    expectFalse(this.seenStartSuite_);

    this.seenStartSuite_ = true;
  },

  /**
   * Checks that the table has one row per started experiment, and the events
   * occur in the proper order.
   * @param {object} experiment Experiment that was just started.
   */
  onStartedConnectionTestExperiment: function(experiment) {
    if (this.isDone())
      return;
    console.log('Experiment: ' + this.experimentsRun_);
    expectEquals(this.url_, experiment.url, 'Test run on wrong URL');
    expectTrue(this.seenStartSuite_, 'Experiment started before suite.');
    expectFalse(this.seenStartExperiment_,
                'Two experiments running at once.');
    checkTestTableRows(this.experimentsRun_ + 1);

    this.seenStartExperiment_ = true;
  },

  /**
   * Checks that the table has one row per started experiment, and the events
   * occur in the proper order.
   * @param {object} experiment Experiment that finished.
   * @param {number} result Code indicating success or reason for failure.
   */
  onCompletedConnectionTestExperiment: function(experiment, result) {
    if (this.isDone())
      return;
    expectEquals(this.url_, experiment.url, 'Test run on wrong URL');
    // Can only rely on the error code of the first test.
    if (this.experimentsRun_ == 0)
      expectEquals(this.expectedResult_, result);
    // If the first expected result is an error, all tests should return some
    // error.
    if (this.expectedResult_ < 0)
      expectLT(result, 0);

    expectTrue(this.seenStartExperiment_,
               'Experiment stopped without starting.');
    checkTestTableRows(this.experimentsRun_ + 1);

    this.seenStartExperiment_ = false;
    ++this.experimentsRun_;
  },

  /**
   * Checks that we've received all 12 sets of results, and either runs the
   * next test iteration, or ends the test, depending on the total number of
   * iterations.
   */
  onCompletedConnectionTestSuite: function() {
    if (this.isDone())
      return;
    expectTrue(this.seenStartSuite_, 'Suite stopped without being started.');
    expectFalse(this.seenStartExperiment_,
                'Suite stopped while experiment was still running.');
    expectEquals(12, this.experimentsRun_,
                 'Incorrect number of experiments run.');
    checkTestTableRows(this.experimentsRun_);

    this.onTaskDone();
  }
};

/**
 * Checks that there are |expected| rows in the test table.
 * @param {number} expectedRows Expected number of rows in the table.
 */
function checkTestTableRows(expectedRows) {
  NetInternalsTest.checkTbodyRows(TestView.SUMMARY_DIV_ID, expectedRows);
}

/**
 * Runs the test suite twice, expecting a passing result the first time.  Checks
 * the first result, the order of events that occur, and the number of rows in
 * the table.  Uses the TestServer.
 */
TEST_F('NetInternalsTest', 'netInternalsTestViewPassTwice', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask('files/title1.html'));
  // 0 indicates success, the null means we'll use the URL resulting from the
  // previous task.
  taskQueue.addTask(new RunTestSuiteTask(0, null));

  taskQueue.addTask(
      new NetInternalsTest.GetTestServerURLTask('files/title2.html'));
  // 0 indicates success, the null means we'll use the URL resulting from the
  // previous task.
  taskQueue.addTask(new RunTestSuiteTask(0, null));
  taskQueue.run();
});

/**
 * Runs the test suite twice.  Checks the exact error code of the first result,
 * the order of events that occur, and the number of rows in the HTML table.
 * Does not use the TestServer.
 */
TEST_F('NetInternalsTest', 'netInternalsTestViewFailTwice', function() {
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new RunTestSuiteTask(NetError.ERR_UNSAFE_PORT,
                                         'http://127.0.0.1:7/'));
  taskQueue.addTask(new RunTestSuiteTask(NetError.ERR_UNSAFE_PORT,
                                         'http://127.0.0.1:7/'));
  taskQueue.run();
});

})();  // Anonymous namespace
