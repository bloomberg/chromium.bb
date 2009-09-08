chrome.test.runTests([
  function newtab() {
    chrome.tabs.create({"url": "chrome://newtab/"}, 
                       chrome.test.callbackPass(function(response) {}));
  }
]);

