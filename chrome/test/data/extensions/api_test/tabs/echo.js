// Content script that echoes back all messages.
// Posting a message with "GET" returns the name and # of connections opened.
var connections = 0;

chrome.extension.onConnect.addListener(function onConnect(port) {
  connections++;
  port.onMessage.addListener(function onMessage(msg) {
    if (msg == "GET") {
      port.postMessage({"name": port.name, "connections": connections});
    } else {
      port.postMessage(msg);
    }
  });
});

chrome.extension.onRequest.addListener(function(request, sender, respond) {
  respond(request);
});