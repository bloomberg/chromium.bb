// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testPassed = false;
var resultString = "";
var asyncCounter = 0;
var javascriptDone = false;
var vrDisplayPromiseDone = false;

function finishJavascriptStep() {
  javascriptDone = true;
}

function checkResultsForFailures(tests, harness_status) {
  testPassed = true;
  if (harness_status["status"] != 0) {
    testPassed = false;
    resultString += "Harness failed due to " +
                    (harness_status["status"] == 1 ? "error" : "timeout")
                    + ". ";
  }
  for (var test in tests) {
    let passed = (tests[test]["status"] == 0);
    if (!passed) {
      testPassed = false;
      resultString += "FAIL ";
    } else {
      resultString += "PASS ";
    }
    resultString += tests[test]["name"] +
                    (passed ? "" : (": " + tests[test]["message"])) +
                    ". ";
  }
}

add_completion_callback( (tests, harness_status) => {
  checkResultsForFailures(tests, harness_status);
  console.debug("Test result: " + (testPassed ? "Pass" : "Fail"));
  console.debug("Test result string: " + resultString);
  finishJavascriptStep();
});
