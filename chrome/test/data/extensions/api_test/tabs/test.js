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
var moveTabIds = {};

var testCallback = chrome.test.testCallback;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

chrome.test.runTests([
  function getSelected() {
    chrome.tabs.getSelected(null, testCallback(true, function(tab) {
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
                       testCallback(true, function(tab){
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
    chrome.windows.create({}, testCallback(false, function(win) {
      assertTrue(win.id > 0);
      secondWindowId = win.id;
      chrome.tabs.create({"windowId" : firstWindowId, "url" : "chrome://a"},
                         testCallback(false, null));
      chrome.tabs.create({"windowId" : secondWindowId, "url" : "chrome://b"},
                         testCallback(true, null));
   }));
  },

  function getAllFirstWindow() {
    // TODO(asargent) Add test for passing null for windowId - this should
    // default to the "current" window.
    chrome.tabs.getAllInWindow(firstWindowId,
                               testCallback(true, function(tabs) {
      assertEq(3, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(firstWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);

        // The most recent tab should be selected
        assertEq((i == 2), tabs[i].selected);
      }
      assertEq("about:blank", tabs[0].url);
      assertEq("chrome://newtab/", tabs[1].url);
      assertEq("chrome://a/", tabs[2].url);
      testTabId = tabs[2].id;
    }));
  },

  function getAllSecondWindow() {
    chrome.tabs.getAllInWindow(secondWindowId,
                               testCallback(true, function(tabs) {
      assertEq(2, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(secondWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);
      }
      assertEq("chrome://newtab/", tabs[0].url);
      assertEq("chrome://b/", tabs[1].url);
    }));
  },

  function update() {
    chrome.tabs.update(testTabId, {"selected":true, "url": "chrome://c"},
                       testCallback(false, function(){
      chrome.tabs.getSelected(firstWindowId, testCallback(true, function(tab) {
        assertEq("chrome://c/", tab.url);
        assertEq(true, tab.selected);
      }));
    }));
  },

  // Create 2 new windows, close existing windows.
  function moveTabsSetup1() {
    chrome.windows.create({}, testCallback(false, function(win) {
      moveWindow1 = win.id;
    }));
    chrome.windows.create({}, testCallback(false, function(win) {
      moveWindow2 = win.id;
    }));
    chrome.windows.remove(firstWindowId, testCallback(false, null));
    chrome.windows.remove(secondWindowId, testCallback(true, null));
  },

  // Create a bunch of tabs and record the resulting ids.
  function moveTabsSetup2() {
    var letters = ['a', 'b', 'c', 'd', 'e'];
    for (var i in letters) {
      chrome.tabs.create({"windowId": moveWindow1,
                          "url": "chrome://" + letters[i]},
                         testCallback(false, function(tab) {
        var letter = tab.url[tab.url.length-2];
        moveTabIds[letter] = tab.id;
        if (tab.url == "chrome://e/") {
          chrome.test.succeed();
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
                     testCallback(false, null));
    chrome.tabs.move(moveTabIds['e'], {"index": 2},
                     testCallback(false, null));
    chrome.tabs.move(moveTabIds['d'], {"windowId": moveWindow2, "index": 2},
                     testCallback(true, null));
  },

  // Check that the tab/window state is what we expect after doing moves.
  function moveTabsCheck() {
    chrome.tabs.getAllInWindow(moveWindow1, testCallback(false, function(tabs) {
      assertEq(4, tabs.length);
      assertEq("chrome://newtab/", tabs[0].url);
      assertEq("chrome://a/", tabs[1].url);
      assertEq("chrome://e/", tabs[2].url);
      assertEq("chrome://c/", tabs[3].url);
    }));
    chrome.tabs.getAllInWindow(moveWindow2, testCallback(true, function(tabs) {
      assertEq(3, tabs.length);
      assertEq("chrome://b/", tabs[0].url);
      assertEq("chrome://newtab/", tabs[1].url);
      assertEq("chrome://d/", tabs[2].url);
    }));
  },

  function remove() {
    chrome.tabs.remove(moveTabIds["d"], testCallback(false, function() {
      chrome.tabs.getAllInWindow(moveWindow2,
                                 testCallback(true, function(tabs) {
        assertEq(2, tabs.length);
        assertEq("chrome://b/", tabs[0].url);
        assertEq("chrome://newtab/", tabs[1].url);
      }));
    }));
  },

  function detectLanguage() {
    chrome.tabs.getAllInWindow(moveWindow1, testCallback(false, function(tabs) {
      chrome.tabs.detectLanguage(tabs[0].id, testCallback(true, function(lang) {
        assertEq("en", lang);
      }));
    }));
  },

  function captureVisibleTab() {
    // Grab an image for each of our two windows.
    var firstImage;
    var secondImage;
    chrome.tabs.captureVisibleTab(moveWindow1,
                                  testCallback(false, function(url) {
      assertEq("string", typeof(url));
      assertTrue(url.length > 0);
      firstImage = url;
    }));
    chrome.tabs.captureVisibleTab(moveWindow2,
                                  testCallback(false, function(url) {
      assertEq("string", typeof(url));
      assertTrue(url.length > 0);
      secondImage = url;
      assertTrue(firstImage != secondImage);
    }));

    // Now pass null for windowId - it should come back with something
    // equal to either the first or second window. This is nondeterministic
    // depending on whether you let chrome stay focused, or click
    // focus away (or are running on the try/build servers).
    chrome.tabs.captureVisibleTab(null, testCallback(true, function(url) {
      assertEq("string", typeof(url));
      assertTrue(url.length > 0);
      assertTrue(url == firstImage || url == secondImage);
    }));
  },

  function onCreated() {
    var callbackSuccess = false;
    var listener = function(tab) {
      assertEq("chrome://f/", tab.url);
      callbackSuccess = true;
    };
    chrome.tabs.onCreated.addListener(listener);
    chrome.tabs.create({"windowId": moveWindow1, "url": "chrome://f",
                        "selected": true}, testCallback(true, function(tab) {
      chrome.tabs.onCreated.removeListener(listener);
      assertTrue(callbackSuccess);
    }));
  },

  function onUpdated() {
    var listener = function(tabid, info) {
      if (tabid != moveTabIds['a']) {
        return;  // ignore updates for tabs we weren't interested in
      }
      if (info.status == "complete") {
        chrome.tabs.onUpdated.removeListener(listener);
        chrome.test.succeed();
      }
    };
    chrome.tabs.onUpdated.addListener(listener);
    chrome.tabs.update(moveTabIds['a'], {"url": "chrome://aa"},
                       testCallback(false, null));
  },

  function onMoved() {
    var listener = function(tabid, info) {
      chrome.tabs.onMoved.removeListener(listener);
      assertEq(moveTabIds['a'], tabid);
      chrome.test.succeed();
    };
    chrome.tabs.onMoved.addListener(listener);
    chrome.tabs.move(moveTabIds['a'], {"index": 0}, testCallback(false, null));
  },

  function onSelectionChanged() {
    var listener = function(tabid, info) {
      chrome.tabs.onSelectionChanged.removeListener(listener);
      assertEq(moveTabIds['c'], tabid);
      chrome.test.succeed();
    };
    chrome.tabs.onSelectionChanged.addListener(listener);
    chrome.tabs.update(moveTabIds['c'], {"selected": true},
                       testCallback(false, null));
  },

  function onRemoved() {
    var listener = function(tabid) {
      chrome.tabs.onRemoved.removeListener(listener);
      assertEq(moveTabIds['c'], tabid);
      chrome.test.succeed();
    };
    chrome.tabs.onRemoved.addListener(listener);
    chrome.tabs.remove(moveTabIds['c'], testCallback(false, null));
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
