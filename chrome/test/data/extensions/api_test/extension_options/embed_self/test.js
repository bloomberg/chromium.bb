// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests if the <extensionoptions> guest view is successfully created. This
  // test expects that the guest options page will add a {'pass': true} property
  // to every Window and fire the runtime.onMessage event with a short message.
  function canCreateExtensionOptionsGuest() {
    var done = chrome.test.listenForever(chrome.runtime.onMessage,
        function(message, sender, sendResponse) {
      if (message == 'ready') {
        sendResponse('canCreateExtensionOptionsGuest');
      } else if (message == 'done') {
        var views = chrome.extension.getViews();
        chrome.test.assertEq(2, views.length);
        views.forEach(function(view) {
          chrome.test.assertTrue(view.pass);
        });
        chrome.test.assertEq(chrome.runtime.id, sender.id);
        document.body.removeChild(extensionoptions);
        done();
      }
    });

    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  },

  // Tests if the <extensionoptions> guest view can access the chrome.storage
  // API, a privileged extension API.
  function guestCanAccessStorage() {
    var onStorageChanged = false;
    var onSetAndGet = false;

    chrome.test.listenOnce(chrome.storage.onChanged, function(change) {
      chrome.test.assertEq(42, change.test.newValue);
    });

    // Listens for messages from the options page.
    var done = chrome.test.listenForever(chrome.runtime.onMessage,
        function(message, sender, sendResponse) {
      // Options page is waiting for a command
      if (message == 'ready') {
        sendResponse('guestCanAccessStorage');
      // Messages from the options page containing test data
      } else if (message.description == 'onStorageChanged') {
        chrome.test.assertEq(message.expected, message.actual);
        onStorageChanged = true;
      } else if (message.description == 'onSetAndGet') {
        chrome.test.assertEq(message.expected, message.actual);
        onSetAndGet = true;
      }

      if (onStorageChanged && onSetAndGet) {
        document.body.removeChild(extensionoptions);
        done();
      }
    });

    var extensionoptions = document.createElement('extensionoptions');
    extensionoptions.setAttribute('extension', chrome.runtime.id);
    document.body.appendChild(extensionoptions);
  }
]);
