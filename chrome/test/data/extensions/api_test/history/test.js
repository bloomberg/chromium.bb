// History api test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.History

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;

var GOOGLE_URL = 'http://www.google.com/';
var PICASA_URL = 'http://www.picasa.com/';
var A_RELATIVE_URL =
    'http://www.a.com:1337/files/extensions/api_test/history/a.html';
var B_RELATIVE_URL =
    'http://www.b.com:1337/files/extensions/api_test/history/b.html';

/**
* A helper function to flip the setTimeout arguments and make the code more
* readable.
* @param {number} seconds The number of seconds to wait.
* @param {function} callback Closure.
*/
function waitAFewSeconds(seconds, callback) {
  setTimeout(callback, seconds * 1000);
};

/**
* Object used for listening to the chrome.history.onVisited events.  The global
* object 'itemVisited' stores the last item received.
*/
var itemVisitedCallback = null;
function itemVisitedListener(visited) {
  if (null != itemVisitedCallback) {
    itemVisitedCallback(visited);
  };
};

function removeItemVisitedListener() {
  chrome.experimental.history.onVisited.removeListener(itemVisitedListener);
  itemVisitedCallback = null;
}

function setItemVisitedListener(callback) {
  chrome.experimental.history.onVisited.addListener(itemVisitedListener);
  itemVisitedCallback = callback;
}

function setNextItemVisitedListener(callback) {
  itemVisitedCallback = callback;
}

/**
* An object used for listening to the chrome.history.onVisitRemoved events. The
* global object 'itemRemovedInfo' stores the information from the last callback.
*/
var itemRemovedCallback = null;
function itemRemovedListener(removed) {
  if (null != itemRemovedCallback) {
    itemRemovedCallback(removed);
  };
};

function removeItemRemovedListener() {
  chrome.experimental.history.onVisited.removeListener(itemRemovedListener);
  itemRemovedCallback = null;
}

function setItemRemovedListener(callback) {
  chrome.experimental.history.onVisitRemoved.addListener(itemRemovedListener);
  itemRemovedCallback = callback;
}

function setNextItemRemovedListener(callback) {
  itemRemovedCallback = callback;
}

/**
* An object used for listening to the chrome.history.onVisitRemoved events.  Set
* 'tabCompleteCallback' to a function to add extra processing to the callback.
* The global object 'tabsCompleteData' contains a list of the last known state
* of every tab.
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
  var query = {'search': ''};
  chrome.experimental.history.search(query, function(results) {
    callback(results.length);
  });
};

/**
* Populates the history by calling addUrl for each url in the array urls.
* @param {Array.<string>} urls The array of urls to populate the history.
* @param {function} callback Closure.
*/
function populateHistory(urls, callback) {
  urls.forEach(function(url) {
    chrome.experimental.history.addUrl({ 'url': url });
  });
  callback();
};

