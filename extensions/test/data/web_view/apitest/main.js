// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};

// TODO(lfg) Move these functions to a common js.
window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    window.console.warn('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

embedder.test = {};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    window.console.warn('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    window.console.warn('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    window.console.warn('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};


// Tests begin.

// This test verifies that the allowtransparency property cannot be changed
// once set. The attribute can only be deleted.
function testAllowTransparencyAttribute() {
  var webview = document.createElement('webview');
  webview.src = 'data:text/html,webview test';
  webview.allowtransparency = true;

  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    webview.allowtransparency = false;
    embedder.test.assertTrue(webview.allowtransparency);
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    webview.removeAttribute('allowtransparency');
    embedder.test.assertFalse(webview.allowtransparency);
    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

function testAPIMethodExistence() {
  var apiMethodsToCheck = [
    'back',
    'find',
    'forward',
    'canGoBack',
    'canGoForward',
    'clearData',
    'getProcessId',
    'getZoom',
    'go',
    'print',
    'reload',
    'setZoom',
    'stop',
    'stopFinding',
    'terminate',
    'executeScript',
    'insertCSS',
    'getUserAgent',
    'isUserAgentOverridden',
    'setUserAgentOverride'
  ];
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    for (var i = 0; i < apiMethodsToCheck.length; ++i) {
      embedder.test.assertEq('function',
                             typeof webview[apiMethodsToCheck[i]]);
    }

    // Check contentWindow.
    embedder.test.assertEq('object', typeof webview.contentWindow);
    embedder.test.assertEq('function',
                           typeof webview.contentWindow.postMessage);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

// This test verifies that assigning the src attribute the same value it had
// prior to a crash spawns off a new guest process.
function testAssignSrcAfterCrash() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    if (!terminated) {
      webview.terminate();
      return;
    }
    // The guest has recovered after being terminated.
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(evt) {
    terminated = true;
    webview.setAttribute('src', 'data:text/html,test page');
  });
  webview.setAttribute('src', 'data:text/html,test page');
  document.body.appendChild(webview);
}

// Makes sure 'sizechanged' event is fired only if autosize attribute is
// specified.
// After loading <webview> without autosize attribute and a size, say size1,
// we set autosize attribute and new min size with size2. We would get (only
// one) sizechanged event with size1 as old size and size2 as new size.
function testAutosizeAfterNavigation() {
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        // This would be triggered after we set autosize attribute.
        embedder.test.assertEq(50, e.oldWidth);
        embedder.test.assertEq(100, e.oldHeight);
        embedder.test.assertTrue(e.newWidth >= 60 && e.newWidth <= 70);
        embedder.test.assertTrue(e.newHeight >= 110 && e.newHeight <= 120);

        // Remove autosize attribute and expect webview to return to its
        // original size.
        webview.removeAttribute('autosize');
        break;
      case 2:
        // Expect 50x100.
        embedder.test.assertEq(50, e.newWidth);
        embedder.test.assertEq(100, e.newHeight);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.setAttribute('autosize', true);
    webview.setAttribute('minwidth', 60);
    webview.setAttribute('maxwidth', 70);
    webview.setAttribute('minheight', 110);
    webview.setAttribute('maxheight', 120);
  });

  webview.style.width = '50px';
  webview.style.height = '100px';
  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// This test verifies that if a browser plugin is in autosize mode before
// navigation then the guest starts auto-sized.
function testAutosizeBeforeNavigation() {
  var webview = document.createElement('webview');

  webview.setAttribute('autosize', 'true');
  webview.setAttribute('minwidth', 200);
  webview.setAttribute('maxwidth', 210);
  webview.setAttribute('minheight', 100);
  webview.setAttribute('maxheight', 110);

  webview.addEventListener('sizechanged', function(e) {
    embedder.test.assertEq(0, e.oldWidth);
    embedder.test.assertEq(0, e.oldHeight);
    embedder.test.assertTrue(e.newWidth >= 200 && e.newWidth <= 210);
    embedder.test.assertTrue(e.newHeight >= 100 && e.newHeight <= 110);
    embedder.test.succeed();
  });

  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// This test verifies that a lengthy page with autosize enabled will report
// the correct height in the sizechanged event.
function testAutosizeHeight() {
  var webview = document.createElement('webview');

  webview.autosize = true;
  webview.minwidth = 200;
  webview.maxwidth = 210;
  webview.minheight = 40;
  webview.maxheight = 200;

  var step = 1;
  webview.addEventListener('sizechanged', function(e) {
    switch (step) {
      case 1:
        embedder.test.assertEq(0, e.oldHeight);
        embedder.test.assertEq(200, e.newHeight);
        // Change the maxheight to verify that we see the change.
        webview.maxheight = 50;
        break;
      case 2:
        embedder.test.assertEq(200, e.oldHeight);
        embedder.test.assertEq(50, e.newHeight);
        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }
    ++step;
  });

  webview.src = 'data:text/html,' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>';
  document.body.appendChild(webview);
}

// This test verifies that all autosize attributes can be removed
// without crashing the plugin, or throwing errors.
function testAutosizeRemoveAttributes() {
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        // This is the sizechanged event for autosize.

        // Remove attributes.
        webview.removeAttribute('minwidth');
        webview.removeAttribute('maxwidth');
        webview.removeAttribute('minheight');
        webview.removeAttribute('maxheight');
        webview.removeAttribute('autosize');

        // We'd get one more sizechanged event after we turn off
        // autosize.
        webview.style.width = '500px';
        webview.style.height = '500px';
        break;
      case 2:
        embedder.test.succeed();
        break;
    }

    ++step;
  };

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 600;
    webview.maxheight = 400;
    webview.autosize = true;
  });

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

