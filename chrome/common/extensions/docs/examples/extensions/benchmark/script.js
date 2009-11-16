// The port for communicating back to the extension.
var benchmarkExtensionPort = chrome.extension.connect();

// The url is what this page is known to the benchmark as.
// The benchmark uses this id to differentiate the benchmark's
// results from random pages being browsed.

// TODO(mbelshe): If the page redirects, the location changed and the
// benchmark stalls.
var benchmarkExtensionUrl = window.location.toString();

function sendTimesToExtension() {
  if (window.parent != window) {
    return;
  }

  var times = window.chrome.loadTimes();

  // If the load is not finished yet, schedule a timer to check again in a
  // little bit.
  if (times.finishLoadTime != 0) {
    benchmarkExtensionPort.postMessage({message: "load", url: benchmarkExtensionUrl, values: times});
  } else {
    window.setTimeout(sendTimesToExtension, 100);
  }
}

// We can't use the onload event because this script runs at document idle,
// which may run after the onload has completed.
sendTimesToExtension();
