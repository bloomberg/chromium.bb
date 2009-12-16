// Send the variable defined by a.js and modified by b.js back to the extension.
chrome.extension.connect().postMessage(num);
