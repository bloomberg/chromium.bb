
chrome.management.onInstalled.addListener(function(extensionInfo) {
  if (!extensionInfo.isApp) {
    console.log("Can't launch " + extensionInfo.name + " (" +
                extensionInfo.id + "): Not an app.");
    return;
  }
  console.log("Launch " + extensionInfo.name + " (" +
              extensionInfo.id + ")");

  chrome.management.launchApp(extensionInfo.id, function() {
    chrome.test.sendMessage("launched app");
  });

});

chrome.test.sendMessage("launcher loaded");
