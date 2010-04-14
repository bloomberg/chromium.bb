// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTab

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWidth = 400;
var kHeight = 400;

function pageUrl(base) {
  return chrome.extension.getURL(base + ".html");
}

function testAllPixelsAreExpectedColor(imgUrl, color) {
  assertEq("string", typeof(imgUrl));
  var img = new Image();
  img.width = kWidth;
  img.height = kHeight;
  img.src = imgUrl;
  img.onload = pass(function() {
    var canvas = document.createElement("canvas");

    // Comparing pixels is slow enough to hit timeouts.  Compare
    // a 10x10 region.
    canvas.setAttribute('width', 10);
    canvas.setAttribute('height', 10);
    var context = canvas.getContext('2d');
    context.drawImage(img, 0, 0, 10, 10);

    var imageData = context.getImageData(0, 0, 10, 10).data;

    for (var i = 0, n = imageData.length; i < n; i += 4) {
      assertEq(color[0], imageData[i+0]); // Red
      assertEq(color[1], imageData[i+1]); // Green
      assertEq(color[2], imageData[i+2]); // Blue
      assertEq(color[3], imageData[i+3]); // Alpha
    }
  });
}

// Globals used to allow a test to read data from a previous test.
var blackImageUrl;
var whiteImageUrl;

chrome.test.runTests([
  // Open a window with one tab, take a snapshot.
  function captureVisibleTabWhiteImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl("white")], {"width": kWidth, "height": kHeight},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq("string", typeof(imgDataUrl));
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
            whiteImageUrl = imgDataUrl;

            testAllPixelsAreExpectedColor(whiteImageUrl,
                                          [255, 255, 255, 255]);  // White.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabBlackImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl("black")], {"width": kWidth, "height": kHeight},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq("string", typeof(imgDataUrl));
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
            blackImageUrl = imgDataUrl;

            // Check that previous capture was done.
            assertEq('string', typeof(whiteImageUrl));

            assertTrue(whiteImageUrl != blackImageUrl);

            testAllPixelsAreExpectedColor(blackImageUrl,
                                          [0, 0, 0, 255]);  // Black.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabRedPng() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl("red")], {"width": kWidth, "height": kHeight},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId,
                                        {"format": "png"},
                                        pass(function(imgDataUrl) {
            // The URL should be a data URL with has a PNG mime type.
            assertEq("string", typeof(imgDataUrl));
            assertEq('data:image/png;base64,', imgDataUrl.substr(0,22));

            testAllPixelsAreExpectedColor(imgDataUrl,
                                          [255, 0, 0, 255]);  // Red.
          }));
        }));
      }));
    }));
  }
]);
