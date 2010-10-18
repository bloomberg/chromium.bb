// Clipboard API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Clipboard

const TEST_URL_TMPL = "http://localhost:PORT/files/extensions/test_file.html";

chrome.test.getConfig(function(config) {

  var TEST_URL = TEST_URL_TMPL.replace(/PORT/, config.testServer.port);

  function setupWindow(callback) {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.tabs.update(tab.id, {"url": TEST_URL}, callback);
    });
  }

  chrome.test.runTests([
    function executeCopy() {
      var onRequestCompleted = chrome.test.listenForever(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          if (request == "start test") {
            chrome.experimental.clipboard.executeCopy(
              sender.tab.id,
              chrome.test.callbackPass());
          } else if (request == "copy event") {
            onRequestCompleted();
          } else {
            chrome.test.fail("Unexpected request: " + JSON.stringify(request));
          }
        }
      );
      setupWindow();
    },
    function executeCut() {
      var onRequestCompleted = chrome.test.listenForever(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          if (request == "start test") {
            chrome.experimental.clipboard.executeCut(
              sender.tab.id,
              chrome.test.callbackPass());
          } else if (request == "cut event") {
            onRequestCompleted();
          } else {
            chrome.test.fail("Unexpected request: " + JSON.stringify(request));
          }
        }
      );
      setupWindow();
    },
    function executePaste() {
      var onRequestCompleted = chrome.test.listenForever(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          if (request == "start test") {
            chrome.experimental.clipboard.executePaste(
              sender.tab.id,
              chrome.test.callbackPass());
          } else if (request == "paste event") {
            onRequestCompleted();
          } else {
            chrome.test.fail("Unexpected request: " + JSON.stringify(request));
          }
        }
      );
      setupWindow();
    },
    function domPaste() {
      document.body.addEventListener('paste', chrome.test.callbackPass());
      document.execCommand('paste');
    }
  ]);
});
