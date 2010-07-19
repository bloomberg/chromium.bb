// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTab

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWidth = 400;
var kHeight = 400;

function pageUrl(base) {
  return chrome.extension.getURL(base + '.html');
}

function getAllPixels(imgUrl, callbackFn) {
  assertEq('string', typeof(imgUrl));
  var img = new Image();
  img.width = kWidth;
  img.height = kHeight;
  img.src = imgUrl;
  img.onload = pass(function() {
    var canvas = document.createElement('canvas');

    // Comparing pixels is slow enough to hit timeouts.  Compare
    // a 10x10 region.
    canvas.setAttribute('width', 10);
    canvas.setAttribute('height', 10);
    var context = canvas.getContext('2d');
    context.drawImage(img, 0, 0, 10, 10);

    var imageData = context.getImageData(1, 1, 9, 9).data;

    var pixelColors = [];
    for (var i = 0, n = imageData.length; i < n; i += 4) {
      pixelColors.push([imageData[i+0],
                        imageData[i+1],
                        imageData[i+2],
                        imageData[i+3]].join(','));
    }

    callbackFn(pixelColors);
  });
}

function testAllPixelsAreExpectedColor(imgUrl, expectedColor) {
  getAllPixels(imgUrl, function(pixelColors) {
    var badPixels = [];
    for (var i = 0, ie = pixelColors.length; i < ie; ++i) {
      if (pixelColors[i] != expectedColor) {
        badPixels.push({'i': i,
                        'color': pixelColors[i]
                       });
      }
    }
    assertEq('[]', JSON.stringify(badPixels, null, 2));
  });
}

// Build a count of the number of times the colors in
// |expectedColors| occur in the image at |imgUrl|.
function countPixelsWithColors(imgUrl, expectedColors, callback) {
  colorCounts = new Array(expectedColors.length);
  for (var i = 0; i < expectedColors.length; ++i) {
    colorCounts[i] = 0;
  }

  getAllPixels(imgUrl, function(pixelColors) {
    for (var i = 0, ie = pixelColors.length; i < ie; ++i) {
      var colorIdx = expectedColors.indexOf(pixelColors[i]);
      if (colorIdx != -1)
        colorCounts[colorIdx]++;
    }
    callback(colorCounts,          // Mapping from color to # pixels.
             pixelColors.length);  // Total pixels examined.
  });
}

// Globals used to allow a test to read data from a previous test.
var blackImageUrl;
var whiteImageUrl;

chrome.test.runTests([
  // Open a window with one tab, take a snapshot.
  function captureVisibleTabWhiteImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('white')], {'width': kWidth, 'height': kHeight},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);  // waitForAllTabs ensures this.
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/jpg;base64,', imgDataUrl.substr(0,22));
            whiteImageUrl = imgDataUrl;

            testAllPixelsAreExpectedColor(whiteImageUrl,
                                          '255,255,255,255');  // White.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabBlackImage() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('black')], {'width': kWidth, 'height': kHeight},
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

            testAllPixelsAreExpectedColor(blackImageUrl,
                                          '0,0,0,255');  // Black.
          }));
        }));
      }));
    }));
  },

  function captureVisibleTabText() {
    // Keep the resulting image small by making the window small.
    createWindow([pageUrl('text')], {'width': kWidth, 'height': kHeight},
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

            countPixelsWithColors(
                imgDataUrl,
                ['255,255,255,255'],
                pass(function(colorCounts, totalPixelsChecked) {
                  // Some pixels should not be white, because the text
                  // is not white.  Can't test for black because
                  // antialiasing makes the pixels slightly different
                  // on each display setting.

                  // chrome.test.log(totalPixelsChecked);
                  // chrome.test.log(colorCounts[0]);
                  assertTrue(totalPixelsChecked != colorCounts[0]);
                }));
          }));
        }));
      }));
    }));
  }
]);
