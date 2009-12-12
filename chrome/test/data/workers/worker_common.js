if (!self.postMessage) {
  // This is a shared worker - mimic dedicated worker APIs
  onconnect = function(event) {
    event.ports[0].onmessage = function(e) {
      self.onmessage(e);
    };
    self.postMessage = function(msg, ports) {
      event.ports[0].postMessage(msg, ports);
    };
  };
}
onmessage = function(evt) {
  if (evt.data == "ping")
    postMessage("pong");
  else if (/eval.+/.test(evt.data)) {
    try {
      postMessage(eval(evt.data.substr(5)));
    } catch (ex) {
      postMessage(ex);
    }
  }
}
