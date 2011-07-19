// Clipboard permission test for Chrome.
// browser_tests.exe --gtest_filter=ClipboardApiTest.Extension

chrome.test.runTests([
  function domCopy() {
    if (document.execCommand('copy'))
      chrome.test.succeed();
    else
      chrome.test.fail('execCommand("copy") failed');
  },
  function domPaste() {
    if (document.execCommand('paste'))
      chrome.test.succeed();
    else
      chrome.test.fail('execCommand("paste") failed');
  }
]);

