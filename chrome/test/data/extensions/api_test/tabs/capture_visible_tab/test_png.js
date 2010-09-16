// API test for chrome.tabs.captureVisibleTab(), capturing PNG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabPng

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWindowRect = {
  'width': 400,
  'height': 400
};

var whiteImageUrl;
var textImageUrl;

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
          chrome.tabs.captureVisibleTab(winId,
                                        {'format': 'png'},
                                        pass(function(imgDataUrl) {
            // The URL should be a data URL with has a PNG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/png;base64,', imgDataUrl.substr(0,22));
            whiteImageUrl = imgDataUrl;

            testPixelsAreExpectedColor(whiteImageUrl,
                                       kWindowRect,
                                       '255,255,255,255');  // White.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabText() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('text')],
                 kWindowRect,
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId,
                                        {'format': 'png'},
                                        pass(function(imgDataUrl) {
            // The URL should be a data URL with has a PNG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/png;base64,', imgDataUrl.substr(0,22));
            textImageUrl = imgDataUrl;
            assertTrue(whiteImageUrl != textImageUrl);

            countPixelsWithColors(
                imgDataUrl,
                kWindowRect,
                ['255,255,255,255'],
                pass(function(colorCounts, totalPixelsChecked) {
                  // Some pixels should not be white, because the text
                  // is not white.  Can't test for black because
                  // antialiasing makes the pixels slightly different
                  // on each display setting.  Test that all pixels are
                  // not the same color.
                  assertTrue(totalPixelsChecked > colorCounts[0],
                             JSON.stringify({
                               message: 'Some pixels should not be white.',
                               totalPixelsChecked: totalPixelsChecked,
                               numWhitePixels: colorCounts[0]
                             }, null, 2));
                }));
            }));
        }));
      }));
    }));
  }
]);
