// api_test.js
// mini-framework for ExtensionApiTest browser tests
// TODO(erikkay) - figure out a way to share this code across extensions

var completed = false;
var tests;
var currentTest;

function complete() {
  completed = true;

  // a bit of a hack just to try to get the script to stop running at this point
  throw "completed";
}

function fail(message) {
  if (completed) throw "completed";

  var stack;
  try {
    crash.me += 0;  // intentional exception to get the stack trace
  } catch (e) {
    stack = e.stack.split("\n");
    stack = stack.slice(2);  // remove title and fail()
    stack = stack.join("\n");
  }

  if (!message) {
    message = "FAIL (no message)";
  }
  message += "\n" + stack;
  console.log("[FAIL] " + currentTest.name + ": " + message);
  chrome.test.fail(message);
  complete();
}

function allTestsSucceeded() {
  console.log("All tests succeeded");
  if (completed) throw "completed";

  chrome.test.pass();
  complete();
}

function runNextTest() {
  currentTest = tests.shift();
  if (!currentTest) {
    allTestsSucceeded();
    return;
  }
  currentTest.call();
}

function succeed() {
  console.log("[SUCCESS] " + currentTest.name);
  runNextTest();
}

window.onerror = function(message, url, code) {
  if (completed) return;

  fail(message);
};

function assertTrue(test, message) {
  if (test !== true) {
    if (typeof(test) == "string") {
      if (message) {
        message = test + "\n" + message;
      } else {
        message = test;
      }
    }
    fail(message);
  }
}

function assertNoLastError() {
  if (chrome.extension.lastError != undefined) {
    fail("lastError.message == " + chrome.extension.lastError.message);
  }
}

// Wrapper for generating test functions, that takes care of calling
// assertNoLastError() and succeed() for you.
function testFunction(func) {
  return function() {
    assertNoLastError();
    try {
      func.apply(null, arguments);
    } catch (e) {
      var stack = null;
      if (typeof(e.stack) != "undefined") {
        stack = e.stack.toString()
      }
      var msg = "Exception during execution of testFunction in " +
                currentTest.name;
      if (stack) {
        msg += "\n" + stack;
      } else {
        msg += "\n(no stack available)";
      }
      fail(msg);
    }
    succeed();
  };
}
