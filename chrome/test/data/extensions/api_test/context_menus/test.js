
if (!chrome.contextMenu) {
  chrome.contextMenu = chrome.experimental.contextMenu;
}

var assertNoLastError = chrome.test.assertNoLastError;

var tests = [
  function simple() {
    chrome.contextMenu.create({"title":"1"}, chrome.test.callbackPass());
  },

  function no_properties() {
    chrome.contextMenu.create({}, function(id) {
      chrome.test.assertTrue(chrome.extension.lastError != null);
      chrome.test.succeed();
    });
  },

  function remove() {
    chrome.contextMenu.create({"title":"1"}, function(id) {
      assertNoLastError();
      chrome.contextMenu.remove(id, chrome.test.callbackPass());
    });
  },

  function update() {
    chrome.contextMenu.create({"title":"update test"}, function(id) {
      assertNoLastError();
      chrome.contextMenu.update(id, {"title": "test2"},
                                chrome.test.callbackPass());
    });
  },

  function removeAll() {
    chrome.contextMenu.create({"title":"1"}, function(id) {
      assertNoLastError();
      chrome.contextMenu.create({"title":"2"}, function(id2) {
        assertNoLastError();
        chrome.contextMenu.removeAll(chrome.test.callbackPass());
      });
    });
  },

  function hasParent() {
    chrome.contextMenu.create({"title":"parent"}, function(id) {
      assertNoLastError();
      chrome.contextMenu.create({"title":"child", "parentId":id},
                                function(id2) {
        assertNoLastError();
        chrome.test.succeed();
      });
    });
  }
];


// Add tests for creating menu item with various types and contexts.
var types = ["CHECKBOX", "RADIO", "SEPARATOR"];
var contexts = ["ALL", "PAGE", "SELECTION", "LINK", "EDITABLE", "IMAGE",
                "VIDEO", "AUDIO"];
function makeCreateTest(type, contexts) {
  var result = function() {
    var title = type;
    if (contexts && contexts.length > 0) {
      title += " " + contexts.join(",");
    }
    var properties = {"title": title, "type": type};

    chrome.contextMenu.create(properties, chrome.test.callbackPass());
  };
  result.generatedName = "create_" + type +
                         (contexts ? "-" + contexts.join(",") : "");
  return result;
}

for (var i in types) {
  tests.push(makeCreateTest(types[i]));
}
for (var i in contexts) {
  tests.push(makeCreateTest("NORMAL", [ contexts[i] ]));
}

chrome.test.runTests(tests);

