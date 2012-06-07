// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

chrome.experimental.app.onLaunched.addListener(function() {
  chrome.test.runTests([
   function testCreateWindow() {
     chrome.appWindow.create('test.html', {}, callbackPass(function (win) {
       chrome.test.assertTrue(typeof win.window === 'object');
     }))
   },

   function testUpdateWindowWidth() {
     chrome.appWindow.create('test.html',
         {width:512, height:384, frame:'custom'},
         callbackPass(function(win) {
           // TODO(jeremya): assert that width and height specified in options
           // above specify the innerWidth, not the outerWidth.
           //chrome.test.assertEq(512, win.outerWidth);
           //chrome.test.assertEq(384, win.outerHeight);
           var oldWidth = win.outerWidth, oldHeight = win.outerHeight;
           win.resizeBy(-256, 0);
           chrome.test.assertEq(oldWidth - 256, win.outerWidth);
           chrome.test.assertEq(oldHeight, win.outerHeight);
         }));
   },
  ]);
});
