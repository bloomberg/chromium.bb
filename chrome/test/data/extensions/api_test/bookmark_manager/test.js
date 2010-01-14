// Bookmark Manager API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.BookmarkManager

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;

chrome.test.runTests([
  function getStrings() {
    chrome.experimental.bookmarkManager.getStrings(pass(function(strings) {
      assertEq('string', typeof strings['title']);
      assertEq('string', typeof strings['search_button']);
    }));
  }
]);
