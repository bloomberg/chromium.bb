// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
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

// This helper will verify that |check| returns true. If it does not, it will do
// a trip to the event loop and will try again until |check| returns true. At
// which points |callback| will be called.
// NOTE: if the test fails, it will timeout.
function eventLoopCheck(check, callback) {
  if (check()) {
    callback();
  } else {
    setTimeout(callbackPass(function() { eventLoopCheck(check, callback); }));
  }
}

// This help function will call the callback when the window passed to it will
// be loaded. The callback will have the AppWindow passed as a parameter.
function waitForLoad(win, callback) {
  var window = win.contentWindow;

  if (window.document.readyState == 'complete') {
    callback(win);
    return;
  }

  window.addEventListener('load', callbackPass(function() {
    window.removeEventListener('load', arguments.callee);
    callback(win);
  }));
}

function assertConstraintsUnspecified(win) {
  chrome.test.assertEq(null, win.innerBounds.minWidth);
  chrome.test.assertEq(null, win.innerBounds.minHeight);
  chrome.test.assertEq(null, win.innerBounds.maxWidth);
  chrome.test.assertEq(null, win.innerBounds.maxHeight);
  chrome.test.assertEq(null, win.outerBounds.minWidth);
  chrome.test.assertEq(null, win.outerBounds.minHeight);
  chrome.test.assertEq(null, win.outerBounds.maxWidth);
  chrome.test.assertEq(null, win.outerBounds.maxHeight);
}

function assertBoundsConsistent(win) {
  // Ensure that the inner and outer bounds are consistent. Since platforms
  // have different frame padding, we cannot check the sizes precisely.
  // It is a reasonable assumption that all platforms will have a title bar at
  // the top of the window.
  chrome.test.assertTrue(win.innerBounds.left >= win.outerBounds.left);
  chrome.test.assertTrue(win.innerBounds.top > win.outerBounds.top);
  chrome.test.assertTrue(win.innerBounds.width <= win.outerBounds.width);
  chrome.test.assertTrue(win.innerBounds.height < win.outerBounds.height);

  if (win.innerBounds.minWidth === null)
    chrome.test.assertEq(null, win.outerBounds.minWidth);
  else
    chrome.test.assertTrue(
        win.innerBounds.minWidth <= win.outerBounds.minWidth);

  if (win.innerBounds.minHeight === null)
    chrome.test.assertEq(null, win.outerBounds.minHeight);
  else
    chrome.test.assertTrue(
        win.innerBounds.minHeight < win.outerBounds.minHeight);

  if (win.innerBounds.maxWidth === null)
    chrome.test.assertEq(null, win.outerBounds.maxWidth);
  else
    chrome.test.assertTrue(
        win.innerBounds.maxWidth <= win.outerBounds.maxWidth);

  if (win.innerBounds.maxHeight === null)
    chrome.test.assertEq(null, win.outerBounds.maxHeight);
  else
    chrome.test.assertTrue(
        win.innerBounds.maxHeight < win.outerBounds.maxHeight);
}

function testConflictingBoundsProperty(propertyName) {
  var innerBounds = {};
  var outerBounds = {};
  innerBounds[propertyName] = 20;
  outerBounds[propertyName] = 20;
  chrome.app.window.create('test.html', {
    innerBounds: innerBounds,
    outerBounds: outerBounds
  }, callbackFail('The ' + propertyName + ' property cannot be specified for ' +
                  'both inner and outer bounds.')
  );
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
    }
  ]);
}

