// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OffscreenTabs API test
// browser_tests.exe --gtest_filter=ExperimentalApiTest.OffscreenTabs

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var extensionPath =
  location.href.substring(0, location.href.lastIndexOf("/") + 1);

var inTabs = [
  {
    "url": extensionPath + "a.html",
    "width": 200,
    "height": 200
  },
  {
    "url": extensionPath + "b.html",
    "width": 200,
    "height": 200
  },
  {
    "url": extensionPath + "c.html",
    "width": 1000,
    "height": 800
  }
];

// Tab Management (create, get, getAll, update, remove)
var tabs = [];

// Mouse (sendMouseEvent)
var tabMouse = new Object();

var mouseEvent = {
  "button": 0,
  "altKey": false,
  "ctrlKey": false,
  "shiftKey": false
};

var x = 11;
var y = 11;

// Keyboard (sendKeyboardEvent)

var tabKeyboard = new Object();

// Display (toDataUrl)

var tabsCaptured = [];

var nTabsCaptured = 2;

// Util

function compareTabs(tabA, tabB) {
  assertEq(tabA.id, tabB.id);
  assertEq(tabA.url, tabB.url);
  assertEq(tabA.width, tabB.width);
  assertEq(tabA.height, tabB.height);
}

function sortTab(tabA, tabB) {
  return tabA.id - tabB.id;
}

function verifyTabDoesNotExist(tabId) {
  chrome.experimental.offscreenTabs.
      get(tabId, fail("No offscreen tab with id: " + tabId + "."));
}

// Tab Management (create, get, getAll, update, remove) ------------------------

function startTabManagement() {
  var nCallbacksNotDone = inTabs.length;

  for (var i=0; i<inTabs.length; i++) {
    chrome.experimental.offscreenTabs.create(
      {
        "url": inTabs[i].url,
        "width": inTabs[i].width,
        "height": inTabs[i].height
      },
      pass(function() {
        var j = i;
        return function(tab) {
          tabs[j] = tab;

          nCallbacksNotDone--;

          if (nCallbacksNotDone == 0)
             getAll();
        }
      }())
    );
  }
}

function getAll() {
  chrome.experimental.offscreenTabs.getAll(pass(function(tabsResult) {
    assertEq(tabs.length, tabsResult.length);

    tabs.sort(sortTab);
    tabsResult.sort(sortTab);

    for (var i=0; i<tabs.length; i++)
      compareTabs(tabs[i], tabsResult[i]);

    get();
  }));
}

function get() {
  var comparedTab = tabs[0];

  chrome.experimental.offscreenTabs.get(comparedTab.id, pass(function(tab) {
    compareTabs(comparedTab, tab);

    update();
  }));
}

function update() {
  var replicatedTab = tabs[0];

  chrome.experimental.offscreenTabs.update(tabs[1].id,
    {
      "url": replicatedTab.url,
      "width": replicatedTab.width,
      "height": replicatedTab.height
    },
    pass(function(tab) {
      assertEq(replicatedTab.url, tab.url);
      assertEq(replicatedTab.width, tab.width);
      assertEq(replicatedTab.height, tab.height);

      remove();
    })
  );
}

function remove() {
  for (var i=0; i<nTabsCaptured; i++) {
    chrome.experimental.offscreenTabs.remove(tabs[i].id);
    verifyTabDoesNotExist(tabs[i].id);
  }
}

// Mouse (sendMouseEvent) ------------------------------------------------------

function startMouseEvents() {
  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function (tabId, changeInfo, tab) {
      chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

      assertEq(inTabs[0].url, changeInfo.url);
      assertEq(inTabs[0].url, tab.url);
      assertEq(inTabs[0].width, tab.width);
      assertEq(inTabs[0].height, tab.height);

      tabMouse = tab;

      mouseClick();
  });

  chrome.experimental.offscreenTabs.create(
    {
      "url": inTabs[0].url,
      "width": inTabs[0].width,
      "height": inTabs[0].height
    });
}

function mouseClick() {
  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function(tabId, changeInfo, tab) {
      chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

      assertEq(tabMouse.id, tabId);
      assertEq(tabMouse.id, tab.id);
      assertEq(inTabs[1].url, changeInfo.url);
      assertEq(inTabs[1].url, tab.url);
      assertEq(tabMouse.width, tab.width);
      assertEq(tabMouse.height, tab.height);

      mouseWheel();
  });

  mouseEvent.type = "click";
  chrome.experimental.offscreenTabs.
      sendMouseEvent(tabMouse.id, mouseEvent, x, y);
}

