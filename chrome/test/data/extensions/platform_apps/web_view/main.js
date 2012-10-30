// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var util = {};

// Creates a <webview> tag in document.body and returns the reference to it.
// It also sets a dummy src. The dummy src is significant because this makes
// sure that the <object> shim is created (asynchronously at this point) for the
// <webview> tag. This makes the <webview> tag ready for add/removeEventListener
// calls.
util.createWebViewTagInDOM = function() {
  var webview = document.createElement('webview');
  webview.style.width = '300px';
  webview.style.height = '200px';
  document.body.appendChild(webview);
  var urlDummy = 'data:text/html,<body>Initial dummy guest</body>';
  webview.setAttribute('src', urlDummy);
  return webview;
};

onload = function() {
  chrome.test.runTests([
    function webView() {
      var webview = document.querySelector('webview');
      // Since we can't currently inspect the page loaded inside the <webview>,
      // the only way we can check that the shim is working is by changing the
      // size and seeing if the shim updates the size of the DOM.
      chrome.test.assertEq(300, webview.offsetWidth);
      chrome.test.assertEq(200, webview.offsetHeight);

      webview.style.width = '310px';
      webview.style.height = '210px';

      chrome.test.assertEq(310, webview.offsetWidth);
      chrome.test.assertEq(210, webview.offsetHeight);

      webview.style.width = '320px';
      webview.style.height = '220px';

      chrome.test.assertEq(320, webview.offsetWidth);
      chrome.test.assertEq(220, webview.offsetHeight);

      var dynamicWebViewTag = document.createElement('webview');
      dynamicWebViewTag.setAttribute('src', 'data:text/html,dynamic browser');
      dynamicWebViewTag.style.width = '330px';
      dynamicWebViewTag.style.height = '230px';
      document.body.appendChild(dynamicWebViewTag);

      // Timeout is necessary to give the mutation observers a chance to fire.
      setTimeout(function() {
        chrome.test.assertEq(330, dynamicWebViewTag.offsetWidth);
        chrome.test.assertEq(230, dynamicWebViewTag.offsetHeight);

        chrome.test.succeed();
      }, 0);
    },

    function webViewApiMethodExistence() {
      var webview = document.createElement('webview');
      webview.setAttribute('src', 'data:text/html,webview check api');
      var apiMethodsToCheck = [
        'addEventListener',
        'back',
        'canGoBack',
        'canGoForward',
        'forward',
        'getProcessId',
        'go',
        'reload',
        'removeEventListener',
        'stop',
        'terminate'
      ];
      document.body.appendChild(webview);

      // Timeout is necessary to give the mutation observers of the webview tag
      // shim a chance to fire.
      setTimeout(function() {
        for (var i = 0; i < apiMethodsToCheck.length; ++i) {
          chrome.test.assertEq('function',
                               typeof webview[apiMethodsToCheck[i]]);
        }

        // Check contentWindow.
        chrome.test.assertEq('object', typeof webview.contentWindow);
        chrome.test.assertEq('function',
                             typeof webview.contentWindow.postMessage);

        chrome.test.succeed();
      }, 0);
    },

    function webViewEventListeners() {
      var webview = document.createElement('webview');
      webview.setAttribute('src', 'data:text/html,webview check api');
      document.body.appendChild(webview);

      var validEvents = [
        'exit',
        'loadabort',
        'loadredirect',
        'loadstart',
        'loadstop'
      ];
      var invalidEvents = [
        'makemesandwich',
        'sudomakemesandwich'
      ];

      // Timeout is necessary to give the mutation observers of the webview tag
      // shim a chance to fire.
      setTimeout(function() {
        for (var i = 0; i < validEvents.length; ++i) {
          chrome.test.assertTrue(
              webview.addEventListener(validEvents[i], function() {}));
        }

        for (var i = 0; i < invalidEvents.length; ++i) {
          chrome.test.assertFalse(
              webview.addEventListener(invalidEvents[i], function() {}));
        }

        chrome.test.succeed();
      }, 0);
    },

    function webViewEventName() {
      var webview = document.createElement('webview');
      webview.setAttribute('src', 'data:text/html,webview check api');
      document.body.appendChild(webview);

      setTimeout(function() {
        webview.addEventListener('loadstart', function(evt) {
          chrome.test.assertEq('loadstart', evt.name);
        });

        webview.addEventListener('loadstop', function(evt) {
          chrome.test.assertEq('loadstop', evt.name);
          webview.terminate();
        });

        webview.addEventListener('exit', function(evt) {
          chrome.test.assertEq('exit', evt.name);
          chrome.test.succeed();
        });

        webview.setAttribute('src', 'data:text/html,trigger navigation');
      }, 0);
    },

    // This test registers two listeners on an event (loadcommit) and removes
    // the <webview> tag when the first listener fires.
    // Current expected behavior is that the second event listener will still
    // fire (and we don't crash).
    function webviewDestroyOnEventListener() {
      var webview = util.createWebViewTagInDOM();
      var url = 'data:text/html,<body>Destroy test</body>';

      var continuedAfterDelete = false;
      var loadCommitCount = 0;
      var updateLoadCommitCount = function() {
        ++loadCommitCount;
        if (loadCommitCount == 1) {
          // We remove the <webview> tag when the first listener fires.
          webview.parentNode.removeChild(webview);
          webview = null;
          // Make sure the js executes after nulling |webview|.
          continuedAfterDelete = true;
        } else if (loadCommitCount == 2) {
          chrome.test.assertTrue(continuedAfterDelete);
          chrome.test.succeed();
        }
      };

      var onLoadCommitA = function(e) {
        chrome.test.assertEq('loadcommit', e.name);
        if (e.url == url) {
          updateLoadCommitCount();
        }
      };
      var onLoadCommitB = function(e) {
        chrome.test.assertEq('loadcommit', e.name);
        if (e.url == url) {
          updateLoadCommitCount();
        }
      };

      setTimeout(function() {
        // The test starts from here, by setting the src to |url|. Event
        // listener registration works because we already have a (dummy) src set
        // on the <webview> tag.
        webview.addEventListener('loadcommit', onLoadCommitA);
        webview.addEventListener('loadcommit', onLoadCommitB);
        webview.setAttribute('src', url);
      }, 0);
    },

    // This test checks the current behavior of dynamic event listener
    // registration 'during' an event listener fire.
    // Once an event starts firing its listeners, any mutation to the list of
    // event listeners for this event are deferred until all of the listeners
    // have fired.
    //
    // Step1: Loads a guest with |urlStep1| with two listeners to 'loadcommit':
    // onLoadCommitA and onLoadCommitB. When first of them fires, we try to
    // remove the other and add a third listener (onLoadCommitC).
    // Current expected behavior is these add/remove will be ignored and the
    // original two listeners will fire.
    // Step2: We change the guest to |urlStep2|. This will cause 'loadcommit' to
    // fire with mutated event listeners list (that is one of
    // onLoadCommitA/onLoadCommitB and onLoadCommitC).
    function dynamicEventListenerRegistrationOnListenerFire() {
      var urlStep1 = 'data:text/html,<body>Two</body>';
      var urlStep2 = 'data:text/html,<body>Three</body>';
      var webview = util.createWebViewTagInDOM();

      var listenerFireCount1 = 0;  // Step 1 listeners.
      var listenerFireCount2 = 0;  // Step 2 listeners.

      var loadCommitACalledStep1 = false;
      var loadCommitBCalledStep1 = false;

      var loadCommitACalledStep2 = false;
      var loadCommitBCalledStep2 = false;
      var loadCommitCCalledStep2 = false;

      var expectAToFireInStep2;
      var expectBToFireInStep2;

      var updateTestStateOnListenerStep1 = function() {
        ++listenerFireCount1;
        if (listenerFireCount1 == 1) {  // First listener fire on Step 1.
          chrome.test.assertTrue(loadCommitACalledStep1 ||
                                 loadCommitBCalledStep1);
          // Add an event listener and remove one existing one when the first
          // listener fires. Current implmementation does not expect any of
          // these to be reflected before all event listeners are fired for the
          // current event.

          // Remove.
          if (loadCommitACalledStep1) {
            webview.removeEventListener('loadcommit', onLoadCommitB);
            expectAToFireInStep2 = true;
            expectBToFireInStep2 = false;
          } else {
            webview.removeEventListener('loadcommit', onLoadCommitA);
            expectAToFireInStep2 = false;
            expectBToFireInStep2 = true;
          }
          // Add the other one.
          webview.addEventListener('loadcommit', onLoadCommitC);
        } else if (listenerFireCount1 == 2) {  // Last listener fire on Step 1.
          chrome.test.assertTrue(loadCommitACalledStep1 &&
                                 loadCommitBCalledStep1);
          // Move to Step 2.
          webview.setAttribute('src', urlStep2);
        } else {
          // More than expected listeners fired.
          chrome.test.fail();
        }
      };

      var updateTestStateOnListenerStep2 = function() {
        ++listenerFireCount2;
        if (listenerFireCount2 == 2) {
          // Exactly one of onLoadCommitA and onLoadCommitB must be called.
          chrome.test.assertTrue(loadCommitACalledStep2 ||
                                 loadCommitBCalledStep2);
          // onLoadCommitC must be called.
          chrome.test.assertTrue(loadCommitCCalledStep2);

          chrome.test.succeed();
        } else if (listenerFireCount2 > 2) {
          // More than expected listeners fired.
          chrome.test.fail();
        }
      };

      var onLoadCommitA = function(e) {
        chrome.test.assertEq('loadcommit', e.name);
        switch (e.url) {
          case urlStep1:  // Step 1.
            chrome.test.log('Step 1. onLoadCommitA');
            chrome.test.assertFalse(loadCommitACalledStep1);
            loadCommitACalledStep1 = true;

            updateTestStateOnListenerStep1();
            break;
          case urlStep2:  //  Step 2.
            chrome.test.log('Step 2. onLoadCommitA');
            chrome.test.assertTrue(expectAToFireInStep2);
            // Can be called at most once.
            chrome.test.assertFalse(loadCommitACalledStep2);
            loadCommitACalledStep2 = true;

            updateTestStateOnListenerStep2();
            break;
        }
      };

      var onLoadCommitB = function(e) {
        chrome.test.assertEq('loadcommit', e.name);
        switch (e.url) {
          case urlStep1:  // Step 1.
            chrome.test.log('Step 1. onLoadCommitB');
            chrome.test.assertFalse(loadCommitBCalledStep1);
            loadCommitBCalledStep1 = true;

            updateTestStateOnListenerStep1();
            break;
          case urlStep2:  // Step 2.
            chrome.test.log('Step 2. onLoadCommitB');
            chrome.test.assertTrue(expecBToFireInStep2);
            // Can be called at most once.
            chrome.test.assertFalse(loadCommitBCalledStep2);
            loadCommitBCalledStep2 = true;

            updateTestStateOnListenerStep2();
            break;
        }
      };

      var onLoadCommitC = function(e) {
        chrome.test.assertEq('loadcommit', e.name);
        switch (e.url) {
          case urlStep1:  // Step 1.
            chrome.test.fail();
            break;
          case urlStep2:  // Step 2.
            chrome.test.log('Step 2. onLoadCommitC');
            chrome.test.assertFalse(loadCommitCCalledStep2);
            loadCommitCCalledStep2 = true;

            updateTestStateOnListenerStep2();
            break;
        }
      };

      setTimeout(function() {
        // The test starts from here, by setting the src to |urlStep1|. Event
        // listener registration works because we already have a (dummy) src set
        // on the <webview> tag.
        webview.addEventListener('loadcommit', onLoadCommitA);
        webview.addEventListener('loadcommit', onLoadCommitB);
        webview.setAttribute('src', urlStep1);
      }, 0);
    },

    // This test registers two event listeners on a same event (loadcommit).
    // Each of the listener tries to change some properties on the event param,
    // which should not be possible.
    function cannotMutateEventName() {
      var webview = util.createWebViewTagInDOM();
      var url = 'data:text/html,<body>Two</body>';

      var loadCommitACalled = false;
      var loadCommitBCalled = false;

      var maybeFinishTest = function(e) {
        if (loadCommitACalled && loadCommitBCalled) {
          chrome.test.assertEq('loadcommit', e.name);
          chrome.test.assertTrue(e.isTopLevel);
          chrome.test.succeed();
        }
      };

      var onLoadCommitA = function(e) {
        if (e.url == url) {
          chrome.test.assertEq('loadcommit', e.name);
          chrome.test.assertTrue(e.isTopLevel);
          chrome.test.assertFalse(loadCommitACalled);
          loadCommitACalled = true;
          // Try mucking with properities inside |e|.
          e.name = 'modified';
          e.isTopLevel = 'string-value';
          maybeFinishTest(e);
        }
      };
      var onLoadCommitB = function(e) {
        if (e.url == url) {
          chrome.test.assertEq('loadcommit', e.name);
          chrome.test.assertTrue(e.isTopLevel);
          chrome.test.assertFalse(loadCommitBCalled);
          loadCommitBCalled = true;
          // Try mucking with properities inside |e|.
          e.name = 'modified';
          e.isTopLevel = 'string-value';
          maybeFinishTest(e);
        }
      };

      setTimeout(function() {
        // The test starts from here, by setting the src to |url|. Event
        // listener registration works because we already have a (dummy) src set
        // on the <webview> tag.
        webview.addEventListener('loadcommit', onLoadCommitA);
        webview.addEventListener('loadcommit', onLoadCommitB);
        webview.setAttribute('src', url);
      }, 0);
    }
  ]);
};
