// History api test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionHistoryApiTest.Delete

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  // All the tests require a blank state to start from.  If this fails,
  // expect all other history tests (ExtensionHistoryApiTest.*) to fail.
  function clearHistory() {
    chrome.history.deleteAll(pass(function() {
      countItemsInHistory(pass(function(count) {
        assertEq(0, count);
      }));
    }));
  },

  function deleteUrl() {
    function deleteUrlTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    function onAddedItem() {
      removeItemVisitedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        chrome.history.deleteUrl({ 'url': GOOGLE_URL });
      });
    }

    // deleteUrl entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteUrlTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function deleteStartRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var startDate = {};
    var endDate = {};
    var itemsAdded = 0;

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

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
      }

      removeItemVisitedListener();
      chrome.history.deleteRange({ 'startTime': startDate.getTime(),
                                   'endTime': endDate.getTime() },
                                 function() { });
    }

    // deletRange entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteRangeTestVerification);

      startDate = new Date();
      startDate.setTime(startDate.getTime() - 1000);

      populateHistory([urls[itemsAdded]], function() { });
    });
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur
  // in the sequence x A y B z.  The previous deleteRange test deleted the
  // range [x,y], this test deletes the range [y,z].
  function deleteEndRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var startDate = {};
    var endDate = {};
    var itemsAdded = 0;

    function deleteRange2TestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

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
      }

      removeItemVisitedListener();

      endDate = new Date();
      endDate.setTime(endDate.getTime() + 1000);
      chrome.history.deleteRange({'startTime': startDate.getTime(),
                                  'endTime': endDate.getTime() },
                                  function() { });
    }

    // deletRange entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteRange2TestVerification);
      populateHistory([urls[itemsAdded]], function() { });
    });
  },
]);
