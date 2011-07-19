// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function () {
  function hideAllPages() {
    var pages = ['cloudprintsetup', 'setupdone'];
    for (var i = 0; i < pages.length; ++i) {
      $(pages[i]).style.display = 'none';
      $(pages[i]).tabIndex = -1;
    }
  }

  function showPage(page) {
    hideAllPages();
    $(page).style.display = 'block';
    $(page).tabIndex = 0;
  }

  function showInitialPage() {
    var args = JSON.parse(chrome.dialogArguments);
    showPage(args.pageToShow);
  }

  function showSetupLogin() {
    showPage('cloudprintsetup');
  }

  function showSetupDone(width, height) {
    hideAllPages();
    var moveByX = (window.innerWidth - width) / 2;
    var moveByY = (window.innerHeight - height) / 2;
    var sizeByX = width - window.innerWidth;
    var sizeByY = height - window.innerHeight;
    window.moveBy(moveByX, moveByY);
    window.resizeBy(sizeByX, sizeByY);
    showPage('setupdone');
  }

  return {
    hideAllPages: hideAllPages,
    showPage: showPage,
    showInitialPage: showInitialPage,
    showSetupLogin: showSetupLogin,
    showSetupDone: showSetupDone
  };
});
