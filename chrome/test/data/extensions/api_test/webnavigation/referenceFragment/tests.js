function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.getSelected(null, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Reference fragment navigation.
      function referenceFragment() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: "",
              transitionType: "link",
              url: getURL('a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html#anchor') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: "client_redirect",
              transitionType: "link",
              url: getURL('a.html#anchor') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html#anchor') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html#anchor') }]]);
        chrome.tabs.update(tabId, { url: getURL('a.html') });
      },
    ]);
  });
}
