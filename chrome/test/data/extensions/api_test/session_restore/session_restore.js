// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pages = [pageUrl('a'), pageUrl('b'), pageUrl('c')];
var firstWindowTabIds = [];
var recentlyClosedSecondWindowTabIds = [];
var recentlyClosedTabIds = [];
var recentlyClosedWindowIds = [];
var windowIds = [];

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, callback) {
  chrome.windows.create({url: tabUrls}, function(win) {
    var newTabIds = [];
    win.tabs.forEach(function(tab) {
      newTabIds.push(tab.id);
    });
    callback(win.id, newTabIds);
  });
}

function callForEach(fn, calls, eachCallback, doneCallback) {
  if (!calls.length) {
    doneCallback();
    return;
  }
  fn.call(null, calls[0], callbackPass(function() {
    eachCallback.apply(null, arguments);
    callForEach(fn, calls.slice(1), eachCallback, doneCallback);
  }));
}

function checkEntries(expectedEntries, actualEntries) {
  assertEq(expectedEntries.length, actualEntries.length);
  expectedEntries.forEach(function(expected, i) {
    var actual = actualEntries[i];
    if (expected.tab) {
      assertTrue(actual.hasOwnProperty('tab'));
      assertFalse(actual.hasOwnProperty('window'));
      assertEq(expected.tab.url, actual.tab.url);
    } else {
      assertTrue(actual.hasOwnProperty('window'));
      assertFalse(actual.hasOwnProperty('tab'));
      assertEq(expected.window.tabsLength, actual.window.tabs.length);
    }
  });
}

