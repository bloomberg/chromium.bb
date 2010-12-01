var message = "failed";
try {
  importScripts("REPLACE_WITH_IMPORTED_JS_URL");
} catch(ex) {
}
postMessage(message);
