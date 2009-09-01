// bookmarks api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Bookmarks

var expected = [
  {"children": [
    {"children": [], "id": "1", "parentId": "0", "index": 0, "title":"Bookmarks bar"},
    {"children": [], "id": "2", "parentId": "0", "index": 1, "title":"Other bookmarks"}
    ],
   "id": "0", "title": ""
  }
];

var testCallback = chrome.test.testCallback;

function compareNode(left, right) {
  //console.log(JSON.stringify(left));
  //console.log(JSON.stringify(right));
  // TODO(erikkay): do some comparison of dateAdded
  if (left.id != right.id)
    return "id mismatch: " + left.id + " != " + right.id;
  if (left.title != right.title)
    return "title mismatch: " + left.title + " != " + right.title;
  if (left.url != right.url)
    return "url mismatch: " + left.url + " != " + right.url;
  if (left.index != right.index)
    return "index mismatch: " + left.index + " != " + right.index;
  return true;
}

function compareTrees(left, right) {
  //console.log(JSON.stringify(left));
  //console.log(JSON.stringify(right));
  if (left == null && right == null) {
    console.log("both left and right are NULL");
    return true;
  }
  if (left == null || right == null)
    return left + " !+ " + right;
  if (left.length != right.length)
    return "count mismatch: " + left.length + " != " + right.length;
  for (var i = 0; i < left.length; i++) {
    var result = compareNode(left[i], right[i]);
    if (result !== true) {
      console.log(JSON.stringify(left));
      console.log(JSON.stringify(right));
      return result;
    }
    result = compareTrees(left[i].children, right[i].children);
    if (result !== true)
      return result;
  }
  return true;
}

chrome.test.runTests([
  function getTree() {
    chrome.bookmarks.getTree(testCallback(true, function(results) {
      chrome.test.assertTrue(compareTrees(results, expected),
                 "getTree() result != expected");
      expected = results;
    }));
  },

  function get() {
    chrome.bookmarks.get("1", testCallback(true, function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]));
    }));
  },

  function getArray() {
    chrome.bookmarks.get(["1", "2"], testCallback(true, function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]),
                 "get() result != expected");
      chrome.test.assertTrue(compareNode(results[1], expected[0].children[1]),
                 "get() result != expected");
    }));
  },

  function getChildren() {
    chrome.bookmarks.getChildren("0", testCallback(true, function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]),
                 "getChildren() result != expected");
      chrome.test.assertTrue(compareNode(results[1], expected[0].children[1]),
                 "getChildren() result != expected");
    }));
  },

  function create() {
    var node = {parentId: "1", title:"google", url:"http://www.google.com/"};
    chrome.bookmarks.create(node, testCallback(true, function(results) {
      node.id = results.id;  // since we couldn't know this going in
      node.index = 0;
      chrome.test.assertTrue(compareNode(node, results),
                 "created node != source");
    }));
  },
]);