function testDeprecatedBounds() {
  chrome.test.runTests([
    function contentSize() {
      var options = { bounds: { width: 250, height: 200 } };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.bounds.width, bounds.width);
        chrome.test.assertEq(options.bounds.height, bounds.height);
        chrome.test.assertEq(options.bounds.width, win.innerBounds.width);
        chrome.test.assertEq(options.bounds.height, win.innerBounds.height);
        win.close();
      }));
    },

    function windowPosition() {
      var options = { bounds: { left: 250, top: 200 } };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.bounds.left, bounds.left);
        chrome.test.assertEq(options.bounds.top, bounds.top);
        chrome.test.assertEq(options.bounds.left, win.outerBounds.left);
        chrome.test.assertEq(options.bounds.top, win.outerBounds.top);
        win.close();
      }));
    },

    function minSize() {
      var options = {
        bounds: { width: 250, height: 250 },
        minWidth: 400, minHeight: 450
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.minWidth, bounds.width);
        chrome.test.assertEq(options.minHeight, bounds.height);
        win.close();
      }));
    },

    function maxSize() {
      var options = {
        bounds: { width: 250, height: 250 },
        maxWidth: 200, maxHeight: 150
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.maxWidth, bounds.width);
        chrome.test.assertEq(options.maxHeight, bounds.height);
        win.close();
      }));
    },

    function minAndMaxSize() {
      var options = {
        bounds: { width: 250, height: 250 },
        minWidth: 400, minHeight: 450,
        maxWidth: 200, maxHeight: 150
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.minWidth, bounds.width);
        chrome.test.assertEq(options.minHeight, bounds.height);
        win.close();
      }));
    },

    function simpleSetBounds() {
      chrome.app.window.create('test.html',
          { bounds: { width: 250, height: 200 } }, callbackPass(function(win) {
        var newBounds = {width: 400, height: 450};
        win.setBounds(newBounds);
        chrome.test.waitForRoundTrip('msg', callbackPass(function() {
          var bounds = win.getBounds();
          chrome.test.assertEq(newBounds.width, bounds.width);
          chrome.test.assertEq(newBounds.height, bounds.height);
          win.close();
        }));
      }));
    },

    function heightOnlySetBounds() {
      chrome.app.window.create('test.html', {
        bounds: { width: 512, height: 256 }
      }, callbackPass(function(win) {
        win.setBounds({ height: 512 });
        chrome.test.waitForRoundTrip('msg', callbackPass(function() {
          var bounds = win.getBounds();
          chrome.test.assertEq(512, bounds.width);
          chrome.test.assertEq(512, bounds.height);
          win.close();
        }));
      }));
    },
  ]);
}