function mouseWheel() {
  mouseEvent.type = "mousewheel";
  mouseEvent.wheelDeltaX = 0;
  mouseEvent.wheelDeltaY = -100;
  chrome.experimental.offscreenTabs.
      sendMouseEvent(tabMouse.id, mouseEvent, 0, 0, function(tab) {
        mouseDownUp();
      }
  );
}

function mouseDownUp() {
  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function(tabId, changeInfo, tab) {
      chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

      assertEq(inTabs[2].url, tab.url);

      chrome.experimental.offscreenTabs.remove(tabMouse.id);
      verifyTabDoesNotExist(tabMouse.id);
  });

  mouseEvent.type = "mousedown";
  chrome.experimental.offscreenTabs.
      sendMouseEvent(tabMouse.id, mouseEvent, x, y);

  mouseEvent.type = "mouseup";
  chrome.experimental.offscreenTabs.
      sendMouseEvent(tabMouse.id, mouseEvent, x, y);
}

// Keyboard (sendKeyboardEvent) ------------------------------------------------

function startKeyboardEvents() {
  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function(tabId, changeInfo, tab) {
      chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

      tabKeyboard = tab;

      keyPress();
  });

  chrome.experimental.offscreenTabs.create(
    {
      "url": inTabs[0].url,
      "width": inTabs[0].width,
      "height": inTabs[0].height
    }
  );
}

function keyPress() {
  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function(tabId, changeInfo, tab) {
      chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

      assertEq(inTabs[1].url, tab.url);

      chrome.experimental.offscreenTabs.remove(tabKeyboard.id);
      verifyTabDoesNotExist(tabKeyboard.id);
  });

  var keyboardEvent = {
    "type": "keypress",
    "charCode": 113,  // q
    "keyCode": 113,
    "altKey": false,
    "ctrlKey": false,
    "shiftKey": false
  };

  chrome.experimental.offscreenTabs.
      sendKeyboardEvent(tabKeyboard.id, keyboardEvent);
}


// Display (toDataUrl) ---------------------------------------------------------

// In order to test that we don't get empty images back we can compare two
// images that are supposed to be different. We only need to make sure the two
// offscreen tabs have the same size (i.e. inTabs[0] and inTabs[1])
function startDisplay() {
  var nCallbacksNotDone = nTabsCaptured;

  chrome.experimental.offscreenTabs.onUpdated.addListener(listener =
    function (tabId, changeInfo, tab) {
      tabsCaptured[nTabsCaptured - nCallbacksNotDone] = tab;

      nCallbacksNotDone--;

      if (nCallbacksNotDone == 0) {
        chrome.experimental.offscreenTabs.onUpdated.removeListener(listener);

        toDataUrl();
      }
  });

  for (var i=0; i<nTabsCaptured; i++) {
    chrome.experimental.offscreenTabs.create(
      {
        "url": inTabs[i].url,
        "width": inTabs[i].width,
        "height": inTabs[i].height
      }
    );
  }
}

function toDataUrl() {
  var nCallbacksNotDone = nTabsCaptured;

  for (var i=0; i<nTabsCaptured; i++) {
    chrome.experimental.offscreenTabs.toDataUrl(
      tabsCaptured[i].id,
      {"format": "png"},
      function(dataUrl) {
        var j = i;
        return pass(function(dataUrl) {
          assertEq('string', typeof(dataUrl));
          assertEq('data:image/png;base64,', dataUrl.substr(0,22));

          tabsCaptured[j].dataUrl = dataUrl;

          nCallbacksNotDone--;

          if (nCallbacksNotDone == 0) {
            // Compare the dataUrls
            assertTrue(tabsCaptured[0].dataUrl != tabsCaptured[1].dataUrl);

            for (var i=0; i<nTabsCaptured; i++) {
              chrome.experimental.offscreenTabs.remove(tabsCaptured[i].id);
              verifyTabDoesNotExist(tabsCaptured[i].id);
            }
          }
        })
      }()
    );
  }
}

// Run tests  ------------------------------------------------------------------

chrome.test.runTests([
  // Tab Management (create, get, getAll, update, remove)
  function tabManagement() {
    startTabManagement();
  },

  // Mouse (sendMouseEvent)
  function mouseEvents() {
    startMouseEvents();
  },

  // Keyboard (sendKeyboardEvent)
  function keyboardEvents() {
    startKeyboardEvents();
  },

  // Display (toDataUrl)
  function display() {
    startDisplay();
  }
]);

