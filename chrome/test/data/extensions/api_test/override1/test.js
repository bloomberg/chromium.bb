chrome.test.runTests([
  function newtab() {
    chrome.tabs.create({"url": "chrome://newtab/"}, 
                       chrome.test.testCallback(function(response) {
    }));
  }
]);

