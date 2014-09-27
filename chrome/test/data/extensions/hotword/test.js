// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send a WAKE_UP command to the hotword helper extension.
chrome.runtime.sendMessage(
    'dnhpdliibojhegemfjheidglijccjfmc',
    {type: 'wu'});

// Assume messages are delivered in-order. In this case, this message will be
// handled after the previous one, and indicates the test case has completed.
chrome.runtime.sendMessage(
    'dnhpdliibojhegemfjheidglijccjfmc',
    {type: 'wu'},
    function() {
      chrome.test.sendMessage("done");
    });
