// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Navigates to the test tab, runs the test suite |totalIterations| times, using
 * |url|, and expects to see |expectedResult| from each iteration.  Checks the
 * order and several fields of the events received.
 */
netInternalsTest.test('netInternalsTestView',
                      function(url, expectedResult, totalIterations) {
  /**
   * @param {string} url URL to run the test suite on.
   * @param {number} expectedResult Expected result of the first test.
   * @param {number} totalIterations Number of times to run the test suite.
   * @constructor
   */
  function TestObserver(url, expectedResult, totalIterations) {
    this.url_ = url;
    this.totalIterations_ = totalIterations;
    this.expectedResult_ = expectedResult;
    this.completedIterations_ = 0;
  }

  TestObserver.prototype = {
    /**
     * Starts running the test suite.
     */
    startTestSuite: function() {
      // Initialize state used to track test suite progress.
      this.seenStartSuite_ = false;
      this.seenStartExperiment_ = false;
      this.experimentsRun_ = 0;

      // Simulate entering the url and submitting the form.
      $(TestView.URL_INPUT_ID).value = this.url_;
      $(TestView.SUBMIT_BUTTON_ID).click();
    },

    /**
     * Checks that the table was created/cleared, and that no experiment is
     * currently running.
     */
    onStartedConnectionTestSuite: function() {
      expectFalse(this.seenStartSuite_, 'Suite started more than once.');
      checkTestTableRows(0);
      expectEquals(this.experimentsRun_, 0);
      expectFalse(this.seenStartExperiment_, 0);

      this.seenStartSuite_ = true;
    },

    /**
     * Checks that the table has one row per started experiment, and the events
     * occur in the proper order.
     * @param {Object} experiment Experiment that was just started.
     */
    onStartedConnectionTestExperiment: function(experiment) {
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
     * @param {Object} experiment Experiment that finished.
     * @param {number} result Code indicating success or reason for failure.
     */
    onCompletedConnectionTestExperiment: function(experiment, result) {
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
      expectTrue(this.seenStartSuite_, 'Suite stopped without being started.');
      expectFalse(this.seenStartExperiment_,
                  'Suite stopped while experiment was still running.');
      expectEquals(12, this.experimentsRun_,
                   'Incorrect number of experiments run.');
      checkTestTableRows(this.experimentsRun_);

      ++this.completedIterations_;
      if (this.completedIterations_ < this.totalIterations_) {
        this.startTestSuite();
      } else {
        testDone();
      }
    }
  };

  /**
   * Checks that there are |expected| rows in the test table.
   * @param {number} expectedRows Expected number of rows in the table.
   */
  function checkTestTableRows(expectedRows) {
    netInternalsTest.checkStyledTableRows(TestView.SUMMARY_DIV_ID,
                                          expectedRows);
  }

  netInternalsTest.switchToView('tests');

  // Create observer and start the test.
  var testObserver = new TestObserver(url, expectedResult, totalIterations);
  g_browser.addConnectionTestsObserver(testObserver);
  testObserver.startTestSuite();
});
