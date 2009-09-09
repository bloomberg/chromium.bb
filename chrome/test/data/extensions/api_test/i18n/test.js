// i18n api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.I18N

var testCallback = chrome.test.testCallback;

chrome.test.runTests([
  function getAcceptLanguages() {
    chrome.i18n.getAcceptLanguages(chrome.test.callbackPass(function(results) {
      chrome.test.assertEq(results.length, 2);
      chrome.test.assertEq(results[0], "en-US");
      chrome.test.assertEq(results[1], "en");
    }));
  }
]);