chrome.test.runTests([
  // All the tests require a blank state to start from.  This test is run
  // first to insure that state can be acheived.
  function clearHistory() {
    chrome.experimental.history.deleteAll(pass(function() {
      countItemsInHistory(pass(function(count) {
        assertEq(0, count);
      }));
    }));
  },

  function basicSearch() {
    // basicSearch callback.
    function basicSearchTestVerification() {
      removeItemVisitedListener();
      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    // basicSearch entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(basicSearchTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function timeScopedSearch() {
    var startDate = {};
    var endDate = {};

    function timeScopedSearchTestVerification() {
      removeItemVisitedListener();

      var query = { 'search': '',
                    'startTime': startDate.getTime(),
                    'endTime': endDate.getTime() };
       chrome.experimental.history.search(query, function(results) {
         assertEq(1, results.length);
         assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
       });
    };

    function onAddedItem() {
      // Set the next test callback.
      setNextItemVisitedListener(timeScopedSearchTestVerification);

      // Chrome has seconds resolution, so we must wait in order to search
      // a range.
      waitAFewSeconds(2, function() {
        endDate = new Date();
        endDate.setTime(endDate.getTime() - 500);
        populateHistory([PICASA_URL], function() { });
      });
    };

    // timeScopedSearch entry point.
    chrome.experimental.history.deleteAll(function() {
      // Set the test callback.
      setItemVisitedListener(onAddedItem);
      // Set the start time a few seconds in the past.
      startDate = new Date();
      startDate.setTime(startDate.getTime() - 1000);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  // Give time epochs x,y,z and history events A,B which occur in the sequence
  // x A y B z, this test scopes the search to the interval [y,z] to test that
  // [x,y) is excluded.  The previous test scoped to the interval [x,y].
  function timeScopedSearch2() {
    var startDate = {};
    var endDate = {};

    function timeScopedSearch2TestVerification() {
      removeItemVisitedListener();

      endDate = new Date();
      endDate.setTime(endDate.getTime() + 1000);
      var query = { 'search': '',
                    'startTime': startDate.getTime(),
                    'endTime': endDate.getTime() };
       chrome.experimental.history.search(query, function(results) {
         assertEq(1, results.length);
         assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
       });
    };

    function onAddedItem() {
      // Set the next test callback.
      setNextItemVisitedListener(timeScopedSearch2TestVerification);

      // Chrome has seconds resolution, so we must wait in order to search
      // a range.
      waitAFewSeconds(2, function() {
        startDate = new Date();
        startDate.setTime(startDate.getTime() - 1000);
        populateHistory([PICASA_URL], function() { });
      });
    };

    // timeScopedSearch entry point.
    chrome.experimental.history.deleteAll(function() {
      // Set the test callback.
      setItemVisitedListener(onAddedItem);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function lengthScopedSearch() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var urlsAdded = 0;

    function lengthScopedSearchTestVerification() {
      // Ensure all urls have been added.
      urlsAdded += 1;
      if (urlsAdded < urls.length)
        return;

      removeItemVisitedListener();

      var query = { 'search': '', 'maxResults': 1 };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    // lengthScopedSearch entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(lengthScopedSearchTestVerification);
      populateHistory(urls, function() { });
    });
  },

  function fullTextSearch() {
    chrome.experimental.history.deleteAll(function() {
      // The continuation of the test after the windows have been opened.
      var validateTest = function() {
        // Continue with the test.
        // A title search for www.a.com should find a.
        var query = { 'search': 'www.a.com' };
        chrome.experimental.history.search(query, function(results) {
          assertEq(1, results.length);
          assertEq(A_RELATIVE_URL, results[0].url);

          // Text in the body of b.html.
          query = { 'search': 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' };
          chrome.experimental.history.search(query, function(results) {
            assertEq(1, results.length);
            assertEq(B_RELATIVE_URL, results[0].url);

            // The test has succeeded.
            chrome.test.succeed();
          });
        });
      };

      // Setup a callback object for tab events.
      var urls = [A_RELATIVE_URL, B_RELATIVE_URL];
      var tabIds = [];

      function listenerCallback() {
        if (tabIds.length < urls.length) {
          return;
        };

        // Ensure both tabs have completed loading.
        for (var index = 0, id; id = tabIds[index]; index++) {
          if (!tabsCompleteData[id] ||
          tabsCompleteData[id] != 'complete') {
            return;
          };
        };

        // Unhook callbacks.
        tabCompleteCallback = null;
        chrome.tabs.onUpdated.removeListener(tabsCompleteListener);

        // Allow indexing to occur.
        waitAFewSeconds(2, function() {
          validateTest();
        });
      };

      tabCompleteCallback = listenerCallback;
      chrome.tabs.onUpdated.addListener(tabsCompleteListener);

      // Navigate to a few pages.
      urls.forEach(function(url) {
        chrome.tabs.create({ 'url': url }, function(tab) {
          tabIds.push(tab.id);
        });
      });
    });
  },

  function getVisits() {
    // getVisits callback.
    function getVisitsTestVerification() {
      removeItemVisitedListener();

      // Verify that we received the url.
      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        var id = results[0].id;
        chrome.experimental.history.getVisits({ 'url': GOOGLE_URL }, function(results) {
          assertEq(1, results.length);
          assertEq(id, results[0].id);

          // The test has succeeded.
          chrome.test.succeed();
        });
      });
    };

    // getVisits entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(getVisitsTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function deleteUrl() {
    function deleteUrlTestVerification() {
      removeItemRemovedListener();

      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(0, results.length);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    function onAddedItem() {
      removeItemVisitedListener();

      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        chrome.experimental.history.deleteUrl({ 'url': GOOGLE_URL });
      });
    };

    // deleteUrl entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteUrlTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function deleteRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var startDate = {};
    var endDate = {};
    var itemsAdded = 0;

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    function onAddedItem() {
      itemsAdded += 1;

      if (itemsAdded < urls.length) {
        // Chrome has seconds resolution, so we must wait to search a range.
        waitAFewSeconds(2, function() {
          endDate = new Date();
          endDate.setTime(endDate.getTime() - 1000);
          populateHistory([urls[itemsAdded]], function() { });
        });
        return;
      };

      removeItemVisitedListener();
      chrome.experimental.history.deleteRange({ 'startTime': startDate.getTime(),
                                   'endTime': endDate.getTime() },
                                  function() { });
    };

    // deletRange entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteRangeTestVerification);

      startDate = new Date();
      startDate.setTime(startDate.getTime() - 1000);

      populateHistory([urls[itemsAdded]], function() { });
    });
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur in the
  // sequence x A y B z.  The previous deleteRange test deleted the range [x,y],
  // this test deletes the range [y,z].
  function deleteRange2() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var startDate = {};
    var endDate = {};
    var itemsAdded = 0;

    function deleteRange2TestVerification() {
      removeItemRemovedListener();

      var query = { 'search': '' };
      chrome.experimental.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    function onAddedItem() {
      itemsAdded += 1;

      if (itemsAdded < urls.length) {
        // Chrome has seconds resolution, so we must wait to search a range.
        waitAFewSeconds(2, function() {
          startDate = new Date();
          startDate.setTime(startDate.getTime() - 1000);
          populateHistory([urls[itemsAdded]], function() { });
        });
        return;
      };

      removeItemVisitedListener();

      endDate = new Date();
      endDate.setTime(endDate.getTime() + 1000);
      chrome.experimental.history.deleteRange({ 'startTime': startDate.getTime(),
                                   'endTime': endDate.getTime() },
                                  function() { });
    };

    // deletRange entry point.
    chrome.experimental.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteRange2TestVerification);
      populateHistory([urls[itemsAdded]], function() { });
    });
  },
]);
