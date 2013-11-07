// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var defaultFuzzFactor = 1;

function assertFuzzyEq(expected, actual, fuzzFactor, message) {
  if (!message) {
    message = "Expected: " + expected + "; actual: " + actual + "; "
               + "fuzzyFactor: " + fuzzFactor;
  }

  chrome.test.assertTrue(actual - fuzzFactor <= expected
                         && actual + fuzzFactor >= expected, message);

  if (actual != expected) {
    console.log("FUZZ: a factor of " + Math.abs(actual - expected) +
                "has been used.");
  }
}

function testCreate() {
  chrome.test.runTests([
    function basic() {
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
      }))
    },

    function badWindow() {
      chrome.app.window.create('404.html', callbackPass(function(win) {
        chrome.test.assertTrue(typeof win === 'undefined');
        // TODO(mlamouri): because |win| is not defined, we can not close that
        // window...
      }));
    },

    function loadEvent() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.test.assertEq(document.readyState, 'complete');
          win.contentWindow.close();
        });
      }));
    },

    function multiWindow() {
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

    function contentSize() {
      chrome.app.window.create('test.html',
          { bounds: { width: 250, height: 200 } }, callbackPass(function(win) {
        assertFuzzyEq(250, win.contentWindow.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(200, win.contentWindow.innerHeight, defaultFuzzFactor);
        win.close();
      }));
    },

    function minSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 250, height: 250 },
        minWidth: 400, minHeight: 450
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(400, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(450, w.innerHeight, defaultFuzzFactor);
        w.close();
      }));
    },

    function maxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 250, height: 250 },
        maxWidth: 200, maxHeight: 150
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(200, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(150, w.innerHeight, defaultFuzzFactor);
        w.close();
      }));
    },

    function minAndMaxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 250, height: 250 },
        minWidth: 400, minHeight: 450,
        maxWidth: 200, maxHeight: 150
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(400, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(450, w.innerHeight, defaultFuzzFactor);
        w.close();
      }));
    }
  ]);
}

function testSingleton() {
  chrome.test.runTests([
    function noParameterWithId() {
      chrome.app.window.create(
        'test.html', { id: 'singleton-id' },
        callbackPass(function(win) {
          var w = win.contentWindow;

          chrome.app.window.create(
            'test.html', { id: 'singleton-id' },
            callbackPass(function(win) {
              var w2 = win.contentWindow;
              chrome.test.assertTrue(w === w2);

              w.close();
              w2.close();
            })
          );
        })
      );
    },
  ]);
}

function testBounds() {
  chrome.test.runTests([
    function simpleSetBounds() {
      chrome.app.window.create('test.html',
          { bounds: { width: 250, height: 200 } }, callbackPass(function(win) {
        var b = win.getBounds();
        win.setBounds({width: 400, height: 450})
        // Listen to onresize here rather than win.onBoundsChanged, because
        // onBoundsChanged is fired before the web contents are resized.
        win.contentWindow.onresize = callbackPass(function() {
          assertFuzzyEq(400, win.contentWindow.innerWidth, defaultFuzzFactor);
          assertFuzzyEq(450, win.contentWindow.innerHeight, defaultFuzzFactor);
          win.close();
        });
      }));
    },

    function heightOnlySetBounds() {
      chrome.app.window.create('test.html', {
        bounds: { width: 512, height: 256 }
      }, callbackPass(function(win) {
        win.setBounds({ height: 512 });
        win.contentWindow.onresize = callbackPass(function() {
          assertFuzzyEq(512, win.contentWindow.innerHeight, defaultFuzzFactor);
          win.close();
        });
      }));
    },
  ]);
}

function testCloseEvent() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        win.onClosed.addListener(callbackPass(function() {
          // Mission accomplished.
        }));
        win.contentWindow.close();
      }));
    }
  ]);
}

function testMaximize() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               { bounds: {width: 200, height: 200} },
        callbackPass(function(win) {
          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight > screen.availHeight * 0.8 &&
                   win.contentWindow.outerWidth > screen.availWidth * 0.8;
          }

          function eventLoopCheck(check, done) {
            if (check()) {
              done();
            } else {
              setTimeout(function() { eventLoopCheck(check, done); });
            }
          }

          eventLoopCheck(isWindowMaximized, function() {
            win.close();
          });

          win.maximize();
        })
      );
    }
  ]);
}

