// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var util = {};
var embedder = {};
embedder.baseGuestURL = '';
embedder.emptyGuestURL = '';
embedder.windowOpenGuestURL = '';
embedder.noReferrerGuestURL = '';
embedder.redirectGuestURL = '';
embedder.redirectGuestURLDest = '';
embedder.closeSocketURL = '';
embedder.tests = {};

embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.emptyGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/empty_guest.html';
  embedder.windowOpenGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest.html';
  embedder.noReferrerGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest_noreferrer.html';
  embedder.redirectGuestURL = embedder.baseGuestURL + '/server-redirect';
  embedder.redirectGuestURLDest = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest_redirect.html';
  embedder.closeSocketURL = embedder.baseGuestURL + '/close-socket';
};

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

// Creates a <webview> tag in document.body and returns the reference to it.
// It also sets a dummy src. The dummy src is significant because this makes
// sure that the <object> shim is created (asynchronously at this point) for the
// <webview> tag. This makes the <webview> tag ready for add/removeEventListener
// calls.
util.createWebViewTagInDOM = function(partitionName) {
  var webview = document.createElement('webview');
  webview.style.width = '300px';
  webview.style.height = '200px';
  var urlDummy = 'data:text/html,<body>Initial dummy guest</body>';
  webview.setAttribute('src', urlDummy);
  webview.setAttribute('partition', partitionName);
  document.body.appendChild(webview);
  return webview;
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('DoneShimTest.PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('DoneShimTest.FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

// Tests begin.

function testSize() {
  var webview = document.querySelector('webview');
  // Since we can't currently inspect the page loaded inside the <webview>,
  // the only way we can check that the shim is working is by changing the
  // size and seeing if the shim updates the size of the DOM.
  embedder.test.assertEq(300, webview.offsetWidth);
  embedder.test.assertEq(200, webview.offsetHeight);

  webview.style.width = '310px';
  webview.style.height = '210px';

  embedder.test.assertEq(310, webview.offsetWidth);
  embedder.test.assertEq(210, webview.offsetHeight);

  webview.style.width = '320px';
  webview.style.height = '220px';

  embedder.test.assertEq(320, webview.offsetWidth);
  embedder.test.assertEq(220, webview.offsetHeight);

  var dynamicWebViewTag = document.createElement('webview');
  dynamicWebViewTag.setAttribute('src', 'data:text/html,dynamic browser');
  dynamicWebViewTag.style.width = '330px';
  dynamicWebViewTag.style.height = '230px';
  document.body.appendChild(dynamicWebViewTag);

  // Timeout is necessary to give the mutation observers a chance to fire.
  setTimeout(function() {
    embedder.test.assertEq(330, dynamicWebViewTag.offsetWidth);
    embedder.test.assertEq(230, dynamicWebViewTag.offsetHeight);

    embedder.test.succeed();
  }, 0);
}

function testAPIMethodExistence() {
  var apiMethodsToCheck = [
    'back',
    'canGoBack',
    'canGoForward',
    'forward',
    'getProcessId',
    'go',
    'reload',
    'stop',
    'terminate'
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

function testWebRequestAPIExistence() {
  var apiPropertiesToCheck = [
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onAuthRequired',
    'onBeforeRedirect',
    'onResponseStarted',
    'onCompleted',
    'onErrorOccurred'
  ];
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    for (var i = 0; i < apiPropertiesToCheck.length; ++i) {
      embedder.test.assertEq('object',
                             typeof webview[apiPropertiesToCheck[i]]);
      embedder.test.assertEq(
          'function', typeof webview[apiPropertiesToCheck[i]].addListener);
      embedder.test.assertEq(webview[apiPropertiesToCheck[i]],
                             webview.request[apiPropertiesToCheck[i]]);
    }

    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

// This test verifies that the loadstart, loadstop, and exit events fire as
// expected.
function testEventName() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);

  webview.addEventListener('loadstart', function(evt) {
    embedder.test.assertEq('loadstart', evt.type);
  });

  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
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
  var webview = util.createWebViewTagInDOM(arguments.callee.name);
  var url = 'data:text/html,<body>Destroy test</body>';

  var loadCommitCount = 0;
  function loadCommitCommon(e) {
    embedder.test.assertEq('loadcommit', e.type);
    if (url != e.url)
      return;
    ++loadCommitCount;
    if (loadCommitCount == 1) {
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
    } else if (loadCommitCount > 2) {
      embedder.test.fail();
    }
  };

  // The test starts from here, by setting the src to |url|.
  webview.addEventListener('loadcommit', function(e) {
    webview.parentNode.removeChild(webview);
    loadCommitCommon(e);
  });
  webview.addEventListener('loadcommit', function(e) {
    loadCommitCommon(e);
  });
  webview.setAttribute('src', url);
}

// This test registers two event listeners on a same event (loadcommit).
// Each of the listener tries to change some properties on the event param,
// which should not be possible.
function testCannotMutateEventName() {
  var webview = util.createWebViewTagInDOM(arguments.callee.name);
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
}

// This test verifies that setting the partition attribute after the src has
// been set raises an exception.
function testPartitionRaisesException() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
  setTimeout(function() {
    try {
      webview.partition = 'illegal';
      embedder.test.fail();
    } catch (e) {
      embedder.test.succeed();
    }
  }, 0);
}

function testExecuteScriptFail() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);
  setTimeout(function() {
    try {
    webview.executeScript(
      {code:'document.body.style.backgroundColor = "red";'},
      function(results) {
        embedder.test.fail();
      });
    } catch (e) {
      embedder.test.succeed();
    }
  }, 0);
}

