// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Store settings in the synchronized repository.
var storage = chrome.experimental.storage.sync;
var message = document.querySelector('#message');

// Check if there is CSS specified.
storage.get('css', function(items) {
  console.log(items);
  // If there is CSS specified, inject it into the page.
  if (items.css) {
    chrome.tabs.insertCSS(null, {code: items.css}, function() {
      if (chrome.extension.lastError) {
        message.innerText = 'Not allowed to inject CSS into special page.';
      } else {
        message.innerText = 'Injected style!';
      }
    });
  } else {
    var optionsUrl = chrome.extension.getURL('options.html');
    message.innerHTML = 'Set a style in the <a target="_blank" href="' +
        optionsUrl + '">options page</a> first.';
  }
});

