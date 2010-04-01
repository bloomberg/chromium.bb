// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTab

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
}

chrome.test.runTests([
  // Open a window with one tab, take a snapshot.
  function captureVisibleTabSimple() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl("a")], {"width": 300, "height": 150},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
          }));
        }));
      }));
    }));
  },

  // Open a window with three tabs, take a snapshot of each.
  function captureVisibleTabMultiTab() {
    var snapshotAndRemoveSelectedTab = function(winId, callback) {
      chrome.tabs.getSelected(winId, function(tab) {
        chrome.tabs.captureVisibleTab(winId, function(imgDataUrl) {
          // Test that the URL we got is a data URL which encodes a JPEG image.
          assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));

          // TODO(skerner): Once an option allows captureVisibleTab to
          // take a lossless snapshot with a set color depth, use
          // a canvas to compare |imgDataUrl| to an image of the tab
          // we expect.  This can't be done with JPEG, as the results
          // vary based on the display settings.
          chrome.tabs.remove(tab.id, callback);
        });
      });
    };

    createWindow(["a", "b", "c"].map(pageUrl), {"width": 300, "height": 150},
      function(winId, tabIds){
        waitForAllTabs(pass(function() {
          snapshotAndRemoveSelectedTab(winId, pass(function() {
            snapshotAndRemoveSelectedTab(winId, pass(function() {
              snapshotAndRemoveSelectedTab(winId, pass(function() {}));
            }));
          }));
        }));
      });
  }
]);