function testRestore() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               { bounds: {width: 200, height: 200} },
        callbackPass(function(win) {
          var oldWidth = win.contentWindow.innerWidth;
          var oldHeight = win.contentWindow.innerHeight;

          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight > screen.availHeight * 0.8 &&
                   win.contentWindow.outerWidth > screen.availWidth * 0.8;
          }
          function isWindowRestored() {
            return win.contentWindow.innerHeight == oldHeight &&
                   win.contentWindow.innerWidth == oldWidth;
          }

          function eventLoopCheck(check, done) {
            if (check()) {
              done();
            } else {
              setTimeout(function() { eventLoopCheck(check, done); });
            }
          }

          eventLoopCheck(isWindowMaximized, function() {
            eventLoopCheck(isWindowRestored, function() {
              win.close();
            });

            win.restore();
          });

          win.maximize();
        })
      );
    }
  ]);
}

function testRestoreAfterClose() {
  chrome.test.runTests([
    function restoredBoundsLowerThanNewMineSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 100, height: 150 },
        minWidth: 200, minHeight: 250,
        maxWidth: 200, maxHeight: 250,
        id: 'test-id', singleton: false
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(200, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(250, w.innerHeight, defaultFuzzFactor);

        win.onClosed.addListener(callbackPass(function() {
          chrome.app.window.create('test.html', {
            bounds: { width: 500, height: 550 },
             minWidth: 400, minHeight: 450,
            maxWidth: 600, maxHeight: 650,
            id: 'test-id', singleton: false
          }, callbackPass(function(win) {
            var w = win.contentWindow;
            assertFuzzyEq(400, w.innerWidth, defaultFuzzFactor);
            assertFuzzyEq(450, w.innerHeight, defaultFuzzFactor);
            w.close();
          }));
        }));

        w.close();
      }));
    }
  ]);
}

function testSizeConstraints() {
  chrome.test.runTests([
    function testUndefinedMinAndMaxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 250, height: 250 }
      }, callbackPass(function(win) {
        chrome.test.assertEq(undefined, win.getMinWidth());
        chrome.test.assertEq(undefined, win.getMinHeight());
        chrome.test.assertEq(undefined, win.getMaxWidth());
        chrome.test.assertEq(undefined, win.getMaxHeight());
        win.close();
      }));
    },

    function testSetUndefinedMinAndMaxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        chrome.test.assertEq(100, win.getMinWidth());
        chrome.test.assertEq(101, win.getMinHeight());
        chrome.test.assertEq(104, win.getMaxWidth());
        chrome.test.assertEq(105, win.getMaxHeight());
        win.setMinWidth(null);
        win.setMinHeight(null);
        win.setMaxWidth(null);
        win.setMaxHeight(null);
        win.setBounds({ width: 103, height: 102 });

        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(undefined, win.getMinWidth());
          chrome.test.assertEq(undefined, win.getMinHeight());
          chrome.test.assertEq(undefined, win.getMaxWidth());
          chrome.test.assertEq(undefined, win.getMaxHeight());
          win.close();
        });
      }));
    },

    function testChangingMinAndMaxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        chrome.test.assertEq(100, win.getMinWidth());
        chrome.test.assertEq(101, win.getMinHeight());
        chrome.test.assertEq(104, win.getMaxWidth());
        chrome.test.assertEq(105, win.getMaxHeight());
        win.setMinWidth(98);
        win.setMinHeight(99);
        win.setMaxWidth(106);
        win.setMaxHeight(107);
        win.setBounds({ width: 103, height: 102 });

        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(98, win.getMinWidth());
          chrome.test.assertEq(99, win.getMinHeight());
          chrome.test.assertEq(106, win.getMaxWidth());
          chrome.test.assertEq(107, win.getMaxHeight());
          win.close();
        });
      }));
    },

    function testMinWidthLargerThanMaxWidth() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMinWidth(200);
        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(200, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(200, win.getMaxWidth());
          chrome.test.assertEq(105, win.getMaxHeight());
          win.close();
        });
      }));
    },

    function testMinHeightLargerThanMaxHeight() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMinHeight(200);

        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(200, win.getMinHeight());
          chrome.test.assertEq(104, win.getMaxWidth());
          chrome.test.assertEq(200, win.getMaxHeight());
          win.close();
        });
      }));
    },

    function testMaxWidthSmallerThanMinWidth() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMaxWidth(50);

        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(100, win.getMaxWidth());
          chrome.test.assertEq(105, win.getMaxHeight());
          win.close();
        });
      }));
    },

    function testMaxHeightSmallerThanMinHeight() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMaxHeight(50);

        win.contentWindow.onresize = callbackPass(function() {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(104, win.getMaxWidth());
          chrome.test.assertEq(101, win.getMaxHeight());
          win.close();
        });
      }));
    },
  ]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function(reply) {
    window[reply]();
  });
});
