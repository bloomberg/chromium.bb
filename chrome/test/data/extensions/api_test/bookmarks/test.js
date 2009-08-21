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

var tests = [
  function getTree() {
    chrome.bookmarks.getTree(function(results) {
      assertNoLastError();
      assertTrue(compareTrees(results, expected),
                 "getTree() result != expected");
      expected = results;
      succeed();
    });
  },
  
  function get() {
    chrome.bookmarks.get("1", function(results) {
      assertNoLastError();
      assertTrue(compareNode(results[0], expected[0].children[0]));
      succeed();
    });
  },
  
  function getArray() {
    chrome.bookmarks.get(["1", "2"], function(results) {
      assertNoLastError();
      assertTrue(compareNode(results[0], expected[0].children[0]),
                 "get() result != expected");
      assertTrue(compareNode(results[1], expected[0].children[1]),
                 "get() result != expected");
      succeed();
    });
  },
  
  function getChildren() {
    chrome.bookmarks.getChildren("0", function(results) {
      assertNoLastError();
      assertTrue(compareNode(results[0], expected[0].children[0]),
                 "getChildren() result != expected");
      assertTrue(compareNode(results[1], expected[0].children[1]),
                 "getChildren() result != expected");
      succeed();
    });
  },
  
  function create() {
    var node = {parentId: "1", title:"google", url:"http://www.google.com/"};
    chrome.bookmarks.create(node, function(results) {
      assertNoLastError();
      node.id = results.id;  // since we couldn't know this going in
      node.index = 0;
      assertTrue(compareNode(node, results),
                 "created node != source");
      succeed();
    });
  },
];

runNextTest();
