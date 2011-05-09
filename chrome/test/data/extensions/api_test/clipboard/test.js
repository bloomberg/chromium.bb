// Clipboard permission test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Clipboard

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function domCopy() {
      document.body.addEventListener('copy', chrome.test.callbackPass());
      document.execCommand('copy');
    },
    function domPaste() {
      document.body.addEventListener('paste', chrome.test.callbackPass());
      document.execCommand('paste');
    }
  ]);
});
