// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testActivate() {
    var focused = false;
    var activated = false;
    chrome.input.ime.onFocus.addListener(function(context) {
      if (context.type == 'none') {
        chrome.test.fail();
        return;
      }
      focused = true;
      chrome.test.sendMessage('get_focus_event');
      if (activated)
        chrome.test.succeed();
    });
    chrome.input.ime.activate(function() {
      if (chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      activated = true;
      if (focused)
        chrome.test.succeed();
    });
  },
  function testNormalCreateWindow() {
    var options = { windowType: 'normal' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      // Test for security origin.
      // If security origin is not correctly set, there will be securtiy
      // exceptions when accessing DOM or add event listeners.
      win.addEventListener('unload', function() {});
      chrome.test.succeed();
    });
  },
  function testFollowCursorCreateWindow() {
    var options = { windowType: 'followCursor' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      // test for security origin.
      // If security origin is not correctly set, there will be securtiy
      // exceptions when accessing DOM or add event listeners.
      win.addEventListener('unload', function() {});
      chrome.test.succeed();
    });
  },
  function testBlur() {
     chrome.input.ime.onBlur.addListener(function(context) {
      if (context.type == 'none') {
        chrome.test.fail();
        return;
      }
      chrome.test.sendMessage('get_blur_event');
      chrome.test.succeed();
     });
     chrome.test.succeed();
  },
  function testSendKeyEvents() {
    chrome.input.ime.sendKeyEvents({
      'contextID': 1,
      'keyData': [{
        'type': 'keydown',
        'requestId': '0',
        'key': 'a',
        'code': 'KeyA'
      }, {
        'type': 'keyup',
        'requestId': '1',
        'key': 'a',
        'code': 'KeyA'
     }]
    });
    chrome.test.succeed();
  },
  function testCommitText() {
    chrome.input.ime.commitText({
      contextID: 1,
      text: 'test_commit_text'
    }, function () {
      if (chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      chrome.test.succeed();
    });
  },
  function testComposition() {
    chrome.input.ime.onCompositionBoundsChanged.addListener(
    function(boundsList) {
      if (boundsList.length > 0 && boundsList[0].width > 1 ){
        for (var i = 0; i < boundsList.length; i++ ) {
          if (i==0) {
            composition_left = boundsList[i].left;
            composition_top = boundsList[i].top;
            composition_width = boundsList[i].width;
            composition_height = boundsList[i].height;
          }
          else {
            chrome.test.assertEq(boundsList[i].top, composition_top);
            chrome.test.assertEq(boundsList[i].height, composition_height);
             chrome.test.assertEq(boundsList[i].left,
              composition_left + composition_width);
            composition_left = boundsList[i].left;
            composition_width = boundsList[i].width;
          }
        }
      }
      if (boundsList.length > 20) {
        chrome.test.sendMessage('finish_composition_test');
      }
      chrome.test.succeed();
    });
    chrome.input.ime.setComposition({
      contextID: 1,
      text: 'test_composition_text',
      cursor: 2
    }, function() {
      if(chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      chrome.test.succeed();
    });
   }
]);
