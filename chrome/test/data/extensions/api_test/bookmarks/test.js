// bookmarks api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Bookmarks

var expected = [
  {"children": [
      {children:[], id:"1", parentId:"0", index:0, title:"Bookmarks bar"},
      {children:[], id:"2", parentId:"0", index:1, title:"Other bookmarks"}
    ],
   id:"0", title:""
  }
];

// Some variables that are used across multiple tests.
var node1 = {parentId:"1", title:"Foo bar baz",
             url:"http://www.example.com/hello"};
var node2 = {parentId:"1", title:"foo quux",
             url:"http://www.example.com/bar"};
var node3 = {parentId:"1", title:"bar baz",
             url:"http://www.google.com/hello/quux"};

var testCallback = chrome.test.testCallback;

function compareNode(left, right) {
  //chrome.test.log(JSON.stringify(left));
  //chrome.test.log(JSON.stringify(right));
  // TODO(erikkay): do some comparison of dateAdded
  if (left.id != right.id)
    return "id mismatch: " + left.id + " != " + right.id;
  if (left.title != right.title) {
    // TODO(erikkay): This resource dependency still isn't working reliably.
    // See bug 19866.
    // return "title mismatch: " + left.title + " != " + right.title;
    console.log("title mismatch: " + left.title + " != " + right.title);
  }
  if (left.url != right.url)
    return "url mismatch: " + left.url + " != " + right.url;
  if (left.index != right.index)
    return "index mismatch: " + left.index + " != " + right.index;
  return true;
}

function compareTrees(left, right) {
  //chrome.test.log(JSON.stringify(left) || "<null>");
  //chrome.test.log(JSON.stringify(right) || "<null>");
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
    var node = {parentId:"1", title:"google", url:"http://www.google.com/"};
    chrome.bookmarks.create(node, testCallback(true, function(results) {
      node.id = results.id;  // since we couldn't know this going in
      node.index = 0;
      chrome.test.assertTrue(compareNode(node, results),
                             "created node != source");
      expected[0].children[0].children.push(node);
    }));
  },
  
  function createFolder() {
    var node = {parentId:"1", title:"foo bar"};  // folder
    chrome.bookmarks.create(node, testCallback(true, function(results) {
      node.id = results.id;  // since we couldn't know this going in
      node.index = 1;
      node.children = [];
      chrome.test.assertTrue(compareNode(node, results),
                             "created node != source");
      expected[0].children[0].children.push(node);
    }));
  },
  
  function move_setup() {
    chrome.bookmarks.create(node1, testCallback(false, function(results) {
      node1.id = results.id;
      node1.index = results.index;
      expected[0].children[0].children.push(node1);
    }));
    chrome.bookmarks.create(node2, testCallback(false, function(results) {
      node2.id = results.id;
      node2.index = results.index;
      expected[0].children[0].children.push(node2);
    }));
    chrome.bookmarks.create(node3, testCallback(false, function(results) {
      node3.id = results.id;
      node3.index = results.index;
      expected[0].children[0].children.push(node3);
    }));
    chrome.bookmarks.getTree(testCallback(true, function(results) {
      chrome.test.assertTrue(compareTrees(expected, results),
                             "getTree() result != expected");
      expected = results;
    }));
  },
  
  function move() {
    // Move node1, node2, and node3 from their current location (the bookmark
    // bar) to be under the "other bookmarks" folder instead.
    var other = expected[0].children[1];
    //var folder = expected[0].children[0].children[1];
    //console.log(JSON.stringify(node1));
    chrome.bookmarks.move(node1.id, {parentId:other.id},
                          testCallback(false, function(results) {
      chrome.test.assertEq(results.parentId, other.id);
      node1.parentId = results.parentId;
    }));
    chrome.bookmarks.move(node2.id, {parentId:other.id},
                          testCallback(false, function(results) {
      chrome.test.assertEq(results.parentId, other.id);
      node3.parentId = results.parentId;
    }));
    // Insert node3 at index 1 rather than at the end.  This should result in
    // an order of node1, node3, node2.
    chrome.bookmarks.move(node3.id, {parentId:other.id, index:1},
                          testCallback(true, function(results) {
      chrome.test.assertEq(results.parentId, other.id);
      chrome.test.assertEq(results.index, 1);
      node3.parentId = results.parentId;
      node3.index = 1;
    }));
  },
  
  function search() {
    chrome.bookmarks.search("baz bar", testCallback(false, function(results) {
      // matches node1 & node3
      console.log(JSON.stringify(results));
      chrome.test.assertEq(2, results.length);
    }));
    chrome.bookmarks.search("www hello", testCallback(false, function(results) {
      // matches node1 & node3
      chrome.test.assertEq(2, results.length);
    }));
    chrome.bookmarks.search("bar example",
                            testCallback(false, function(results) {
      // matches node2
      chrome.test.assertEq(1, results.length);
    }));
    chrome.bookmarks.search("foo bar", testCallback(false, function(results) {
      // matches node1
      chrome.test.assertEq(1, results.length);
    }));
    chrome.bookmarks.search("quux", testCallback(true, function(results) {
      // matches node2 & node3
      chrome.test.assertEq(2, results.length);
    }));
  },
  
  function update() {
    chrome.bookmarks.update(node1.id, {"title": "hello world"},
                            testCallback(true, function(results) {
      chrome.test.assertEq("hello world", results.title);
    }));
  },
  
  function remove() {
    chrome.test.succeed();
  },
  
  function removeTree() {
    chrome.test.succeed();
  },
]);