function testInitialBounds() {
  chrome.test.runTests([
    function testNoOptions() {
      chrome.app.window.create('test.html', {
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertTrue(win.innerBounds.width > 0);
        chrome.test.assertTrue(win.innerBounds.height > 0);
        chrome.test.assertTrue(win.outerBounds.width > 0);
        chrome.test.assertTrue(win.outerBounds.height > 0);
        assertConstraintsUnspecified(win);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testInnerBoundsOnly() {
      var innerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.left, win.innerBounds.left);
        chrome.test.assertEq(innerBounds.top, win.innerBounds.top);
        chrome.test.assertEq(innerBounds.width, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.height, win.innerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testOuterBoundsOnly() {
      var outerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.height, win.outerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testFrameless() {
      var outerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds,
        frame: 'none'
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.height, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.left, win.innerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.innerBounds.top);
        chrome.test.assertEq(outerBounds.width, win.innerBounds.width);
        chrome.test.assertEq(outerBounds.height, win.innerBounds.height);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testInnerSizeAndOuterPos() {
      var innerBounds = {
        width: 400,
        height: 300
      };
      var outerBounds = {
        left: 150,
        top: 100
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(innerBounds.width, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.height, win.innerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testInnerAndOuterBoundsEdgeCase() {
      var innerBounds = {
        left: 150,
        height: 300
      };
      var outerBounds = {
        width: 400,
        top: 100
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.left, win.innerBounds.left);
        chrome.test.assertEq(innerBounds.height, win.innerBounds.height);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testPositionOnly() {
      var outerBounds = {
        left: 150,
        top: 100
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertTrue(win.innerBounds.width > 0);
        chrome.test.assertTrue(win.innerBounds.height > 0);
        chrome.test.assertTrue(win.outerBounds.width > 0);
        chrome.test.assertTrue(win.outerBounds.height > 0);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testSizeOnly() {
      var outerBounds = {
        width: 500,
        height: 400
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.height, win.outerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testConflictingProperties() {
      testConflictingBoundsProperty("width");
      testConflictingBoundsProperty("height");
      testConflictingBoundsProperty("left");
      testConflictingBoundsProperty("top");
      testConflictingBoundsProperty("minWidth");
      testConflictingBoundsProperty("minHeight");
      testConflictingBoundsProperty("maxWidth");
      testConflictingBoundsProperty("maxHeight");
    }
  ]);
}

function testInitialBoundsInStable() {
  chrome.test.runTests([
    function testInnerBounds() {
      var innerBounds = {
        width: 600,
        height: 400
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackFail('innerBounds and outerBounds are only available in'+
                      ' dev channel.')
      );
    },

    function testOuterBounds() {
      var outerBounds = {
        width: 600,
        height: 400
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackFail('innerBounds and outerBounds are only available in'+
                      ' dev channel.')
      );
    }
  ]);
}

function testInitialConstraints() {
  chrome.test.runTests([
    function testMaxInnerConstraints() {
      var innerBounds = {
        width: 800,
        height: 600,
        maxWidth: 500,
        maxHeight: 400
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.maxWidth, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.maxHeight, win.innerBounds.height);
        chrome.test.assertEq(innerBounds.maxWidth, win.innerBounds.maxWidth);
        chrome.test.assertEq(innerBounds.maxHeight, win.innerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMinInnerConstraints() {
      var innerBounds = {
        width: 100,
        height: 100,
        minWidth: 300,
        minHeight: 200
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.minWidth, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.height);
        chrome.test.assertEq(innerBounds.minWidth, win.innerBounds.minWidth);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMaxOuterConstraints() {
      var outerBounds = {
        width: 800,
        height: 600,
        maxWidth: 500,
        maxHeight: 400
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.maxWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.maxHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.maxWidth, win.outerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.maxHeight, win.outerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMinOuterConstraints() {
      var outerBounds = {
        width: 100,
        height: 100,
        minWidth: 300,
        minHeight: 200
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMixedConstraints() {
      var innerBounds = {
        width: 100,
        minHeight: 300
      };
      var outerBounds = {
        height: 100,
        minWidth: 400,
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testBadConstraints() {
      var outerBounds = {
        width: 500,
        height: 400,
        minWidth: 800,
        minHeight: 700,
        maxWidth: 300,
        maxHeight: 200
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.minHeight);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testFrameless() {
      var outerBounds = {
        minWidth: 50,
        minHeight: 50,
        maxWidth: 800,
        maxHeight: 800
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds,
        frame: 'none'
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.minHeight);
        chrome.test.assertEq(outerBounds.maxWidth, win.outerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.maxHeight, win.outerBounds.maxHeight);
        chrome.test.assertEq(outerBounds.minWidth, win.innerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.innerBounds.minHeight);
        chrome.test.assertEq(outerBounds.maxWidth, win.innerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.maxHeight, win.innerBounds.maxHeight);
        win.close();
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
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }

          eventLoopCheck(isWindowMaximized, function() {
            win.close();
          });

          win.maximize();
        })
      );
    },

    function nonResizableWindow() {
      chrome.app.window.create('test.html',
                               { bounds: {width: 200, height: 200},
                                 resizable: false },
        callbackPass(function(win) {
          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }

          eventLoopCheck(isWindowMaximized, function() {
            win.close();
          });

          win.maximize();
        })
      );
    },
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
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }
          function isWindowRestored() {
            return win.contentWindow.innerHeight == oldHeight &&
                   win.contentWindow.innerWidth == oldWidth;
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
    function restoredBoundsLowerThanNewMinSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 100, height: 150 },
        minWidth: 200, minHeight: 250,
        maxWidth: 200, maxHeight: 250,
        id: 'test-id'
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(200, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(250, w.innerHeight, defaultFuzzFactor);

        win.onClosed.addListener(callbackPass(function() {
          chrome.app.window.create('test.html', {
            bounds: { width: 500, height: 550 },
            minWidth: 400, minHeight: 450,
            maxWidth: 600, maxHeight: 650,
            id: 'test-id'
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

function testRestoreAfterGeometryCacheChange() {
  chrome.test.runTests([
    function restorePositionAndSize() {
      chrome.app.window.create('test.html', {
        bounds: { left: 200, top: 200, width: 200, height: 200 }, id: 'test-ra',
      }, callbackPass(function(win) { waitForLoad(win, function(win) {
        var w = win.contentWindow;
        // The fuzzy factor here is related to the fact that depending on the
        // platform, the bounds initialization will set the inner bounds or the
        // outer bounds.
        // TODO(mlamouri): remove the fuzz factor.
        assertFuzzyEq(200, w.screenX, 5);
        assertFuzzyEq(200, w.screenY, 30);
        chrome.test.assertEq(200, w.innerHeight);
        chrome.test.assertEq(200, w.innerWidth);

        w.resizeTo(300, 300);
        w.moveTo(100, 100);

        chrome.app.window.create('test.html', {
          bounds: { left: 200, top: 200, width: 200, height: 200 },
          id: 'test-rb', frame: 'none'
        }, callbackPass(function(win2) { waitForLoad(win2, function(win2) {
          var w2 = win2.contentWindow;
          chrome.test.assertEq(200, w2.screenX);
          chrome.test.assertEq(200, w2.screenY);
          chrome.test.assertEq(200, w2.innerWidth);
          chrome.test.assertEq(200, w2.innerHeight);

          w2.resizeTo(100, 100);
          w2.moveTo(300, 300);

          chrome.test.sendMessage('ListenGeometryChange', function(reply) {
            win.onClosed.addListener(callbackPass(function() {
              chrome.app.window.create('test.html', {
                id: 'test-ra'
              }, callbackPass(function(win) { waitForLoad(win, function(win) {
                var w = win.contentWindow;
                chrome.test.assertEq(100, w.screenX);
                chrome.test.assertEq(100, w.screenY);
                chrome.test.assertEq(300, w.outerWidth);
                chrome.test.assertEq(300, w.outerHeight);
              })}));
            }));

            win2.onClosed.addListener(callbackPass(function() {
              chrome.app.window.create('test.html', {
                id: 'test-rb', frame: 'none'
              },callbackPass(function(win2) { waitForLoad(win2, function(win2) {
                var w = win2.contentWindow;
                chrome.test.assertEq(300, w.screenX);
                chrome.test.assertEq(300, w.screenY);
                chrome.test.assertEq(100, w.outerWidth);
                chrome.test.assertEq(100, w.outerHeight);
              })}));
            }));

            win.close();
            win2.close();
          });
        })}));
      })}));
    },
  ]);
}

function testSizeConstraints() {
  chrome.test.runTests([
    function testUndefinedMinAndMaxSize() {
      chrome.app.window.create('test.html', {
        bounds: { width: 250, height: 250 }
      }, callbackPass(function(win) {
        chrome.test.assertEq(null, win.getMinWidth());
        chrome.test.assertEq(null, win.getMinHeight());
        chrome.test.assertEq(null, win.getMaxWidth());
        chrome.test.assertEq(null, win.getMaxHeight());
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

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(null, win.getMinWidth());
          chrome.test.assertEq(null, win.getMinHeight());
          chrome.test.assertEq(null, win.getMaxWidth());
          chrome.test.assertEq(null, win.getMaxHeight());
          win.close();
        }));
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

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(98, win.getMinWidth());
          chrome.test.assertEq(99, win.getMinHeight());
          chrome.test.assertEq(106, win.getMaxWidth());
          chrome.test.assertEq(107, win.getMaxHeight());
          win.close();
        }));
      }));
    },

    function testMinWidthLargerThanMaxWidth() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMinWidth(200);

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(200, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(200, win.getMaxWidth());
          chrome.test.assertEq(105, win.getMaxHeight());
          win.close();
        }));
      }));
    },

    function testMinHeightLargerThanMaxHeight() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMinHeight(200);

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(200, win.getMinHeight());
          chrome.test.assertEq(104, win.getMaxWidth());
          chrome.test.assertEq(200, win.getMaxHeight());
          win.close();
        }));
      }));
    },

    function testMaxWidthSmallerThanMinWidth() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMaxWidth(50);

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(100, win.getMaxWidth());
          chrome.test.assertEq(105, win.getMaxHeight());
          win.close();
        }));
      }));
    },

    function testMaxHeightSmallerThanMinHeight() {
      chrome.app.window.create('test.html', {
        bounds: { width: 102, height: 103 },
        minWidth: 100, minHeight: 101,
        maxWidth: 104, maxHeight: 105
      }, callbackPass(function(win) {
        win.setMaxHeight(50);

        // dummy call to ensure constraints have been changed in the browser
        chrome.test.waitForRoundTrip("msg", callbackPass(function(message) {
          chrome.test.assertEq(100, win.getMinWidth());
          chrome.test.assertEq(101, win.getMinHeight());
          chrome.test.assertEq(104, win.getMaxWidth());
          chrome.test.assertEq(101, win.getMaxHeight());
          win.close();
        }));
      }));
    },
  ]);
}

