// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var PERMISSIONS = {origins: ['http://api.stackoverflow.com/']};
var YES = 'ENABLED';
var NO = 'DISABLED';

var $status = document.querySelector('#status');
chrome.experimental.permissions.onAdded.addListener(function(permissions) {
  $status.innerText = YES;
});
chrome.experimental.permissions.onRemoved.addListener(function(permissions) {
  $status.innerText = NO;
});
chrome.experimental.permissions.contains(PERMISSIONS, function(contains) {
  $status.innerText = contains ? YES : NO;
});

document.querySelector('button#enable').addEventListener('click', function() {
  chrome.experimental.permissions.contains(PERMISSIONS, function(allowed) {
    if (allowed) {
      alert('You already have SO host permission!');
    } else {
      chrome.experimental.permissions.request(PERMISSIONS, function(result) {
        if (result) {
          console.log('SO host permission granted!' +
                      'Open the browser action again.');
        }
      });
    }
  });
});

document.querySelector('button#disable').addEventListener('click', function() {
  chrome.experimental.permissions.contains(PERMISSIONS, function(allowed) {
    if (allowed) {
      chrome.experimental.permissions.remove(PERMISSIONS, function(result) {
        console.log('Revoked SO host permission.');
      });
    } else {
      alert('No SO host permission found.');
    }
  });
});

