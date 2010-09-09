var popupURL =
    "http://a.com:1337/files/extensions/api_test/window_open/popup_blocking/" +
    "extension/foo.html";

function pop() {
  window.open(popupURL);
  window.open(popupURL, "", "width=300,height=300");
}
