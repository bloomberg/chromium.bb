// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.title = chrome.i18n.getMessage('CHAT_MANAGER_NAME');

function reparentCentralRosterToWindow() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  if (backgroundWindow) {
    var backgroundDocument = backgroundWindow.document;
    var iframe = backgroundDocument.getElementById('centralRoster');
    if (iframe) {
      if ((new Date().getTime() - localStorage.rosterClosed) < 500) {
        // If the 'unload' is followed by 'load' in less than 0.5 seconds
        // we assume it is a page refresh and the user wants to restart the app.
        iframe.parentNode.removeChild(iframe);
        iframe = backgroundDocument.createElement('iframe');
        iframe.id ='centralRoster';
        iframe.src ='central_roster.html';
      }
      if (document.adoptNode) {
        document.adoptNode(iframe);
      }
      document.body.appendChild(iframe);

      if (!localStorage.hasOpenedInViewer) {
        // This is the first time the user started the app, so we'll remember
        // the event, check if the centralRoster frame has already been loaded
        // and start the app and that case.
        localStorage.hasOpenedInViewer = true;
        if (iframe.contentWindow.runGTalkScript) {
          iframe.contentWindow.runGTalkScript();
        }
      }
    } else {
      // If there in no centralRoster frame in background page, this must be
      // a second panel trying to load the central roster.
      window.close();
    }
  }
}

function reparentCentralRosterToBackground(event) {
  // We'll remember when was the last time the central roster panel closes
  // in order to detect if it is a close on reload action.
  localStorage.rosterClosed = new Date().getTime();

  if (document) {
    var iframe = document.getElementById('centralRoster');
    if (iframe) {
      var backgroundWindow = chrome.extension.getBackgroundPage();
      if (backgroundWindow) {
        var backgroundDocument = backgroundWindow.document;
        if (backgroundDocument.adoptNode) {
          backgroundDocument.adoptNode(iframe);
        }
        backgroundDocument.body.appendChild(iframe);
      }
    }
  }
}
