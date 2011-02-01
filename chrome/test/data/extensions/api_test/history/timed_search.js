// History api test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionHistoryApiTest.TimedSearch

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function timeScopedSearch() {
    var startDate = {};
    var endDate = {};

    function timeScopedSearchTestVerification() {
      removeItemVisitedListener();

      var query = { 'text': '',
                    'startTime': startDate.getTime(),
                    'endTime': endDate.getTime() };
       chrome.history.search(query, function(results) {
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
    chrome.history.deleteAll(function() {
      // Set the test callback.
      setItemVisitedListener(onAddedItem);
      // Set the start time a few seconds in the past.
      startDate = new Date();
      startDate.setTime(startDate.getTime() - 1000);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function searchWithIntegerTimes() {
    chrome.history.deleteAll(function() {
      // Search with an integer time range.
      var query = { 'text': '',
                    'startTime': 0,
                    'endTime': 12345678 };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);
        chrome.test.succeed();
      });
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
      var query = { 'text': '',
                    'startTime': startDate.getTime(),
                    'endTime': endDate.getTime() };
       chrome.history.search(query, function(results) {
         assertEq(1, results.length);
         assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

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
    chrome.history.deleteAll(function() {
      // Set the test callback.
      setItemVisitedListener(onAddedItem);
      populateHistory([GOOGLE_URL], function() { });
    });
  }
]);