function testBadging() {
  chrome.test.runTests([
    function testSettingAndClearingBadge() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        win.setBadgeIcon('square.png');
        win.clearBadge();
        win.setBadgeIcon('non_square.png');
        win.clearBadge();
        chrome.test.sendMessage(
            'WaitForRoundTrip', callbackPass(function(reply) {}));
      }));
    },
  ]);
}

function testFrameColors() {
  chrome.test.runTests([
    function testWithNoColor() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        chrome.test.assertEq(false, win.hasFrameColor);
        win.close();
      }));
    },

    function testWithFrameNone() {
      chrome.app.window.create('test.html', {
        frame: 'none'
      },
      callbackPass(function(win) {
        chrome.test.assertEq(false, win.hasFrameColor);
        win.close();
      }));
    },

    function testWithBlack() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          type: 'chrome',
          color: '#000000'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(-16777216, win.frameColor);
        win.close();
      }));
    },

    function testWithWhite() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          color: '#FFFFFF'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(-1, win.frameColor);
        win.close();
      }));
    },

    function testWithWhiteShorthand() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          color: '#FFF'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(-1, win.frameColor);
        win.close();
      }));
    },

    function testWithFrameAndFrameOptions() {
      chrome.app.window.create('test.html', {
        frame: 'chrome',
        frameOptions: {
          color: '#FFF'
        }
      },
      callbackFail('Only one of frame and frameOptions can be supplied.'));
    },

    function testWithFrameNoneAndColor() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          type: 'none',
          color: '#FFF'
        }
      },
      callbackFail('Windows with no frame cannot have a color.'));
    },

    function testWithInvalidColor() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          color: 'DontWorryBeHappy'
        }
      },
      callbackFail('The color specification could not be parsed.'));
    }
  ]);
}

function testFrameColorsInStable() {
  chrome.test.runTests([
    function testWithNoColor() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(-1, win.frameColor);
        win.close();
      }));
    },

    function testWithOptionsGivesError() {
      chrome.app.window.create('test.html', {
        frameOptions: {
          color: '#FFF'
        }
      },
      callbackFail('frameOptions is only available in dev channel.'));
    }
  ]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function(reply) {
    window[reply]();
  });
});