function testExecuteScript() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
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

// This test calls terminate() on guest after it has already been
// terminated. This makes sure we ignore the call gracefully.
function testTerminateAfterExit() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var loadstopSucceedsTest = false;
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    if (loadstopSucceedsTest) {
      embedder.test.succeed();
      return;
    }

    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
    // Call terminate again.
    webview.terminate();
    // Load another page. The test would pass when loadstop is called on
    // this second page. This would hopefully catch if call to
    // webview.terminate() caused a browser crash.
    setTimeout(function() {
      loadstopSucceedsTest = true;
      webview.setAttribute('src', 'data:text/html,test second page');
    }, 0);
  });

  webview.setAttribute('src', 'data:text/html,test terminate() crash.');
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

// This test verifies that <webview> restores the src attribute if it is
// removed after navigation.
function testRemoveSrcAttribute() {
  var dataUrl = 'data:text/html,test page';
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    webview.removeAttribute('src');
    setTimeout(function() {
      embedder.test.assertEq(dataUrl, webview.getAttribute('src'));
      embedder.test.succeed();
    }, 0);
  });
  webview.setAttribute('src', dataUrl);
  document.body.appendChild(webview);
}

// This test verifies that it is not possible to instantiate a browser plugin
// directly within an app.
function testBrowserPluginNotAllowed() {
  var container = document.getElementById('object-container');
  if (!container) {
    embedder.test.fail('Container for object not found.');
    return;
  }
  container.innerHTML = '<object type="application/browser-plugin"' +
      ' id="object-plugin"' +
      ' src="data:text/html,<body>You should not see this</body>">' +
      '</object>';
  var objectElement = document.getElementById('object-plugin');
  // Check that bindings are not registered.
  embedder.test.assertTrue(
      objectElement['-internal-setPermission'] === undefined);
  embedder.test.succeed();
}

// This test verifies that new window attachment functions as expected.
function testNewWindow() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

// This test verifies "first-call-wins" semantics. That is, the first call
// to perform an action on the new window takes the action and all
// subsequent calls throw an exception.
function testNewWindowTwoListeners() {
  var webview = document.createElement('webview');
  var error = false;
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    try {
      e.window.attach(newwebview);
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    try {
      e.window.discard();
    } catch (err) {
      embedder.test.succeed();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

// This test verifies that the attach can be called inline without
// preventing default.
function testNewWindowNoPreventDefault() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    try {
      e.window.attach(newwebview);
      embedder.test.succeed();
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

function testNewWindowNoReferrerLink() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.noReferrerGuestURL);
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

// This test verifies that the WebRequest API onBeforeRequest event fires on
// webview.
function testWebRequestAPI() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  var firstLoad = function() {
    webview.removeEventListener('loadstop', firstLoad);
    webview.onBeforeRequest.addListener(function(e) {
      embedder.test.succeed();
    }, { urls: ['<all_urls>']}, ['blocking']) ;
    webview.src = embedder.windowOpenGuestURL;
  };
  webview.addEventListener('loadstop', firstLoad);
  document.body.appendChild(webview);
}

// This test verifies that getProcessId is defined and returns a non-zero
// value corresponding to the processId of the guest process.
function testGetProcessId() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  var firstLoad = function() {
    webview.removeEventListener('loadstop', firstLoad);
    embedder.test.assertTrue(webview.getProcessId() > 0);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', firstLoad);
  document.body.appendChild(webview);
}

// This test verifies that the loadstart event fires at the beginning of a load
// and the loadredirect event fires when a redirect occurs.
function testLoadStartLoadRedirect() {
  var webview = document.createElement('webview');
  var loadstartCalled = false;
  webview.setAttribute('src', embedder.redirectGuestURL);
  webview.addEventListener('loadstart', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL, e.url);
    loadstartCalled = true;
  });
  webview.addEventListener('loadredirect', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL,
        e.oldUrl.replace('127.0.0.1', 'localhost'));
    embedder.test.assertEq(embedder.redirectGuestURLDest,
        e.newUrl.replace('127.0.0.1', 'localhost'));
    if (loadstartCalled) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected and with the
// appropriate fields when an empty response is returned.
function testLoadAbortEmptyResponse() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_EMPTY_RESPONSE', e.reason);
    embedder.test.succeed();
  });
  webview.setAttribute('src', embedder.closeSocketURL);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected when an illegal
