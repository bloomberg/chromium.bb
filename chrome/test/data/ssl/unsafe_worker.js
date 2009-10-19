var message = "failed";
try {
  importScripts("https://127.0.0.1:9666/files/ssl/imported.js");
} catch(ex) {
}
postMessage(message);
