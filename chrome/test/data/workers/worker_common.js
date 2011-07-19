// Track the number of clients for this worker - tests can use this to ensure
// that shared workers are actually shared, not distinct.
var num_clients = 0;

if (!self.postMessage) {
  // This is a shared worker - mimic dedicated worker APIs
  onconnect = function(event) {
    num_clients++;
    event.ports[0].onmessage = function(e) {
      self.postMessage = function(msg) {
        event.ports[0].postMessage(msg);
      };
      self.onmessage(e);
    };
  };
} else {
  num_clients++;
}
onmessage = function(evt) {
  if (evt.data == "ping")
    postMessage("pong");
  else if (evt.data == "auth")
    importScripts("/auth-basic");
  else if (evt.data == "close")
    close();
  else if (/eval.+/.test(evt.data)) {
    try {
      postMessage(eval(evt.data.substr(5)));
    } catch (ex) {
      postMessage(ex);
    }
  }
}
