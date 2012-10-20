// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function browserTag() {
      var browserTag = document.querySelector('browser');
      // Since we can't currently inspect the page loaded inside the <browser>,
      // the only way we can check that the shim is working is by changing the
      // size and seeing if the shim updates the size of the DOM.
      chrome.test.assertEq(300, browserTag.offsetWidth);
      chrome.test.assertEq(200, browserTag.offsetHeight);

      browserTag.setAttribute('width', 310);
      browserTag.setAttribute('height', 210);

      // Timeout is necessary to give the mutation observers a chance to fire.
      setTimeout(function() {
        chrome.test.assertEq(310, browserTag.offsetWidth);
        chrome.test.assertEq(210, browserTag.offsetHeight);

        // Should also be able to query/update the dimensions via getterts/
        // setters.
        chrome.test.assertEq(310, browserTag.width);
        chrome.test.assertEq(210, browserTag.height);

        browserTag.width = 320;
        browserTag.height = 220;

        // Setters also end up operating via mutation observers.
        setTimeout(function() {
          chrome.test.assertEq(320, browserTag.offsetWidth);
          chrome.test.assertEq(220, browserTag.offsetHeight);

          var dynamicBrowserTag = document.createElement('browser');
          dynamicBrowserTag.setAttribute(
              'src', 'data:text/html,dynamic browser');
          dynamicBrowserTag.setAttribute('width', '330');
          dynamicBrowserTag.setAttribute('height', '230');
          document.body.appendChild(dynamicBrowserTag);

          setTimeout(function() {
            chrome.test.assertEq(330, dynamicBrowserTag.offsetWidth);
            chrome.test.assertEq(230, dynamicBrowserTag.offsetHeight);

            chrome.test.succeed();
          }, 0);
        }, 0);
      }, 0);
    },

    function browserTagApiMethodExistence() {
      var browserTag = document.createElement('browser');
      browserTag.setAttribute('src', 'data:text/html,browser tag check api');
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
      document.body.appendChild(browserTag);

      // Timeout is necessary to give the mutation observers of the browser tag
      // shim a chance to fire.
      setTimeout(function() {
        for (var i = 0; i < apiMethodsToCheck.length; ++i) {
          chrome.test.assertEq('function',
                               typeof browserTag[apiMethodsToCheck[i]]);
        }

        // Check contentWindow.
        chrome.test.assertEq('object', typeof browserTag.contentWindow);
        chrome.test.assertEq('function',
                             typeof browserTag.contentWindow.postMessage);

        chrome.test.succeed();
      }, 0);
    },

    function browserTagEventListeners() {
      var browserTag = document.createElement('browser');
      browserTag.setAttribute('src', 'data:text/html,browser tag check api');
      document.body.appendChild(browserTag);

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

      // Timeout is necessary to give the mutation observers of the browser tag
      // shim a chance to fire.
      setTimeout(function() {
        for (var i = 0; i < validEvents.length; ++i) {
          chrome.test.assertTrue(
              browserTag.addEventListener(validEvents[i], function() {}));
        }

        for (var i = 0; i < invalidEvents.length; ++i) {
          chrome.test.assertFalse(
              browserTag.addEventListener(invalidEvents[i], function() {}));
        }

        chrome.test.succeed();
      }, 0);
    },

    function browserTagEventName() {
      var browserTag = document.createElement('browser');
      browserTag.setAttribute('src', 'data:text/html,browser tag check api');
      document.body.appendChild(browserTag);

      setTimeout(function() {
        browserTag.addEventListener('loadstart', function(evt) {
          chrome.test.assertEq('loadstart', evt.name);
        });

        browserTag.addEventListener('loadstop', function(evt) {
          chrome.test.assertEq('loadstop', evt.name);
          browserTag.terminate();
        });

        browserTag.addEventListener('exit', function(evt) {
          chrome.test.assertEq('exit', evt.name);
          chrome.test.succeed();
        });

        browserTag.setAttribute('src', 'data:text/html,trigger navigation');
      }, 0);
    }
  ]);
};
