document.addEventListener("copy", function() {
  chrome.extension.sendRequest("copy event");
}, false);
document.addEventListener("cut", function() {
  chrome.extension.sendRequest("cut event");
}, false);
document.addEventListener("paste", function() {
  chrome.extension.sendRequest("paste event");
}, false);
chrome.extension.sendRequest("start test");
