var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;

var GOOGLE_URL = 'http://www.google.com/';
var PICASA_URL = 'http://www.picasa.com/';

// PORT will be changed to the port of the test server.
var A_RELATIVE_URL =
    'http://www.a.com:PORT/files/extensions/api_test/history/a.html';
var B_RELATIVE_URL =
    'http://www.b.com:PORT/files/extensions/api_test/history/b.html';

/**
 * A helper function to flip the setTimeout arguments and make the code
 * more readable.
 * @param {number} seconds The number of seconds to wait.
 * @param {function} callback Closure.
 */
function waitAFewSeconds(seconds, callback) {
  setTimeout(callback, seconds * 1000);
};

/**
 * Object used for listening to the chrome.history.onVisited events.  The
 * global object 'itemVisited' stores the last item received.
 */
var itemVisitedCallback = null;
function itemVisitedListener(visited) {
  if (null != itemVisitedCallback) {
    itemVisitedCallback(visited);
  };
};

function removeItemVisitedListener() {
  chrome.history.onVisited.removeListener(itemVisitedListener);
  itemVisitedCallback = null;
}

function setItemVisitedListener(callback) {
  chrome.history.onVisited.addListener(itemVisitedListener);
  itemVisitedCallback = callback;
}

function setNextItemVisitedListener(callback) {
  itemVisitedCallback = callback;
}

/**
 * An object used for listening to the chrome.history.onVisitRemoved events.
 * The global object 'itemRemovedInfo' stores the information from the last
 * callback.
 */
var itemRemovedCallback = null;
function itemRemovedListener(removed) {
  if (null != itemRemovedCallback) {
    itemRemovedCallback(removed);
  };
};

function removeItemRemovedListener() {
  chrome.history.onVisited.removeListener(itemRemovedListener);
  itemRemovedCallback = null;
}

function setItemRemovedListener(callback) {
  chrome.history.onVisitRemoved.addListener(itemRemovedListener);
  itemRemovedCallback = callback;
}

function setNextItemRemovedListener(callback) {
  itemRemovedCallback = callback;
}

/**
 * An object used for listening to the chrome.history.onVisitRemoved events.
 * Set 'tabCompleteCallback' to a function to add extra processing to the
 * callback.  The global object 'tabsCompleteData' contains a list of the
 * last known state of every tab.
 */
var tabCompleteCallback = null;
var tabsCompleteData = {};
function tabsCompleteListener(tabId, changeInfo) {
  if (changeInfo && changeInfo.status) {
    tabsCompleteData[tabId] = changeInfo.status;
  };
  if (null != tabCompleteCallback) {
    tabCompleteCallback();
  };
};

/**
 * Queries the entire history for items, calling the closure with an argument
 * specifying the the number of items in the query.
 * @param {function(number)} callback The closure.
 */
function countItemsInHistory(callback) {
  var query = {'text': ''};
  chrome.history.search(query, function(results) {
    callback(results.length);
  });
}

/**
 * Populates the history by calling addUrl for each url in the array urls.
 * @param {Array.<string>} urls The array of urls to populate the history.
 * @param {function} callback Closure.
 */
function populateHistory(urls, callback) {
  urls.forEach(function(url) {
    chrome.history.addUrl({ 'url': url });
  });
  callback();
}

/**
 * Tests call this function to invoke specific tests.
 * @param {Array.<funcion>} testFns The tests to run.
 */
function runHistoryTestFns(testFns) {
  chrome.test.getConfig(function(config) {
    var fixPort = function(url) {
      return url.replace(/PORT/, config.testServer.port);
    };
    A_RELATIVE_URL = fixPort(A_RELATIVE_URL);
    B_RELATIVE_URL = fixPort(B_RELATIVE_URL);

    chrome.test.runTests(testFns);
  });
}
