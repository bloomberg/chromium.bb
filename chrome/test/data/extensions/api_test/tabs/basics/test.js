// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Tabs

// We have a bunch of places where we need to remember some state from one
// test (or setup code) to subsequent tests.
var firstWindowId = null;
var secondWindowId = null;
var firstTabIndex = null;
var testTabId = null;

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

  function create() {
    chrome.tabs.create({"windowId" : firstWindowId, "selected" : false},
                       pass(function(tab){
      assertTrue(tab.index > firstTabIndex);
      assertEq(firstWindowId, tab.windowId);
      assertEq(false, tab.selected);
      assertEq("chrome://newtab/", tab.url);
    }));
  },

  function createInOtherWindow() {
    chrome.windows.create({}, pass(function(win) {
      // The newly created window is now the currentWindow.
      // Create a tab in the older window.
      chrome.tabs.create({"windowId" : firstWindowId, "selected" : false},
                         pass(function(tab) {
        assertEq(firstWindowId, tab.windowId);
      }));
      // Create a tab in this new window.
      chrome.tabs.create({"windowId" : win.id}, pass(function(tab) {
        assertEq(win.id, tab.windowId);
      }));
    }));
  },

  function createAtIndex() {
    chrome.tabs.create({"windowId" : firstWindowId, "index" : 1},
                       pass(function(tab) {
      assertEq(1, tab.index);
    }));
  },

  function createSelected() {
    chrome.tabs.create({"windowId" : firstWindowId, "selected" : true},
                       pass(function(tab) {
      assertTrue(tab.selected);
      chrome.tabs.getSelected(firstWindowId, pass(function(selectedTab) {
        assertEq(tab.id, selectedTab.id);
      }));
    }));
  },

  function setupTwoWindows() {
    setupWindow(["about:blank", "chrome://newtab/", pageUrl("a")],
                pass(function(winId, tabIds) {
      firstWindowId = winId;
      testTabId = tabIds[2];

      createWindow(["chrome://newtab/", pageUrl("b")], {},
                           pass(function(winId, tabIds) {
        secondWindowId = winId;
      }));
    }));
  },

  function getAllInWindow() {
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
    }));

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

  /* TODO: Enable this test when crbug.com/28055 is fixed. This bug causes a
     newly created window not to be set as the current window, if
     Chrome was not the foreground window when the create call was made.
  function getAllInWindowNullArg() {
    chrome.tabs.getAllInWindow(null, pass(function(tabs) {
      assertEq(2, tabs.length);
      assertEq(secondWindowId, tabs[0].windowId);
    }));
  }, */

  function update() {
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
          chrome.tabs.update(tabs[2].id,
			     {selected: true},
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

  // Do a series of moves so that we get the following
  //
  // Before:
  //  Window1: (newtab),a,b,c,d,e
  //  Window2: (newtab)
  //
  // After:
  //  Window1: (newtab),a,e,c
  //  Window2: b,(newtab),d
  function setupLetterPages() {
    var pages = ["chrome://newtab/", pageUrl('a'), pageUrl('b'),
                   pageUrl('c'), pageUrl('d'), pageUrl('e')];
    setupWindow(pages, pass(function(winId, tabIds) {
      firstWindowId = winId;
      moveTabIds['a'] = tabIds[1];
      moveTabIds['b'] = tabIds[2];
      moveTabIds['c'] = tabIds[3];
      moveTabIds['d'] = tabIds[4];
      moveTabIds['e'] = tabIds[5];
      createWindow(["chrome://newtab/"], {}, pass(function(winId, tabIds) {
        secondWindowId = winId;
      }));
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(pages.length, tabs.length);
        for (var i in tabs) {
          assertEq(pages[i], tabs[i].url);
        }
      }));
    }));
  },

  function move() {
    // Check that the tab/window state is what we expect after doing moves.
    function checkMoveResults()
    {
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(4, tabs.length);
        assertEq("chrome://newtab/", tabs[0].url);
        assertEq(pageUrl("a"), tabs[1].url);
        assertEq(pageUrl("e"), tabs[2].url);
        assertEq(pageUrl("c"), tabs[3].url);

        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(3, tabs.length);
          assertEq(pageUrl("b"), tabs[0].url);
          assertEq("chrome://newtab/", tabs[1].url);
          assertEq(pageUrl("d"), tabs[2].url);
        }));
      }));
    }

    chrome.tabs.move(moveTabIds['b'], {"windowId": secondWindowId, "index": 0},
                     pass(function(tabB) {
        chrome.test.assertEq(0, tabB.index);
        chrome.tabs.move(moveTabIds['e'], {"index": 2},
                         pass(function(tabE) {
          chrome.test.assertEq(2, tabE.index);
          chrome.tabs.move(moveTabIds['d'], {"windowId": secondWindowId,
                           "index": 2}, pass(function(tabD) {
            chrome.test.assertEq(2, tabD.index);
            checkMoveResults();
        }));
      }));
    }));
  },

  function remove() {
    chrome.tabs.remove(moveTabIds["d"], pass(function() {
      chrome.tabs.getAllInWindow(secondWindowId,
                                 pass(function(tabs) {
        assertEq(2, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
      }));
    }));
  },

  /*
  // TODO(jcampan): http://crbug.com/30662 the detection language library
  //                crashes on some sites and has been temporarily disabled.
  function detectLanguage() {
    chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
      chrome.tabs.detectLanguage(tabs[0].id, pass(function(lang) {
        assertEq("en", lang);
      }));
    }));
  },
  */

  function tabsOnCreated() {
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      assertEq(pageUrl("f"), tab.url);
    });

    chrome.tabs.create({"windowId": firstWindowId, "url": pageUrl("f"),
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

  function setupTabsOnAttachDetach()
  {
    setupWindow(["", ""], pass(function(winId, tabIds) {
      firstWindowId = winId;
      testTabId = tabIds[1];
      createWindow([""], {}, pass(function(winId, tabIds) {
        secondWindowId = winId;
      }));
    }));
  },

  function tabsOnAttached() {
    function moveAndListen(tabId, properties, callback) {
      chrome.test.listenOnce(chrome.tabs.onAttached,
                             function(testTabId, info) {
        // Ensure notification is correct.
        assertEq(testTabId, tabId);
        assertEq(properties.windowId, info.newWindowId);
        assertEq(properties.index, info.newPosition);
        if (callback)
          callback();
      });
      chrome.tabs.move(tabId, properties);
    };

    // Move tab to second window, then back to first.
    // The original tab/window configuration should be restored.
    // tabsOnDetached() depends on it.
    moveAndListen(testTabId, {"windowId": secondWindowId, "index": 0},
                  pass(function() {
      moveAndListen(testTabId, {"windowId": firstWindowId, "index": 1});
    }));
  },

  function tabsOnDetached() {
    function moveAndListen(tabId, oldWindowId, oldIndex, properties,
                                 callback) {
      chrome.test.listenOnce(chrome.tabs.onDetached,
                             function(detachedTabId, info) {
        // Ensure notification is correct.
        assertEq(detachedTabId, tabId);
        assertEq(oldWindowId, info.oldWindowId);
        assertEq(oldIndex, info.oldPosition);
        if (callback)
          callback();
      });
      chrome.tabs.move(tabId, properties);
    };

    // Move tab to second window, then back to first.
    moveAndListen(testTabId, firstWindowId, 1,
                  {"windowId": secondWindowId, "index": 0}, pass(function() {
      moveAndListen(testTabId, secondWindowId, 0,
                    {"windowId": firstWindowId, "index": 1});
    }));
  },

  function setupRelativeUrlTests() {
    setupWindow(["about:blank"], pass(function(winId, tabIds) {
      firstWindowId = winId;
    }));
  },

  function relativeUrlTestsTabsCreate() {
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

  function relativeUrlTestsTabsUpdate() {
    // Will be called from relative.html
    window.relativePageLoaded = chrome.test.callbackAdded();

    chrome.tabs.update(testTabId, {url: pageUrl("a")}, function(tab) {
      chrome.test.assertEq(pageUrl("a"), tab.url);
      chrome.tabs.update(tab.id, {url: "relative.html"}, function(tab) {
      });
    });
  },

  function relativeUrlTestsWindowCreate() {
    // Will be called from relative.html
    window.relativePageLoaded = chrome.test.callbackAdded();

    chrome.windows.create({url: "relative.html"});
  },

  function windowCreate() {
    chrome.windows.create({type: "popup"}, pass(function(window) {
      assertEq(window.type, "popup");
      assertTrue(!window.incognito);
    }));
    chrome.windows.create({incognito: true}, pass(function(window) {
      // This extension is not incognito-enabled, so it shouldn't be able to
      // see the incognito window.
      assertEq(window, null);
    }));
  },

  function windowsOnCreated() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      assertTrue(window.width > 0);
      assertTrue(window.height > 0);
      assertEq(window.type, "normal");
      assertTrue(!window.incognito);
      windowEventsWindow = window;
      chrome.tabs.getAllInWindow(window.id, pass(function(tabs) {
        assertEq(pageUrl("a"), tabs[0].url);
      }));
    });

    chrome.windows.create({"url": pageUrl("a")}, pass(function(tab) {}));
  },

  function windowSetFocused() {
    chrome.windows.getCurrent(function(oldWin) {
      chrome.windows.create({}, function(newWin) {
        assertTrue(newWin.focused);
        chrome.windows.update(oldWin.id, {focused:true});
        chrome.windows.get(oldWin.id, pass(function(oldWin2) {
          assertTrue(oldWin2.focused);
        }));
      });
    });
  },

  /* TODO: Enable this test when crbug.com/28055 is fixed. This bug causes a
     newly created window not to be set as the current window, if
     Chrome was not the foreground window when the create call was made.
  function windowsOnFocusChanged() {
    chrome.windows.create({}, pass(function(window) {
      chrome.test.listenOnce(chrome.windows.onFocusChanged,
                             function(windowId) {
        assertEq(windowEventsWindow.id, windowId);
      });
      chrome.windows.remove(window.id);
    }));
  } */
]);
