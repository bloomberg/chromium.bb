// tabs api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.TabConnect

// We have a bunch of places where we need to remember some state from one
// test (or setup code) to subsequent tests.
var testTabId = null;

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;
var fail = chrome.test.fail;

chrome.test.getConfig(function(config) {

  chrome.test.runTests([
    function setupConnect() {
      // The web page that our content script will be injected into.
      var relativePath = '/extensions/api_test/tabs/basics/relative.html';
      var testUrl =
          ['http://localhost:PORT'.replace(/PORT/, config.testServer.port),
           relativePath].join('');

      setupWindow([testUrl], pass(function(winId, tabIds) {
        testTabId = tabIds[0];
        waitForAllTabs(pass());
      }));
    },

    function connectMultipleConnects() {
      var connectCount = 0;
      function connect10() {
        var port = chrome.tabs.connect(testTabId);
        chrome.test.listenOnce(port.onMessage, function(msg) {
          assertEq(++connectCount, msg.connections);
          if (connectCount < 10)
            connect10();
        });
        port.postMessage("GET");
      }
      connect10();
    },

    function connectName() {
      var name = "akln3901n12la";
      var port = chrome.tabs.connect(testTabId, {"name": name});
      chrome.test.listenOnce(port.onMessage, function(msg) {
        assertEq(name, msg.name);

        var port = chrome.tabs.connect(testTabId);
        chrome.test.listenOnce(port.onMessage, function(msg) {
          assertEq('', msg.name);
        });
        port.postMessage("GET");
      });
      port.postMessage("GET");
    },

    function connectPostMessageTypes() {
      var port = chrome.tabs.connect(testTabId);
      // Test the content script echoes the message back.
      var echoMsg = {"num": 10, "string": "hi", "array": [1,2,3,4,5],
                     "obj":{"dec": 1.0}};
      chrome.test.listenOnce(port.onMessage, function(msg) {
        assertEq(echoMsg.num, msg.num);
        assertEq(echoMsg.string, msg.string);
        assertEq(echoMsg.array[4], msg.array[4]);
        assertEq(echoMsg.obj.dec, msg.obj.dec);
      });
      port.postMessage(echoMsg);
    },

    function connectPostManyMessages() {
      var port = chrome.tabs.connect(testTabId);
      var count = 0;
      var done = chrome.test.listenForever(port.onMessage, function(msg) {
        assertEq(count++, msg);
        if (count == 999) {
          done();
        }
      });
      for (var i = 0; i < 1000; i++) {
        port.postMessage(i);
      }
    },

    function connectNoTab() {
      // Expect a disconnect event when you connect to a non-existant tab, and
      // once disconnected, expect an error while trying to post messages.
      var errorMsg = 
        "Could not establish connection. Receiving end does not exist.";
      chrome.tabs.create({}, pass(function(tab) {
        chrome.tabs.remove(tab.id, pass(function() {
          var p = chrome.tabs.connect(tab.id);
          p.onDisconnect.addListener(callbackFail(errorMsg, function() {
            try {
              p.postMessage();
              fail("Error should have been thrown.");
            } catch (e) {
              // Do nothing- an exception should be thrown.
            }
          }));
        }));
      }));
    },

    function sendRequest() {
      var request = "test";
      chrome.tabs.sendRequest(testTabId, request, pass(function(response) {
        assertEq(request, response);
      }));
    },

    function sendRequestNoTab() {
      var errorMsg = 
        "Could not establish connection. Receiving end does not exist.";
      chrome.tabs.create({}, pass(function(tab) {
        chrome.tabs.remove(tab.id, pass(function() {
          chrome.tabs.sendRequest(tab.id, "test", callbackFail(errorMsg));
        }));
      }));
    }
  ]);
});
