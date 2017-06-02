// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var panelWindow;
var nonShelfWindow;
var shelfWindow;

function processNextCommand() {
  chrome.test.sendMessage('ready', function(response) {
    if (response == 'Exit') {
      return;
    }
    if (response == 'createPanelWindow') {
      chrome.app.window.create('main.html', { type: 'panel' }, function (win) {
         panelWindow = win;
      });
    }
    if (response == 'setPanelWindowIcon') {
      panelWindow.setIcon('icon64.png')
    }
    if (response == 'createNonShelfWindow') {
      // Create the shell window; it should use the app icon, and not affect
      // the panel icon.
      chrome.app.window.create(
        'main.html', { id: 'win',
                       type: 'shell' },
        function (win) {
            nonShelfWindow = win;
        });
    }
    if (response == 'createShelfWindow') {
      // Create the shell window which is shown in shelf; it should use
      // another custom app icon.
      chrome.app.window.create(
        'main.html', { id: 'win_with_icon',
                       type: 'shell',
                       showInShelf: true },
        function (win) {
          shelfWindow = win;
        });
    }
    if (response == 'setShelfWindowIcon') {
      shelfWindow.setIcon('icon32.png')
    }
    processNextCommand();
  });
};


chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched');
  processNextCommand();
});

