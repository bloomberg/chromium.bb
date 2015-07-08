// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var embedder = {};
embedder.setUp_ = function(config) {
  if (!config || !config.testServer)
    return;
};

window.runTest = function(testName, appToEmbed, secondAppToEmbed) {
  if (!embedder.test.testList[testName]) {
    LOG('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName](appToEmbed, secondAppToEmbed);
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    LOG('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

var checkExtensionAttribute = function(element, expectedValue) {
  embedder.test.assertEq(expectedValue, element.extension);
};

var checkSrcAttribute = function(element, expectedValue) {
  embedder.test.assertEq(expectedValue, element.src);
};

// Tests begin.
function testLoadAPIFunction(extensionIdOne, extensionIdTwo) {
  var extensionScheme = 'chrome-extension://';
  var srcOne = 'data:text/html,<body>One</body>';
  var srcTwo = 'data:text/html,<body>Two</body>';
  var invalidExtensionId = 'fakeExtension';

  var extensionview = document.querySelector('extensionview');

  var runStepOne = function() {
    // Call load with a specified extension ID and src.
    extensionview.load(extensionScheme + extensionIdOne + '/' + srcOne)
    .then(function onLoadResolved() {
      checkExtensionAttribute(extensionview, extensionIdOne);
      checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
          '/' + srcOne);
      runStepTwo();
    }, function onLoadRejected() {
      embedder.test.fail();
    });
  };

  var runStepTwo = function() {
    // Call load with the same extension Id and src.
    extensionview.load(extensionScheme + extensionIdOne + '/' + srcOne)
    .then(function onLoadResolved() {
      checkExtensionAttribute(extensionview, extensionIdOne);
      checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
          '/' + srcOne);
      runStepThree();
    }, function onLoadRejected() {
      embedder.test.fail();
    });
  };

  var runStepThree = function() {
    // Call load with the same extension Id and different src.
    extensionview.load(extensionScheme + extensionIdOne + '/' + srcTwo)
    .then(function onLoadResolved() {
      checkExtensionAttribute(extensionview, extensionIdOne);
      checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
          '/' + srcTwo);
      runStepFour();
    }, function onLoadRejected() {
      embedder.test.fail();
    });
  };

  var runStepFour = function() {
    // Call load with a new extension Id and src.
    extensionview.load(extensionScheme + extensionIdTwo + '/' + srcOne)
    .then(function onLoadResolved() {
      checkExtensionAttribute(extensionview, extensionIdTwo);
      checkSrcAttribute(extensionview, extensionScheme + extensionIdTwo +
          '/' + srcOne);
      runStepFive();
    }, function onLoadRejected() {
      embedder.test.fail();
    });
  };

  var runStepFive = function() {
    // Call load with an invalid extension.
    extensionview.load(extensionScheme + invalidExtensionId + '/' + srcOne)
    .then(function onLoadResolved() {
      embedder.test.fail();
    }, function onLoadRejected() {
      runStepSix();
    });
  };

  var runStepSix = function() {
    // Call load with a valid extension Id and src after an invalid call.
    extensionview.load(extensionScheme + extensionIdOne + '/' + srcTwo)
    .then(function onLoadResolved() {
      runStepSeven();
    }, function onLoadRejected() {
      embedder.test.fail();
    });
  };

  var runStepSeven = function() {
    // Call load with a null extension.
    extensionview.load(null)
    .then(function onLoadResolved() {
      embedder.test.fail();
    }, function onLoadRejected() {
      embedder.test.succeed();
    });
  };

  runStepOne();
};

function testQueuedLoadAPIFunction(extensionIdOne, extensionIdTwo) {
  var extensionScheme = 'chrome-extension://';
  var srcOne = 'data:text/html,<body>One</body>';
  var srcTwo = 'data:text/html,<body>Two</body>';
  var loadCallCount = 1;

  var extensionview = document.querySelector('extensionview');

  // Call load a first time with a specified extension ID and src.
  extensionview.load(extensionScheme + extensionIdOne + '/' + srcOne)
  .then(function onLoadResolved() {
    // First load call has completed.
    checkExtensionAttribute(extensionview, extensionIdOne);
    checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
        '/' + srcOne);
    embedder.test.assertEq(1, loadCallCount);
    // Checked for expected attributes and call order.
    loadCallCount++;
  }, function onLoadRejected() {
    embedder.test.fail();
  });

  // Call load a second time with the same extension Id and src.
  extensionview.load(extensionScheme + extensionIdOne + '/' + srcOne)
  .then(function onLoadResolved() {
    // Second load call has completed.
    checkExtensionAttribute(extensionview, extensionIdOne);
    checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
        '/' + srcOne);
    embedder.test.assertEq(2, loadCallCount);
    // Checked for expected attributes and call order.
    loadCallCount++;
  }, function onLoadRejected() {
    embedder.test.fail();
  });

  // Call load a third time with the same extension Id and different src.
  extensionview.load(extensionScheme + extensionIdOne + '/' + srcTwo)
  .then(function onLoadResolved() {
    // Third load call has completed.
    checkExtensionAttribute(extensionview, extensionIdOne);
    checkSrcAttribute(extensionview, extensionScheme + extensionIdOne +
        '/' + srcTwo);
    embedder.test.assertEq(3, loadCallCount);
    // Checked for expected attributes and call order.
    loadCallCount++;
  }, function onLoadRejected() {
    embedder.test.fail();
  });

  // Call load a fourth time with a new extension Id and src.
  extensionview.load(extensionScheme + extensionIdTwo + '/' + srcOne)
  .then(function onLoadResolved() {
    // Fourth load call has completed.
    checkExtensionAttribute(extensionview, extensionIdTwo);
    checkSrcAttribute(extensionview, extensionScheme + extensionIdTwo +
        '/' + srcOne);
    embedder.test.assertEq(4, loadCallCount);
    // Checked for expected attributes and call order.
    embedder.test.succeed();
  }, function onLoadRejected() {
    embedder.test.fail();
  });
};

embedder.test.testList = {
  'testLoadAPIFunction': testLoadAPIFunction,
  'testQueuedLoadAPIFunction': testQueuedLoadAPIFunction,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
