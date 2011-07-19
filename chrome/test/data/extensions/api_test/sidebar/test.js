// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.experimental.sidebar.
// browser_tests.exe --gtest_filter=SidebarApiTest.Sidebar

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const pass = chrome.test.callbackPass;

if (!chrome.sidebar) {
  chrome.sidebar = chrome.experimental.sidebar;
}

/**
* A helper function to show sidebar. Verifies that sidebar was hidden before
* and is shown after the call.
* @param {id} tab id to expand sidebar for.
* @param {function} callback Closure.
*/
function showSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    assertEq('hidden', state);
    chrome.sidebar.show({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      assertEq('shown', state);
      callback();
    });
  });
}

/**
* A helper function to expand sidebar. Verifies that sidebar was shown
* before and is expanded after the call (provided the specified tab
* is currently selected).
* @param {id} tab id to expand sidebar for.
* @param {function} callback Closure.
*/
function expandSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    assertEq('shown', state);
    chrome.sidebar.expand({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      if (undefined == id) {
        assertEq('active', state);
        callback();
      } else {
        chrome.tabs.get(id, function(tab) {
          if (tab.selected) {
            assertEq('active', state);
          } else {
            assertEq('shown', state);
          }
          callback();
        });
      }
    });
  });
}

/**
* A helper function to collapse sidebar. Verifies that sidebar was active
* before (provided the specified tab is currently selected) and is not active
* after the call.
* @param {id} tab id to collapse sidebar for.
* @param {function} callback Closure.
*/
function collapseSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    if (undefined == id) {
      assertEq('active', state);
    } else {
      chrome.tabs.get(id, function(tab) {
        if (tab.selected) {
          assertEq('active', state);
        } else {
          assertEq('shown', state);
        }
      });
    }
    chrome.sidebar.collapse({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      assertEq('shown', state);
      callback();
    });
  });
}

/**
* A helper function to hide sidebar. Verifies that sidebar was not hidden
* before and is hidden after the call.
* @param {id} tab id to hide sidebar for.
* @param {function} callback Closure.
*/
function hideSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    assertTrue('hidden' != state);
    chrome.sidebar.hide({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      assertEq('hidden', state);
      callback();
    });
  });
}

/**
* A helper function to show sidebar for the current tab.
* @param {function} callback Closure.
*/
function showSidebarForCurrentTab(callback) {
  showSidebar(undefined, callback);
}

/**
* A helper function to expand sidebar for the current tab.
* @param {function} callback Closure.
*/
function expandSidebarForCurrentTab(callback) {
  expandSidebar(undefined, callback);
}

/**
* A helper function to collapse sidebar for the current tab.
* @param {function} callback Closure.
*/
function collapseSidebarForCurrentTab(callback) {
  collapseSidebar(undefined, callback);
}

/**
* A helper function to hide sidebar for the current tab.
* @param {function} callback Closure.
*/
function hideSidebarForCurrentTab(callback) {
  hideSidebar(undefined, callback);
}

var tests = [
  function showHideSidebar() {
    showSidebarForCurrentTab(function() {
      expandSidebarForCurrentTab(function() {
        collapseSidebarForCurrentTab(function() {
          hideSidebarForCurrentTab(function() {
            showSidebarForCurrentTab(function() {
              hideSidebarForCurrentTab(pass());
            });
          });
        });
      });
    });
  },

  function switchingTabs() {
    showSidebarForCurrentTab(function() {
      expandSidebarForCurrentTab(function() {
        chrome.tabs.getSelected(null, function(tabWithSidebar) {
          chrome.tabs.create({}, function(tab) {
            // Make sure sidebar is not visible on this new tab.
            chrome.sidebar.getState({}, function(state) {
              assertEq('hidden', state);
              // Switch back to the tab with the sidebar.
              chrome.tabs.update(tabWithSidebar.id, {selected: true},
                                 function(theSameTab) {
                // It makes sure sidebar is visible before hiding it.
                hideSidebarForCurrentTab(pass());
              });
            });
          });
        });
      });
    });
  },

  function sidebarOnInactiveTab() {
    // 'switchingTabs' test created two tabs.
    chrome.tabs.getAllInWindow(null, function(tabs) {
      assertEq(2, tabs.length);
      showSidebar(tabs[1].id, function() {
        expandSidebar(tabs[1].id, function() {
          // Make sure sidebar is not visible on the current tab.
          chrome.sidebar.getState({}, function(state) {
            assertEq('hidden', state);
            // Switch to the tab with the sidebar.
            chrome.tabs.update(tabs[1].id, {selected: true},
                               function() {
              // Make sure sidebar is visible on the current tab.
              chrome.sidebar.getState({}, function(state) {
                assertEq('active', state);

                // Switch to the tab with no sidebar.
                chrome.tabs.update(tabs[0].id, {selected: true},
                                   function() {
                  collapseSidebar(tabs[1].id, function() {
                    hideSidebar(tabs[1].id, pass());
                  });
                });
              });
            });
          });
        });
      });
    });
  },

  function navigateSidebar() {
    showSidebarForCurrentTab(function() {
      expandSidebarForCurrentTab(function() {
        chrome.sidebar.navigate({path: 'simple_page.html'});
        hideSidebarForCurrentTab(pass());
      });
    });
  },

  function crashTest() {
    // Chrome should not crash on this request.
    chrome.sidebar.getState(undefined, function(state) {
      assertEq('hidden', state);
      chrome.sidebar.getState(null, function(state) {
        assertEq('hidden', state);
        // Also check that it really does return state of the current tab.
        showSidebarForCurrentTab(function() {
          chrome.sidebar.getState(undefined, function(state) {
            assertEq('shown', state);
            chrome.sidebar.getState(null, function(state) {
              assertEq('shown', state);
              hideSidebarForCurrentTab(pass());
            });
          });
        });
      });
    });
  },

  function testSetFunctions() {
    showSidebarForCurrentTab(function() {
      // TODO(alekseys): test unicode strings.
      chrome.sidebar.setBadgeText({text: 'Some random text'});
      chrome.sidebar.setIcon({path: 'icon.png'});
      chrome.sidebar.setTitle({title: 'Some random title'});
      hideSidebarForCurrentTab(pass());
    });
  }
];

chrome.test.runTests(tests);
