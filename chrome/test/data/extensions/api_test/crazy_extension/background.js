// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is for bizarre things that extensions probably will do. We just
// need to ensure that behaviour is predictable.

chrome.test.runTests([
  function accessNonexistentIframe() {
    var iframe = document.createElement("iframe");
    iframe.src = document.location;
    var iteration = 0;
    iframe.addEventListener('load', function() {
      switch (iteration) {
        case 0: {
          // First test is that chrome.app doesn't even get defined if it was
          // never accessed.
          var iframeChrome = iframe.contentWindow.chrome;
          document.body.removeChild(iframe);
          chrome.test.assertEq(undefined, iframeChrome.app);
          document.body.appendChild(iframe);
          break;
        }
        case 1: {
          // Second test is that it does if accessed before removal.
          var iframeChrome = iframe.contentWindow.chrome;
          var iframeChromeApp = iframeChrome.app;
          chrome.test.assertTrue(typeof(iframeChromeApp) == 'object');
          document.body.removeChild(iframe);
          chrome.test.assertTrue(typeof(iframeChrome.app) == 'object');
          document.body.appendChild(iframe);
          break;
        }
        case 2: {
          // Third test is that accessing API methods doesn't crash the
          // renderer if the frame doesn't exist anymore.
          var iframeChromeApp = iframe.contentWindow.chrome.app;
          document.body.removeChild(iframe);
          chrome.test.assertEq(undefined, iframeChromeApp.getDetails());
          chrome.test.succeed();
          break;
        }
      }
      iteration++;
    });
    document.body.appendChild(iframe);
  }
]);
