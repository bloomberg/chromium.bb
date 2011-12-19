function output(msg) {
  webInspector.console.addMessage(webInspector.console.Severity.Log, msg);
}

function test() {
  var expectedAPIs = [
    "console",
    "inspectedWindow",
    "network",
    "panels"
  ];

  for (var i = 0; i < expectedAPIs.length; ++i) {
    var api = expectedAPIs[i];
    if (typeof chrome.experimental.devtools[api] !== "object") {
      output("FAIL: API " + api + " is missing");
      return;
    }
  }
  if (typeof chrome.experimental.devtools.inspectedWindow.tabId !== "number") {
    output("FAIL: chrome.experimental.inspectedWindow.tabId is not a number");
    return;
  }
  output("PASS");
}

test();
