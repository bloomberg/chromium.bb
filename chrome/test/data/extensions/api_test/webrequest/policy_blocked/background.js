// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Return messages to tell which URL requests are visible to the extension.
chrome.webRequest.onBeforeRequest.addListener(function(details) {
   if (details.url.indexOf('example2') != -1) {
     chrome.test.sendMessage('protected_origin');
   }
   if (details.url.indexOf('protected_url') != -1) {
     chrome.test.sendMessage('protected_url');
   }
}, {urls: ['<all_urls>']}, []);
