function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.getSelected(null, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Navigates to a non-existant page.
      function nonExistant() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('nonexistant.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('nonexistant.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('nonexistant.html') });
      },

      // An page that tries to load an non-existant iframe.
      function nonExistantIframe() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('d.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('d.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('c.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('d.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('c.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('d.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('d.html') });
      },

      // An iframe navigates to a non-existant page.
      function nonExistantIframeNavigation() {
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
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('b.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('c.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('c.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('a.html') });
      },
    ]);
  });
}
