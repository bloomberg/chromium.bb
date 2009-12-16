// Utility functions to help with tabs/windows testing.

// Removes current windows and creates one window with tabs set to
// the urls in the array |tabUrls|. At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function setupWindow(tabUrls, callback) {
  createWindow(tabUrls, function(winId, tabIds) {
    // Remove all other windows.
    var removedCount = 0;
    chrome.windows.getAll({}, function(windows) {
      for (var i in windows) {
        if (windows[i].id != winId) {
          chrome.windows.remove(windows[i].id, function() {
            removedCount++;
            if (removedCount == windows.length - 1)
              callback(winId, tabIds);
          });
        }
      }
      if (windows.length == 1)
        callback(winId, tabIds);
    });
  });
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, callback) {
  chrome.windows.create({"url": tabUrls[0]}, function(win) {
    assertTrue(win.id > 0);
    var newTabIds = [];

    // Create tabs and populate newTabIds array.
    chrome.tabs.getSelected(win.id, function (tab) {
      newTabIds.push(tab.id);
      for (var i = 1; i < tabUrls.length; i++) {
        chrome.tabs.create({"windowId": win.id, "url": tabUrls[i]},
                           function(tab){
          newTabIds.push(tab.id);
          if (newTabIds.length == tabUrls.length)
            callback(win.id, newTabIds);
        });
      }
      if (tabUrls.length == 1)
        callback(win.id, newTabIds);
    });
  });
}

// Waits until all tabs (yes, in every window) have status "complete".
// This is useful to prevent test overlap when testing tab events.
// |callback| should look like function() {...}.
function waitForAllTabs(callback) {
  // Wait for all tabs to load.
  function waitForTabs(){
    chrome.windows.getAll({"populate": true}, function(windows) {
      var ready = true;
      for (var i in windows){
        for (var j in windows[i].tabs) {
          if (windows[i].tabs[j].status != "complete") {
            ready = false;
            break;
          }
        }
        if (!ready)
          break;
      }
      if (ready)
        callback();
      else
        window.setTimeout(waitForTabs, 30);
    });
  }
  waitForTabs();
}

