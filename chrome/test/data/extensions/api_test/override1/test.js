var tests = [
  function newtab() {
    chrome.tabs.create({"url": "chrome://newtab/"}, 
                       testFunction(function(response) {
      console.log("AFTER");
    }));
  }
];

runNextTest();

