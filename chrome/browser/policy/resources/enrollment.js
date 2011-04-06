// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('enterpriseEnrollment', function() {

  function showScreen(screen) {
    var screens = ['login-screen', 'confirmation-screen'];
    for (var i = 0; i < screens.length; i++) {
      $(screens[i]).hidden = screens[i] != screen;

      // Hiding an iframe unfortunately doesn't remove it or its contents from
      // tabbing order. To hack around this:
      // - Hide the content document (if it exists), so nested elements won't
      //   receive focus and relinquish focus if they already have it.
      // - Set tabIndex = -1 on the iframe, so it doesn't get focused itself.
      //
      // See https://bugs.webkit.org/show_bug.cgi?id=55861
      iframes = $(screens[i]).getElementsByTagName('iframe');
      for (var j = 0; j < iframes.length; j++) {
        var display = '';
        if (screens[i] != screen) {
          display = 'none';
          iframes[j].tabIndex = -1;
        } else {
          display = 'block';
          iframes[j].removeAttribute('tabIndex');
        }
        if (iframes[j].contentDocument && iframes[j].contentDocument.body) {
          iframes[j].contentDocument.body.style.display = display;
        }
      }
    }
  }

  function showInitialScreen() {
    var args = JSON.parse(chrome.dialogArguments);
    showScreen(args.initialScreen);
  }

  return {
    showScreen: showScreen,
    showInitialScreen: showInitialScreen
  };
});

document.addEventListener('DOMContentLoaded',
                          enterpriseEnrollment.showInitialScreen);
