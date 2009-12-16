// We should not be able to read the "num" variable which was defined in a.js
// from the "isolated world 1" extension.
chrome.extension.connect().postMessage(typeof num == "undefined");
