// find map on demand

console.log("mappy_content_script.js loaded");

var maps_key = "ABQIAAAATfHumDbW3OmRByfquHd3SRTRERdeAiwZ9EeJWta3L_JZVS0bOBRQeZgr4K0xyVKzUdnnuFl8X9PX0w";

chrome.extension.onConnect.addListener(function(port) {
  //console.log("extension connected");
  port.onMessage.addListener(function(data) {
    //console.log("extension sent message");
    findAddress(port);
  });
});

var findAddress = function(port) {
  var found;
  var re = /(\d+\s+[':.,\s\w]*,\s*[A-Za-z]+\s*\d{5}(-\d{4})?)/m;
  var node = document.body;
  var done = false;
  while (!done) {
    done = true;
    for (var i = 0; i < node.childNodes.length; ++i) {
      var child = node.childNodes[i];
      if (child.textContent.match(re)) {
        node = child;
        found = node;
        done = false;
        break;
      }
    }
  }
  if (found) {
    var text = "";
    if (found.childNodes.length) {
      for (var i = 0; i < found.childNodes.length; ++i) {
        text += found.childNodes[i].textContent + " ";
      }
    } else {
      text = found.textContent;
    }
    var match = re.exec(text);
    if (match && match.length) {
      console.log("found: " + match[0]);
      var trim = /\s{2,}/g;
      var map = match[0].replace(trim, " ");
      port.postMessage({message:"map", values:[map]});
    } else {
      console.log("found bad " + found.textContent);
      console.log("no match in: " + text);
    }
  } else {
    console.log("no match in " + node.textContent);
  }
}

