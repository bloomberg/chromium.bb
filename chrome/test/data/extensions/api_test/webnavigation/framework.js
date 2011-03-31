var expectedEventData;
var capturedEventData;
var nextFrameId;
var frameIds;
var nextTabId;
var tabIds;

function getURL(relativeURL) {
  // Trim the filename from the current path to find the current directory.
  var currentDir = location.pathname.replace(/\/[^/]*$/, "");
  return chrome.extension.getURL(currentDir + "/" + relativeURL);
}

function start() {
  // Open a new tab to run the tests in, since the first tab hosts the
  // test page. Using blank.html instead of about:blank because of
  // http://crbug.com/77947
  chrome.tabs.create({url: "/blank.html"}, function(tab) {
    setupEvents();
    runTests(tab);
  });
}

function expect(data) {
  expectedEventData = data;
  capturedEventData = [];
  nextFrameId = 1;
  frameIds = {};
  nextTabId = 0;
  tabIds = {};
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  chrome.test.assertEq(JSON.stringify(expectedEventData),
      JSON.stringify(capturedEventData));
  chrome.test.succeed();
}

function captureEvent(name, details) {
  // normalize details.
  if (('frameId' in details) && (details.frameId != 0)) {
    if (frameIds[details.frameId] === undefined) {
      frameIds[details.frameId] = nextFrameId++;
    }
    details.frameId = frameIds[details.frameId];
  }
  if ('tabId' in details) {
    if (tabIds[details.tabId] === undefined) {
      tabIds[details.tabId] = nextTabId++;
    }
    details.tabId = tabIds[details.tabId];
  }
  if ('sourceTabId' in details) {
    if (tabIds[details.sourceTabId] === undefined) {
      tabIds[details.sourceTabId] = nextTabId++;
    }
    details.sourceTabId = tabIds[details.sourceTabId];
  }
  capturedEventData.push([name, details]);
  checkExpectations();
}

function setupEvents() {
  chrome.experimental.webNavigation.onBeforeNavigate.addListener(
    function(details) {
      captureEvent("onBeforeNavigate", details);
  });

  chrome.experimental.webNavigation.onCommitted.addListener(function(details) {
    captureEvent("onCommitted", details);
  });

  chrome.experimental.webNavigation.onDOMContentLoaded.addListener(
    function(details) {
      captureEvent("onDOMContentLoaded", details);
  });

  chrome.experimental.webNavigation.onCompleted.addListener(
    function(details) {
      captureEvent("onCompleted", details);
  });

  chrome.experimental.webNavigation.onBeforeRetarget.addListener(
    function(details) {
      captureEvent("onBeforeRetarget", details);
  });

  chrome.experimental.webNavigation.onErrorOccurred.addListener(
    function(details) {
      captureEvent("onErrorOccurred", details);
  });
}
