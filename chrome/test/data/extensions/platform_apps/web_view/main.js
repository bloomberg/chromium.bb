// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function webView() {
      var webview = document.querySelector('webview');
      // Since we can't currently inspect the page loaded inside the <webview>,
      // the only way we can check that the shim is working is by changing the
      // size and seeing if the shim updates the size of the DOM.
      chrome.test.assertEq(300, webview.offsetWidth);
      chrome.test.assertEq(200, webview.offsetHeight);

      webview.setAttribute('width', 310);
      webview.setAttribute('height', 210);

      // Timeout is necessary to give the mutation observers a chance to fire.
      setTimeout(function() {
        chrome.test.assertEq(310, webview.offsetWidth);
        chrome.test.assertEq(210, webview.offsetHeight);

        // Should also be able to query/update the dimensions via getterts/
        // setters.
        chrome.test.assertEq(310, webview.width);
        chrome.test.assertEq(210, webview.height);

        webview.width = 320;
        webview.height = 220;

        // Setters also end up operating via mutation observers.
        setTimeout(function() {
          chrome.test.assertEq(320, webview.offsetWidth);
          chrome.test.assertEq(220, webview.offsetHeight);

          var dynamicWebViewTag = document.createElement('webview');
          dynamicWebViewTag.setAttribute(
              'src', 'data:text/html,dynamic browser');
          dynamicWebViewTag.setAttribute('width', '330');
          dynamicWebViewTag.setAttribute('height', '230');
          document.body.appendChild(dynamicWebViewTag);

          setTimeout(function() {
            chrome.test.assertEq(330, dynamicWebViewTag.offsetWidth);
            chrome.test.assertEq(230, dynamicWebViewTag.offsetHeight);

            chrome.test.succeed();
          }, 0);
        }, 0);
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
    }
  ]);
};
