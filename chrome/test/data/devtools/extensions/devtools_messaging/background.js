function onRequest(request, sender, callback) {
  var response = JSON.stringify(request);
  callback("onRequest callback: " + response);
}

chrome.extension.onRequest.addListener(onRequest);

function onMessage(port, message) {
  port.postMessage("port.onMessage: " + message);
}

function onConnect(port) {
  port.onMessage.addListener(onMessage.bind(null, port));
}

chrome.extension.onConnect.addListener(onConnect);
