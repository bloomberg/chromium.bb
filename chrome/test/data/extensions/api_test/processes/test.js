// Processes API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Processes

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var tabs = [];

function createTab(index, url) {
  chrome.tabs.create({"url": url}, pass(function(tab) {
    tabs[index] = tab;
  }));
}

var getProcess = chrome.experimental.processes.getProcessForTab;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
}

chrome.test.runTests([
  function setupProcessTests() {
    // Open 4 tabs for test, then wait and create a 5th
    createTab(0, "about:blank");
    createTab(1, pageUrl("a"));
    createTab(2, pageUrl("b"));
    createTab(3, "chrome://newtab/");

    // Wait for all loads to complete.
    var completedCount = 0;
    var onUpdatedCompleted = chrome.test.listenForever(
      chrome.tabs.onUpdated,
      function(changedTabId, changeInfo, changedTab) {
        if (changedTab.status == "complete") {
          completedCount++;

          // Once the NTP finishes loading, create another one.  This ensures
          // both NTPs end up in the same process.
          if (changedTabId == tabs[3].id) {
            createTab(4, "chrome://newtab/");
          }
        }

        // Once all tabs are done loading, continue with the next test.
        if (completedCount == 4) {
          onUpdatedCompleted();
        }
      }
    );

  },

  function extensionPageInOwnProcess() {
    getProcess(tabs[0].id, pass(function(process0) {
      getProcess(tabs[1].id, pass(function(process1) {
        // about:blank and extension page should not share a process
        assertTrue(process0.id != process1.id);
      }));
    }));
  },

  function extensionPagesShareProcess() {
    getProcess(tabs[1].id, pass(function(process1) {
      getProcess(tabs[2].id, pass(function(process2) {
        // Pages from same extension should share a process
        assertEq(process1.id, process2.id);
      }));
    }));
  },

  function newTabPageInOwnProcess() {
    getProcess(tabs[0].id, pass(function(process0) {
      getProcess(tabs[3].id, pass(function(process3) {
        // NTP should not share a process with current tabs
        assertTrue(process0.id != process3.id);
      }));
    }));
  },

  function newTabPagesShareProcess() {
    getProcess(tabs[3].id, pass(function(process3) {
      getProcess(tabs[4].id, pass(function(process4) {
        // Multiple NTPs should share a process
        assertEq(process3.id, process4.id);
      }));
    }));
  },

]);
