// Processes API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Processes

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var listenOnce = chrome.test.listenOnce;

var tabs = [];

function createTab(index, url) {
  chrome.tabs.create({"url": url}, pass(function(tab) {
    tabs[index] = tab;
  }));
}

var getProcessId = chrome.experimental.processes.getProcessIdForTab;

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
    getProcessId(tabs[0].id, pass(function(pid0) {
      getProcessId(tabs[1].id, pass(function(pid1) {
        // about:blank and extension page should not share a process
        assertTrue(pid0 != pid1);
      }));
    }));
  },

  function extensionPagesShareProcess() {
    getProcessId(tabs[1].id, pass(function(pid1) {
      getProcessId(tabs[2].id, pass(function(pid2) {
        // Pages from same extension should share a process
        assertEq(pid1, pid2);
      }));
    }));
  },

  function newTabPageInOwnProcess() {
    getProcessId(tabs[0].id, pass(function(pid0) {
      getProcessId(tabs[3].id, pass(function(pid3) {
        // NTP should not share a process with current tabs
        assertTrue(pid0 != pid3);
      }));
    }));
  },

  function newTabPagesShareProcess() {
    getProcessId(tabs[3].id, pass(function(pid3) {
      getProcessId(tabs[4].id, pass(function(pid4) {
        // Multiple NTPs should share a process
        assertEq(pid3, pid4);
      }));
    }));
  },

  function idsInUpdateEvent() {
    listenOnce(chrome.experimental.processes.onUpdated, function(processes) {
      // onUpdated should return a valid dictionary of processes,
      // indexed by process ID.
      var pids = Object.keys(processes);
      assertEq(5, pids.length);

      // Should be able to look up process object by ID.
      assertTrue(processes[pids[0]].id == pids[0]);
      assertTrue(processes[pids[0]].id != processes[pids[1]].id);

      getProcessId(tabs[0].id, pass(function(pidTab0) {
        // Process ID for tab 0 should be listed in pids.
        assertTrue(processes[pidTab0] != undefined);
        assertEq("renderer", processes[pidTab0].type);
      }));
    });
  },

  function typesInUpdateEvent() {
    listenOnce(chrome.experimental.processes.onUpdated, function(processes) {
      // Check types: 1 browser, 3 renderers, and 1 extension
      var browserCount = 0;
      var rendererCount = 0;
      var extensionCount = 0;
      for (pid in processes) {
        switch (processes[pid].type) {
          case "browser":
            browserCount++;
            break;
          case "renderer":
            rendererCount++;
            break;
          case "extension":
            extensionCount++;
            break;
          default:
            fail("Unexpected process type:" + processes[pid].type);
        }
      }
      assertEq(1, browserCount);
      assertEq(3, rendererCount);
      assertEq(1, extensionCount);
    });
  },

  function propertiesOfProcesses() {
    listenOnce(chrome.experimental.processes.onUpdated, function(processes) {
      for (pid in processes) {
        var process = processes[pid];
        assertTrue("id" in process);
        assertTrue("type" in process);
        assertTrue("cpu" in process);
        assertTrue("network" in process);
        assertTrue("sharedMemory" in process);
        assertTrue("privateMemory" in process);
      }
    });
  },
]);