chrome.test.runTests([
  // After setupWindows
  //
  //  Window1: a,b,c
  //  Window2: a,b
  //  Window3: a,b
  //
  // After retriveClosedTabs:
  //
  //  Window1: c
  //  Window2: a,b
  //  Window3: a,b
  //  ClosedList: a,b
  //
  // After retriveClosedWindows:
  //
  //  Window1: c
  //  ClosedList: Window2,Window3,a,b
  function setupWindows() {
    var callArgs = [
      pages,
      pages.slice(0, 2),
      pages.slice(0, 2)
    ];
    callForEach(
      createWindow,
      callArgs,
      function each(winId, tabIds) {
        windowIds.push(winId);
      },
      function done() {
        chrome.tabs.getAllInWindow(windowIds[0], callbackPass(function(tabs) {
          assertEq(pages.length, tabs.length);
          tabs.forEach(function(tab) {
            firstWindowTabIds.push(tab.id);
          });
        }));
        chrome.windows.getAll({"populate": true},
          callbackPass(function(win) {
            assertEq(callArgs.length + 1, win.length);
          })
        );
      }
    );
  },

  function retrieveClosedTabs() {
    // Check that the recently closed list contains what we expect
    // after removing tabs.
    callForEach(
      chrome.tabs.remove,
      firstWindowTabIds.slice(0, 2).reverse(),
      function each() {
      },
      function done() {
        chrome.sessionRestore.getRecentlyClosed(
          {maxResults: 2, entryType: "tab"},
          callbackPass(function(entries) {
            var expectedEntries = [
              { tab: { url: pages[0] } },
              { tab: { url: pages[1] } }
            ];
            checkEntries(expectedEntries, entries);
            entries.forEach(function(entry) {
              recentlyClosedTabIds.push(entry.id);
            });
          })
        );
      }
    );
  },

  function retrieveClosedWindows() {
    // Check that the recently closed list contains what we expect
    // after removing windows.
    callForEach(
      chrome.windows.remove,
      windowIds.slice(1, 3).reverse(),
      function each() {
      },
      function done() {
        chrome.sessionRestore.getRecentlyClosed(
          {maxResults: 2, entryType: "window"},
          callbackPass(function(entries) {
            var expectedEntries = [
              { window: { tabsLength: 2 } },
              { window: { tabsLength: 2 } }
            ];
            checkEntries(expectedEntries, entries);
            entries[0].window.tabs.forEach(function(tab) {
              recentlyClosedSecondWindowTabIds.push(tab.id);
            });
            entries.forEach(function(entry) {
              recentlyClosedWindowIds.push(entry.id);
            });
          })
        );
      }
    );
  },

  function retrieveClosedEntries() {
    // Check that the recently closed list contains what we expect
    // after removing tabs and windows.
    chrome.sessionRestore.getRecentlyClosed(
      callbackPass(function(entries) {
        var expectedEntries = [
          { window: { tabsLength: 2 } },
          { window: { tabsLength: 2 } },
          { tab: { url: pages[0] } },
          { tab: { url: pages[1] } }
        ];
        checkEntries(expectedEntries, entries);
        assertEq(recentlyClosedTabIds.length + recentlyClosedWindowIds.length,
          entries.length);
      })
    );
  },

  function retrieveMaxEntries() {
    // Check that the recently closed list contains what we expect
    // after removing tabs and windows.
    chrome.sessionRestore.getRecentlyClosed({maxResults: 25},
      callbackPass(function(entries) {
        var expectedEntries = [
          { window: { tabsLength: 2 } },
          { window: { tabsLength: 2 } },
          { tab: { url: pages[0] } },
          { tab: { url: pages[1] } }
        ];
        checkEntries(expectedEntries, entries);
        assertEq(recentlyClosedTabIds.length + recentlyClosedWindowIds.length,
          entries.length);
      })
    );
  },

  function restoreClosedTabs() {
    chrome.windows.get(windowIds[0], {"populate": true},
      callbackPass(function(win) {
        var tabCountBeforeRestore = win.tabs.length;
        callForEach(
          chrome.sessionRestore.restore,
          recentlyClosedTabIds.slice(0, 2),
          function each() {
          },
          function done() {
            chrome.windows.get(windowIds[0], {"populate": true},
              callbackPass(function(win){
                assertEq(tabCountBeforeRestore + 2, win.tabs.length);
                win.tabs.forEach(function(tab, i) {
                  assertEq(pages[i++], tab.url);
                });
              })
            );
          }
        );
      })
    );
  },

  function restoreTabInClosedWindow() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      chrome.sessionRestore.restore(recentlyClosedSecondWindowTabIds[0],
        callbackPass(function() {
          chrome.windows.getAll({"populate": true},
            callbackPass(function(win) {
              assertEq(windowCountBeforeRestore + 1, win.length);
              assertEq(1, win[win.length - 1].tabs.length);
              assertEq(pages[0], win[win.length - 1].tabs[0].url);
            })
          );
        })
      );
    }));
  },

  function restoreClosedWindows() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      callForEach(
        chrome.sessionRestore.restore,
        recentlyClosedWindowIds.slice(0, 1),
        function each() {
        },
        function done() {
          chrome.windows.getAll({"populate": true},
            callbackPass(function(win) {
              assertEq(windowCountBeforeRestore + 1, win.length);
            })
          );
        }
      );
    }));
  },

  function restoreSameEntryTwice() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      chrome.sessionRestore.restore(recentlyClosedWindowIds[0],
        callbackFail("Invalid session id.", function() {
          chrome.windows.getAll({"populate": true},
            callbackPass(function(win) {
              assertEq(windowCountBeforeRestore, win.length);
            })
          );
        })
      );
    }));
  },

  function restoreInvalidEntries() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      chrome.sessionRestore.restore(-1,
        callbackFail("Invalid session id.", function() {
          chrome.windows.getAll({"populate": true},
            callbackPass(function(win) {
              assertEq(windowCountBeforeRestore, win.length);
            })
          );
        })
      );
    }));
  },

  function restoreMostRecentEntry() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      chrome.sessionRestore.restore(callbackPass(function() {
        chrome.windows.getAll({"populate": true},
          callbackPass(function(win) {
            assertEq(windowCountBeforeRestore + 1, win.length);
          })
        );
      }));
    }));
  },

  function checkRecentlyClosedListEmpty() {
    chrome.windows.getAll({"populate": true}, callbackPass(function(win) {
      var windowCountBeforeRestore = win.length;
      chrome.sessionRestore.restore(
        callbackFail("There are no recently closed sessions.", function() {
          chrome.windows.getAll({"populate": true},
            callbackPass(function(win) {
              assertEq(windowCountBeforeRestore, win.length);
            })
          );
        })
      );
    }));
  }

]);
