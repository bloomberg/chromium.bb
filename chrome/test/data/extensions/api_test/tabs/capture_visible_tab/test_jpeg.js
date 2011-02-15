// API test for chrome.tabs.captureVisibleTab(), capturing JPEG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabJpeg

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWindowRect = {
  'width': 400,
  'height': 400
};

var kTestDir = '/files/extensions/api_test/tabs/capture_visible_tab/common/';
var kURLBaseA = 'http://a.com:PORT' + kTestDir;
var kURLBaseB = 'http://b.com:PORT' + kTestDir;

// Globals used to allow a test to read data from a previous test.
var blackImageUrl;
var whiteImageUrl;

chrome.test.getConfig(function(config) {
  var fixPort = function(url) {
    return url.replace(/PORT/, config.testServer.port);
  };

  chrome.test.runTests([
    // Open a window with one tab, take a snapshot.
    function captureVisibleTabWhiteImage() {
      // Keep the resulting image small by making the window small.
      createWindow([fixPort(kURLBaseA + 'white.html')],
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
      createWindow([fixPort(kURLBaseA + 'black.html')],
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
    },

    function captureVisibleTabNoPermissions() {
      var fail_url = fixPort(kURLBaseB + 'black.html');
      createWindow([fail_url], kWindowRect, pass(function(winId, tabIds) {
        waitForAllTabs(pass(function() {
          chrome.tabs.getSelected(winId, pass(function(tab) {
            assertEq('complete', tab.status);  // waitForAllTabs ensures this.
            chrome.tabs.captureVisibleTab(winId, fail(
              'Cannot access contents of url "' + fail_url +
              '". Extension manifest must request permission ' +
              'to access this host.'));
          }));
        }));
      }));
    }

  ]);
});

