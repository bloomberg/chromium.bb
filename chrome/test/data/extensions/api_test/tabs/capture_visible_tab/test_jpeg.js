// API test for chrome.tabs.captureVisibleTab(), capturing JPEG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabJpeg

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWindowRect = {
  'width': 400,
  'height': 400
};

// Globals used to allow a test to read data from a previous test.
var blackImageUrl;
var whiteImageUrl;

chrome.test.runTests([
  // Open a window with one tab, take a snapshot.
  function captureVisibleTabWhiteImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('white')],
                 kWindowRect,
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
            whiteImageUrl = imgDataUrl;

            testPixelsAreExpectedColor(whiteImageUrl,
                                       kWindowRect,
                                       '255,255,255,255');  // White.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabBlackImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('black')],
               kWindowRect,
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
            blackImageUrl = imgDataUrl;

            // Check that previous capture was done.
            assertEq('string', typeof(whiteImageUrl));

            assertTrue(whiteImageUrl != blackImageUrl);

            testPixelsAreExpectedColor(blackImageUrl,
                                       kWindowRect,
                                       '0,0,0,255');  // Black.
          }));
        }));
      }));
    }));
  }
]);
