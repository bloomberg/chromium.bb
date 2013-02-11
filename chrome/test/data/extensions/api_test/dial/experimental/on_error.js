chrome.test.runTests([
  function noListeners() {
    chrome.test.listenOnce(chrome.dial.onError, function(error) {
      chrome.test.assertTrue(error.code == "no_listeners");
    });
    chrome.dial.discoverNow(function(result) {
      chrome.test.assertTrue(!result);
    });
  }
  // TODO(justinlin): Test other errors.
]);