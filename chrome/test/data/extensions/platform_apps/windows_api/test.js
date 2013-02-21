// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
   function testCreateWindow() {
     chrome.app.window.create('test.html',
                              {id: 'testId'},
                              callbackPass(function(win) {
       chrome.test.assertEq(typeof win.contentWindow.window, 'object');
       chrome.test.assertTrue(
         typeof win.contentWindow.document !== 'undefined');
       chrome.test.assertFalse(
         'about:blank' === win.contentWindow.location.href);
       var cw = win.contentWindow.chrome.app.window.current();
       chrome.test.assertEq(cw, win);
       chrome.test.assertEq('testId', cw.id);
       win.contentWindow.close();
     }));
   },

   function testCreateBadWindow() {
     chrome.app.window.create('404.html', callbackPass(function(win) {
       chrome.test.assertTrue(typeof win === 'undefined');
     }));
   },

   function testOnLoad() {
     chrome.app.window.create('test.html', callbackPass(function(win) {
       win.contentWindow.onload = callbackPass(function() {
         chrome.test.assertEq(document.readyState, 'complete');
         win.contentWindow.close();
       });
     }));
   },

   function testCreateMultiWindow() {
     chrome.test.assertTrue(null === chrome.app.window.current());
     chrome.app.window.create('test.html',
                              {id: 'testId1'},
                              callbackPass(function(win1) {
       chrome.app.window.create('test.html',
                                {id: 'testId2'},
                                callbackPass(function(win2) {
         var cw1 = win1.contentWindow.chrome.app.window.current();
         var cw2 = win2.contentWindow.chrome.app.window.current();
         chrome.test.assertEq('testId1', cw1.id);
         chrome.test.assertEq('testId2', cw2.id);
         chrome.test.assertTrue(cw1 === win1);
         chrome.test.assertTrue(cw2 === win2);
         chrome.test.assertFalse(cw1 === cw2);
         win1.contentWindow.close();
         win2.contentWindow.close();
       }));
     }));
   },

   function testCreateWindowContentSize() {
     chrome.app.window.create('test.html',
         { bounds: { width: 250, height: 200 } }, callbackPass(function(win) {
       chrome.test.assertEq(250, win.contentWindow.innerWidth);
       chrome.test.assertEq(200, win.contentWindow.innerHeight);
       win.close();
     }));
   },

   function testSetBoundsContentSize() {
     chrome.app.window.create('test.html',
         { bounds: { width: 250, height: 200 } }, callbackPass(function(win) {
       var b = win.getBounds();
       win.setBounds({width: 400, height: 450})
       // Listen to onresize here rather than win.onBoundsChanged, because
       // onBoundsChanged is fired before the web contents are resized.
       win.contentWindow.onresize = callbackPass(function() {
         win.contentWindow.onresize = undefined;
         chrome.test.assertEq(400, win.contentWindow.innerWidth);
         chrome.test.assertEq(450, win.contentWindow.innerHeight);
         win.close();
       });
     }));
   },

   function testOnClosedEvent() {
     chrome.app.window.create('test.html', callbackPass(function(win) {
       win.onClosed.addListener(callbackPass(function() {
         // Mission accomplished.
       }));
       win.contentWindow.close();
     }));
   },

   function testMinSize() {
     chrome.app.window.create('test.html', {
       bounds: { width: 250, height: 250 },
       minWidth: 400, minHeight: 450
     }, callbackPass(function(win) {
       var w = win.contentWindow;
       chrome.test.assertEq(400, w.innerWidth);
       chrome.test.assertEq(450, w.innerHeight);
       w.close();
     }));
   },

   function testMaxSize() {
     chrome.app.window.create('test.html', {
       bounds: { width: 250, height: 250 },
       maxWidth: 200, maxHeight: 150
     }, callbackPass(function(win) {
       var w = win.contentWindow;
       chrome.test.assertEq(200, w.innerWidth);
       chrome.test.assertEq(150, w.innerHeight);
       w.close();
     }));
   },

   function testMinAndMaxSize() {
     chrome.app.window.create('test.html', {
       bounds: { width: 250, height: 250 },
       minWidth: 400, minHeight: 450,
       maxWidth: 200, maxHeight: 150
     }, callbackPass(function(win) {
       var w = win.contentWindow;
       chrome.test.assertEq(400, w.innerWidth);
       chrome.test.assertEq(450, w.innerHeight);
       w.close();
     }));
   },

   function testMinSizeRestore() {
     chrome.app.window.create('test.html', {
       bounds: { width: 250, height: 250 },
       minWidth: 400, minHeight: 450,
       id: 'test-id', singleton: false
     }, callbackPass(function(win) {
       var w = win.contentWindow;
       chrome.test.assertEq(400, w.innerWidth);
       chrome.test.assertEq(450, w.innerHeight);
       w.close();

       chrome.app.window.create('test.html', {
         bounds: { width: 250, height: 250 },
         minWidth: 500, minHeight: 550,
         id: 'test-id', singleton: false
       }, callbackPass(function(win) {
         var w = win.contentWindow;
         chrome.test.assertEq(500, w.innerWidth);
         chrome.test.assertEq(550, w.innerHeight);
         w.close();
       }));
     }));
   },

   function testSingleton() {
     chrome.app.window.create('test.html', {
       id: 'singleton-id'
     }, callbackPass(function(win) {
       var w = win.contentWindow;

       chrome.app.window.create('test.html', {
         id: 'singleton-id'
       }, callbackPass(function(win) {
         var w2 = win.contentWindow;

         chrome.test.assertTrue(w === w2);

         chrome.app.window.create('test.html', {
           id: 'singleton-id', singleton: false
         }, callbackPass(function(win) {
           var w3 = win.contentWindow;

           chrome.test.assertFalse(w === w3);

           w.close();
           w2.close();
           w3.close();
         }));
       }));
     }));
   },

   /*function testMaximize() {
     chrome.app.window.create('test.html', {width: 200, height: 200},
         callbackPass(function(win) {
           win.onresize = callbackPass(function(e) {
             // Crude test to check we're somewhat maximized.
             chrome.test.assertTrue(
                 win.outerHeight > screen.availHeight * 0.8);
             chrome.test.assertTrue(
                 win.outerWidth > screen.availWidth * 0.8);
           });
           win.chrome.app.window.maximize();
         }));
   },*/

   /*function testRestore() {
     chrome.app.window.create('test.html', {width: 200, height: 200},
         callbackPass(function(win) {
           var oldWidth = win.innerWidth;
           var oldHeight = win.innerHeight;
           win.onresize = callbackPass(function() {
             chrome.test.assertTrue(win.innerWidth != oldWidth);
             chrome.test.assertTrue(win.innerHeight != oldHeight);
             // Seems like every time we resize, we get two resize events.
             // See http://crbug.com/133869.
             win.onresize = callbackPass(function() {
               // Ignore the immediately following resize, as it's a clone of
               // the one we just got.
               win.onresize = callbackPass(function() {
                 chrome.test.assertEq(oldWidth, win.innerWidth);
                 chrome.test.assertEq(oldHeight, win.innerHeight);
               });
             })
             win.chrome.app.window.restore();
           });
           win.chrome.app.window.maximize();
         }));
   },*/
  ]);
});
