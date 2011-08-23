// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that the 'p' element has had CSS injected, and that the CSS code
// has had the __MSG_text_color__ message replaced ('text_color' must
// not be present in any CSS code).

var rules = getMatchedCSSRules(document.getElementById('pId'));
if (rules != null) {
  for (var i = 0; i < rules.length; ++i) {
    if (rules.item(i).cssText.indexOf('text_color') != -1) {
      chrome.extension.sendRequest({tag: 'paragraph_style', message:
          'Found unreplaced test_color in: ' + rules.item(i).cssText});
      break;
    }
  }
  chrome.extension.sendRequest({tag: 'paragraph_style', message: 'passed'});
} else {
  chrome.extension.sendRequest({tag: 'paragraph_style', message:
      'No CSS rules found.'});
}