// chrome URL is provided.
function testLoadAbortIllegalChromeURL() {
  var webview = document.createElement('webview');
  var onFirstLoadStop = function(e) {
    webview.removeEventListener('loadstop', onFirstLoadStop);
    webview.setAttribute('src', 'chrome://newtab');
  };
  webview.addEventListener('loadstop', onFirstLoadStop);
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_ABORTED', e.reason);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);
}

function testLoadAbortIllegalFileURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_ABORTED', e.reason);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'file://foo');
  document.body.appendChild(webview);
}

// This test verifies that the reload method on webview functions as expected.
function testReload() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  var loadCommitCount = 0;
  webview.addEventListener('loadstop', function(e) {
    if (loadCommitCount < 2) {
      webview.reload();
    } else if (loadCommitCount == 2) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  webview.addEventListener('loadcommit', function(e) {
    embedder.test.assertEq(triggerNavUrl, e.url);
    embedder.test.assertTrue(e.isTopLevel);
    loadCommitCount++;
  });

  webview.setAttribute('src', triggerNavUrl);
  document.body.appendChild(webview);
}

// This test verifies that a <webview> is torn down gracefully when removed from
// the DOM on exit.

window.removeWebviewOnExitDoCrash = null;

function testRemoveWebviewOnExit() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  webview.addEventListener('loadstop', function(e) {
    chrome.test.sendMessage('guest-loaded');
  });

  window.removeWebviewOnExitDoCrash = function() {
    webview.terminate();
  };

  webview.addEventListener('exit', function(e) {
    // We expected to be killed.
    if (e.reason != 'killed') {
      console.log('EXPECTED TO BE KILLED!');
      return;
    }
    webview.parentNode.removeChild(webview);
  });

  // Trigger a navigation to create a guest process.
  webview.setAttribute('src', embedder.emptyGuestURL);
  document.body.appendChild(webview);
}

embedder.test.testList = {
  'testSize': testSize,
  'testAPIMethodExistence': testAPIMethodExistence,
  'testWebRequestAPIExistence': testWebRequestAPIExistence,
  'testEventName': testEventName,
  'testDestroyOnEventListener': testDestroyOnEventListener,
  'testCannotMutateEventName': testCannotMutateEventName,
  'testPartitionRaisesException': testPartitionRaisesException,
  'testExecuteScriptFail': testExecuteScriptFail,
  'testExecuteScript': testExecuteScript,
  'testTerminateAfterExit': testTerminateAfterExit,
  'testAssignSrcAfterCrash': testAssignSrcAfterCrash,
  'testRemoveSrcAttribute': testRemoveSrcAttribute,
  'testBrowserPluginNotAllowed': testBrowserPluginNotAllowed,
  'testNewWindow': testNewWindow,
  'testNewWindowTwoListeners': testNewWindowTwoListeners,
  'testNewWindowNoPreventDefault': testNewWindowNoPreventDefault,
  'testNewWindowNoReferrerLink': testNewWindowNoReferrerLink,
  'testContentLoadEvent': testContentLoadEvent,
  'testWebRequestAPI': testWebRequestAPI,
  'testGetProcessId': testGetProcessId,
  'testLoadStartLoadRedirect': testLoadStartLoadRedirect,
  'testLoadAbortEmptyResponse': testLoadAbortEmptyResponse,
  'testLoadAbortIllegalChromeURL': testLoadAbortIllegalChromeURL,
  'testLoadAbortIllegalFileURL': testLoadAbortIllegalFileURL,
  'testReload': testReload,
  'testRemoveWebviewOnExit': testRemoveWebviewOnExit
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage("Launched");
  });
};
