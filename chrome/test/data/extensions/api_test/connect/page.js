// For test onRequest.
chrome.extension.sendRequest({step: 1}, function(response) {
  if (response.nextStep)
    chrome.extension.sendRequest({step: 2});
});

// For test sendRequest.
chrome.extension.onRequest.addListener(function(request, sendResponse) {
  sendResponse({success: (request.step2 == 1)});
});
