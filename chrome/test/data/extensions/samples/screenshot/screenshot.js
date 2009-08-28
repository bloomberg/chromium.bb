function takeScreenshot() {  
  chrome.tabs.captureVisibleTab(null, function(img) {
    var screenshotUrl = img;
    var viewTabUrl = chrome.extension.getURL('screenshot.html');

    chrome.tabs.create({url: viewTabUrl}, function(tab) {
      var targetId = tab.id;

      chrome.tabs.onUpdated.addListener(function(tabId, changedProps) {
        if (tabId == targetId &&
            changedProps.status == "complete") {
          var views = chrome.extension.getViews();
          for (var i = 0; i < views.length; i++) {
            var view = views[i];
            if (view.location.href == viewTabUrl) {
              view.setScreenshotUrl(screenshotUrl);
            }
          }
        }
      });
    })
  });
};