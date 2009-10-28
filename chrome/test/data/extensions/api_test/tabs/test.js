// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Tabs

// We have a bunch of places where we need to remember some state from one
// test (or setup code) to subsequent tests.
var firstWindowId = null;
var secondWindowId = null;
var firstTabIndex = null;
var testTabId = null;

var moveWindow1 = null;
var moveWindow2 = null;
var windowEventsWindow = null;
var moveTabIds = {};

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
}

chrome.test.runTests([
  function getSelected() {
    chrome.tabs.getSelected(null, pass(function(tab) {
      assertEq("about:blank", tab.url);
      assertEq("about:blank", tab.title);
      firstWindowId = tab.windowId;
      firstTabIndex = tab.index;
    }));
  },

  function openNewTab() {
    // TODO(asargent) Add more tests for the following cases:
    //  1) two windows open. create tab in "other" window.
    //  2) don't pass windowId. confirm created in "this" window.
    //  3) pass index. confirm placed at correct index position.
    //  4) pass selected. confirm is selected.
    chrome.tabs.create({"windowId" : firstWindowId, "selected" : false},
                       pass(function(tab){
      assertTrue(tab.index > firstTabIndex);
      assertEq(firstWindowId, tab.windowId);
      assertEq(false, tab.selected);
      assertEq("chrome://newtab/", tab.url);
    }));
  },

  // Setup a new window for later tests, and open some tabs in the
  // first and second windows.
  function createWindow() {
    // TODO(asargent) Add more tests for:
    //  1) window sizing/positioning.
    //  2) passed url (relative & absolute)
    chrome.windows.create({}, pass(function(win) {
      assertTrue(win.id > 0);
      secondWindowId = win.id;
      // Create first window.
      chrome.tabs.create({"windowId" : firstWindowId, "url" : pageUrl("a")},
                         pass(function() {
        // Create second window.
        chrome.tabs.create({"windowId" : secondWindowId, "url" :pageUrl("b")},
                           pass());
      }));
    }));
  },

  function getAllFirstWindow() {
    // TODO(asargent) Add test for passing null for windowId - this should
    // default to the "current" window.
    chrome.tabs.getAllInWindow(firstWindowId,
                               pass(function(tabs) {
      assertEq(3, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(firstWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);

        // The most recent tab should be selected
        assertEq((i == 2), tabs[i].selected);
      }
      assertEq("about:blank", tabs[0].url);
      assertEq("chrome://newtab/", tabs[1].url);
      assertEq(pageUrl("a"), tabs[2].url);
      testTabId = tabs[2].id;
    }));
  },

  function getAllSecondWindow() {
    chrome.tabs.getAllInWindow(secondWindowId,
                               pass(function(tabs) {
      assertEq(2, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(secondWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);
      }
      assertEq("chrome://newtab/", tabs[0].url);
      assertEq(pageUrl("b"), tabs[1].url);
    }));
  },

  function updateUrl() {
    chrome.tabs.get(testTabId, pass(function(tab) {
      assertEq(pageUrl("a"), tab.url);
      // Update url.
      chrome.tabs.update(testTabId, {"url": pageUrl("c")},
                         pass(function(tab){
        chrome.test.assertEq(pageUrl("c"), tab.url);
        // Check url.
        chrome.tabs.get(testTabId, pass(function(tab) {
          assertEq(pageUrl("c"), tab.url);
        }));
      }));
    }));
  },

  function updateSelect() {
    chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
      assertEq(false, tabs[1].selected);
      assertEq(true, tabs[2].selected);
      // Select tab[1].
      chrome.tabs.update(tabs[1].id, {selected: true},
                         pass(function(tab1){
        // Check update of tab[1].
        chrome.test.assertEq(true, tab1.selected);
        chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
          assertEq(true, tabs[1].selected);
          assertEq(false, tabs[2].selected);
          // Select tab[2].
          chrome.tabs.update(tabs[2].id, {selected: true},
                             pass(function(tab2){
            // Check update of tab[2].
            chrome.test.assertEq(true, tab2.selected);
            chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
              assertEq(false, tabs[1].selected);
              assertEq(true, tabs[2].selected);
            }));
          }));
        }));
      }));
    }));
  },

  // Create 2 new windows, close existing windows.
  function moveTabsSetup1() {
    chrome.windows.create({}, pass(function(win1) {
      moveWindow1 = win1.id;
      chrome.windows.create({}, pass(function(win2) {
        moveWindow2 = win2.id;
        chrome.windows.remove(firstWindowId, pass(function(){
          chrome.windows.remove(secondWindowId, pass());
        }));
      }));
    }));
  },

  // Create a bunch of tabs and record the resulting ids.
  function moveTabsSetup2() {
    var letters = ['a', 'b', 'c', 'd', 'e'];
    for (var i in letters) {
      chrome.tabs.create({"windowId": moveWindow1,
                          "url": pageUrl(letters[i])},
                         pass(function(tab) {
        var letter = tab.url[tab.url.length-6];
        moveTabIds[letter] = tab.id;

        // Assert on last callback that tabs were added in the order we created
        // them.
        if (letter == 'e') {
          chrome.tabs.getAllInWindow(moveWindow1, pass(function(tabs) {
            assertEq(6, tabs.length);
            assertEq("chrome://newtab/", tabs[0].url);
            assertEq(pageUrl("a"), tabs[1].url);
            assertEq(pageUrl("b"), tabs[2].url);
            assertEq(pageUrl("c"), tabs[3].url);
            assertEq(pageUrl("d"), tabs[4].url);
            assertEq(pageUrl("e"), tabs[5].url);
          }));
        }
      }));
    }
  },

  // Do a series of moves so that we get the following
  //
  // Before:
  //  Window1: (newtab),a,b,c,d,e
  //  Window2: (newtab)
  //
  // After:
  //  Window1: (newtab),a,e,c
  //  Window2: b,(newtab),d
  function moveTabs() {
    chrome.tabs.move(moveTabIds['b'], {"windowId": moveWindow2, "index": 0},
                     pass(function(tabB) {
        chrome.test.assertEq(0, tabB.index);
        chrome.tabs.move(moveTabIds['e'], {"index": 2},
                         pass(function(tabE) {
          chrome.test.assertEq(2, tabE.index);
          chrome.tabs.move(moveTabIds['d'], {"windowId": moveWindow2,
                           "index": 2}, pass(function(tabD) {
            chrome.test.assertEq(2, tabD.index);
        }));
      }));
    }));
  },

  // Check that the tab/window state is what we expect after doing moves.
  function moveTabsCheck() {
    chrome.tabs.getAllInWindow(moveWindow1, pass(function(tabs) {
      assertEq(4, tabs.length);
      assertEq("chrome://newtab/", tabs[0].url);
      assertEq(pageUrl("a"), tabs[1].url);
      assertEq(pageUrl("e"), tabs[2].url);
      assertEq(pageUrl("c"), tabs[3].url);

      chrome.tabs.getAllInWindow(moveWindow2, pass(function(tabs) {
        assertEq(3, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
        assertEq(pageUrl("d"), tabs[2].url);
      }));
    }));
  },

  function remove() {
    chrome.tabs.remove(moveTabIds["d"], pass(function() {
      chrome.tabs.getAllInWindow(moveWindow2,
                                 pass(function(tabs) {
        assertEq(2, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
      }));
    }));
  },

  function detectLanguage() {
    chrome.tabs.getAllInWindow(moveWindow1, pass(function(tabs) {
      chrome.tabs.detectLanguage(tabs[0].id, pass(function(lang) {
        assertEq("en", lang);
      }));
    }));
  },

  /* TODO(rafaelw): Ideally, this test would include a page with known content,
     it'd take a capture and compair it to some expected output.
     TODO(rafaelw): This test fails in at least three distinct ways. One where
     the function actually fails to get anything and logs a "Internal error
     while trying to capture visible region of the current tab" error from the
     browser process.
  function captureVisibleTab() {
    // Take First Capture
    chrome.tabs.captureVisibleTab(moveWindow1,
                                  pass(function(window1Url) {
      assertEq("string", typeof(window1Url));
      assertTrue(window1Url.length > 0);

      // Take Second Capture
      chrome.tabs.captureVisibleTab(moveWindow2,
                                    pass(function(window2Url) {
        assertEq("string", typeof(window2Url));
        assertTrue(window2Url.length > 0);
        assertTrue(window1Url != window2Url);

        // Now pass null for windowId - it should come back with something
        // equal to either the first or second window. This is nondeterministic
        // depending on whether you let chrome stay focused, or click
        // focus away (or are running on the try/build servers).
        chrome.tabs.captureVisibleTab(null, pass(function(url) {
          assertEq("string", typeof(url));
          assertTrue(url.length > 0);
          assertTrue(url == window1Url || url == window2Url);
        }));
      }));
    }));
  }, */

  function tabsOnCreated() {
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      assertEq(pageUrl("f"), tab.url);
    });

    chrome.tabs.create({"windowId": moveWindow1, "url": pageUrl("f"),
                        "selected": true}, pass(function(tab) {}));
  },

  function tabsOnUpdatedIgnoreTabArg() {
    // A third argument was added to the onUpdated event callback.
    // Test that an event handler which ignores this argument works.
    var onUpdatedCompleted = chrome.test.listenForever(chrome.tabs.onUpdated,
      function(tabid, changeInfo) {
        if (tabid == moveTabIds['a'] && changeInfo.status == "complete") {
          onUpdatedCompleted();
        }
      }
    );

    chrome.tabs.update(moveTabIds['a'], {"url": pageUrl("f")}, pass());
  },

  function tabsOnUpdated() {
    var onUpdatedCompleted = chrome.test.listenForever(
      chrome.tabs.onUpdated,
      function(tabid, changeInfo, tab) {
        // |tab| contains the id of the tab it describes.
        // Test that |tabid| matches this id.
        assertEq(tabid, tab.id);

        // If |changeInfo| has a status property, than
        // it should match the status of the tab in |tab|.
        if (changeInfo.status) {
          assertEq(changeInfo.status, tab.status);
        }

        if (tabid == moveTabIds['a'] && changeInfo.status == "complete") {
          onUpdatedCompleted();
        }
      }
    );

    chrome.tabs.update(moveTabIds['a'], {"url": pageUrl("f")}, pass());
  },

  function tabsOnMoved() {
    chrome.test.listenOnce(chrome.tabs.onMoved, function(tabid, info) {
      assertEq(moveTabIds['a'], tabid);
    });

    chrome.tabs.move(moveTabIds['a'], {"index": 0}, pass());
  },

  function tabsOnSelectionChanged() {
    chrome.test.listenOnce(chrome.tabs.onSelectionChanged,
      function(tabid, info) {
        assertEq(moveTabIds['c'], tabid);
      }
    );

    chrome.tabs.update(moveTabIds['c'], {"selected": true},
                       pass());
  },

  function tabsOnRemoved() {
    chrome.test.listenOnce(chrome.tabs.onRemoved, function(tabid) {
      assertEq(moveTabIds['c'], tabid);
    });

    chrome.tabs.remove(moveTabIds['c'], pass());
  },

  function setupRelativeUrlTests() {
    chrome.windows.create({}, pass(function(win) {
      assertTrue(win.id > 0);
      firstWindowId = win.id;

      chrome.windows.getAll({}, pass(function(windows) {
        for (var i = 0; i < windows.length; i++) {
          if (windows[i].id != firstWindowId) {
            chrome.windows.remove(windows[i].id, pass());
          }
        }
      }));
    }));
  },

  function relativeUrlTabsCreate() {
    // Will be called from relative.html
    window.relativePageLoaded = chrome.test.callbackAdded();
    var createCompleted = chrome.test.callbackAdded();

    chrome.tabs.create({windowId: firstWindowId, url: 'relative.html'},
      function(tab){
        testTabId = tab.id;
        createCompleted();
      }
    );
  },

  function relativeUrlTabsUpdate() {
    // Will be called from relative.html
    window.relativePageLoaded = chrome.test.callbackAdded();

    chrome.tabs.update(testTabId, {url: pageUrl("a")}, function(tab) {
      chrome.test.assertEq(pageUrl("a"), tab.url);
      chrome.tabs.update(tab.id, {url: "relative.html"}, function(tab) {
      });
    });
  },

  function relativeUrlWindowsCreate() {
    // Will be called from relative.html
    window.relativePageLoaded = chrome.test.callbackAdded();

    chrome.windows.create({url: "relative.html"});
  },

  function windowsOnCreated() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertTrue(window.width > 0);
      chrome.test.assertTrue(window.height > 0);
      windowEventsWindow = window;
      chrome.tabs.getAllInWindow(window.id, pass(function(tabs) {
        assertEq(pageUrl("a"), tabs[0].url);
      }));
    });

    chrome.windows.create({"url": pageUrl("a")}, pass(function(tab) {}));
  },

  function windowsOnRemoved() {
    chrome.test.listenOnce(chrome.windows.onRemoved, function(windowId) {
      assertEq(windowEventsWindow.id, windowId);
    });

    chrome.windows.remove(windowEventsWindow.id, pass());
  }

  // TODO(asargent) We still need to add tests for the following:
  //  Methods:
  //   -chrome.tabs.connect
  //  Events:
  //   -chrome.tabs.onAttached
  //   -chrome.tabs.onDetched
  //
  // Also, it would be an improvement to check the captureVisibleTab results
  // against a known-good result.
]);