// This test verifies that autosize works when some of the parameters are unset.
function testAutosizeWithPartialAttributes() {
  window.console.log('testAutosizeWithPartialAttributes');
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    window.console.log('sizeChangeHandler, new: ' +
                       e.newWidth + ' X ' + e.newHeight);
    switch (step) {
      case 1:
        // Expect 300x200.
        embedder.test.assertEq(300, e.newWidth);
        embedder.test.assertEq(200, e.newHeight);

        // Change the min size to cause a relayout.
        webview.minwidth = 500;
        break;
      case 2:
        embedder.test.assertTrue(e.newWidth >= webview.minwidth);
        embedder.test.assertTrue(e.newWidth <= webview.maxwidth);

        // Tests when minwidth > maxwidth, minwidth = maxwidth.
        // i.e. minwidth is essentially 700.
        webview.minwidth = 800;
        break;
      case 3:
        // Expect 700X?
        embedder.test.assertEq(700, e.newWidth);
        embedder.test.assertTrue(e.newHeight >= 200);
        embedder.test.assertTrue(e.newHeight <= 600);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 200;
    webview.maxheight = 600;
    webview.autosize = true;
  });

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

// This test registers two event listeners on a same event (loadcommit).
// Each of the listener tries to change some properties on the event param,
// which should not be possible.
function testCannotMutateEventName() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Two</body>';
  var loadCommitACalled = false;
  var loadCommitBCalled = false;

  var maybeFinishTest = function(e) {
    if (loadCommitACalled && loadCommitBCalled) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.succeed();
    }
  };

  var onLoadCommitA = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitACalled);
      loadCommitACalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };
  var onLoadCommitB = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitBCalled);
      loadCommitBCalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };

  // The test starts from here, by setting the src to |url|. Event
  // listener registration works because we already have a (dummy) src set
  // on the <webview> tag.
  webview.addEventListener('loadcommit', onLoadCommitA);
  webview.addEventListener('loadcommit', onLoadCommitB);
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// This test verifies that the load event fires when the a new page is
// loaded.
// TODO(fsamuel): Add a test to verify that subframe loads within a guest
// do not fire the 'contentload' event.
function testContentLoadEvent() {
  var webview = document.createElement('webview');
  webview.addEventListener('contentload', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test registers two listeners on an event (loadcommit) and removes
// the <webview> tag when the first listener fires.
// Current expected behavior is that the second event listener will still
// fire without crashing.
function testDestroyOnEventListener() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Destroy test</body>';

  var loadCommitCount = 0;
  function loadCommitCommon(e) {
    embedder.test.assertEq('loadcommit', e.type);
    if (url != e.url)
      return;
    ++loadCommitCount;
    if (loadCommitCount == 2) {
      // Pass in a timeout so that we can catch if any additional loadcommit
      // occurs.
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
    } else if (loadCommitCount > 2) {
      embedder.test.fail();
    }
  };

  // The test starts from here, by setting the src to |url|.
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit1');
    webview.parentNode.removeChild(webview);
    loadCommitCommon(e);
  });
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit2');
    loadCommitCommon(e);
  });
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// Tests that a <webview> that starts with "display: none" style loads
// properly.
function testDisplayNoneWebviewLoad() {
  var webview = document.createElement('webview');
  var visible = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visible);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    visible = true;
    // This should trigger loadstop.
    webview.style.display = '';
  }, 0);
}

function testDisplayNoneWebviewRemoveChild() {
  var webview = document.createElement('webview');
  var visibleAndInDOM = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visibleAndInDOM);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    webview.parentNode.removeChild(webview);
    webview.style.display = '';
    visibleAndInDOM = true;
    // This should trigger loadstop.
    document.body.appendChild(webview);
  }, 0);
}

function testExecuteScript() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function() {
    webview.executeScript(
      {code:'document.body.style.backgroundColor = "red";'},
      function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);
        embedder.test.succeed();
      });
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

function testExecuteScriptFail() {
  var webview = document.createElement('webview');
  try {
    webview.executeScript(
        {code: 'document.body.style.backgroundColor = "red";'},
        function(results) { embedder.test.fail(); });
  }
  catch (e) {
    embedder.test.succeed();
  }
}



// Tests end.

embedder.test.testList = {
  'testAllowTransparencyAttribute': testAllowTransparencyAttribute,
  'testAPIMethodExistence': testAPIMethodExistence,
  'testAssignSrcAfterCrash': testAssignSrcAfterCrash,
  'testAutosizeAfterNavigation': testAutosizeAfterNavigation,
  'testAutosizeBeforeNavigation': testAutosizeBeforeNavigation,
  'testAutosizeHeight': testAutosizeHeight,
  'testAutosizeRemoveAttributes': testAutosizeRemoveAttributes,
  'testAutosizeWithPartialAttributes': testAutosizeWithPartialAttributes,
  'testCannotMutateEventName': testCannotMutateEventName,
  'testContentLoadEvent': testContentLoadEvent,
  'testDestroyOnEventListener': testDestroyOnEventListener,
  'testDisplayNoneWebviewLoad': testDisplayNoneWebviewLoad,
  'testDisplayNoneWebviewRemoveChild': testDisplayNoneWebviewRemoveChild,
  'testExecuteScript': testExecuteScript,
  'testExecuteScriptFail': testExecuteScriptFail
};

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
