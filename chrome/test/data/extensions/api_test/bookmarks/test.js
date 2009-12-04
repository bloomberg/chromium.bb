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
var quota_node1 = {parentId:"1", title:"Dave",
                   url:"http://www.dmband.com/"};
var quota_node2 = {parentId:"1", title:"UW",
                   url:"http://www.uwaterloo.ca/"};
var quota_node3 = {parentId:"1", title:"Whistler",
                   url:"http://www.whistlerblackcomb.com/"};

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

function compareNode(left, right) {
  //chrome.test.log("compareNode()");
  //chrome.test.log(JSON.stringify(left, null, 2));
  //chrome.test.log(JSON.stringify(right, null, 2));
  // TODO(erikkay): do some comparison of dateAdded
  if (left.id != right.id)
    return "id mismatch: " + left.id + " != " + right.id;
  if (left.title != right.title) {
    // TODO(erikkay): This resource dependency still isn't working reliably.
    // See bug 19866.
    // return "title mismatch: " + left.title + " != " + right.title;
    chrome.test.log("title mismatch: " + left.title + " != " + right.title);
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
    return true;
  }
  if (left == null || right == null)
    return left + " !+ " + right;
  if (left.length != right.length)
    return "count mismatch: " + left.length + " != " + right.length;
  for (var i = 0; i < left.length; i++) {
    var result = compareNode(left[i], right[i]);
    if (result !== true) {
      chrome.test.log(result);
      chrome.test.log(JSON.stringify(left, null, 2));
      chrome.test.log(JSON.stringify(right, null, 2));
      return result;
    }
    result = compareTrees(left[i].children, right[i].children);
    if (result !== true)
      return result;
  }
  return true;
}

function createThreeNodes(one, two, three) {
  var bookmarks_bar = expected[0].children[0];
  chrome.bookmarks.create(one, pass(function(results) {
    one.id = results.id;
    one.index = results.index;
    bookmarks_bar.children.push(one);
  }));
  chrome.bookmarks.create(two, pass(function(results) {
    two.id = results.id;
    two.index = results.index;
    bookmarks_bar.children.push(two);
  }));
  chrome.bookmarks.create(three, pass(function(results) {
    three.id = results.id;
    three.index = results.index;
    bookmarks_bar.children.push(three);
  }));
  chrome.bookmarks.getTree(pass(function(results) {
    chrome.test.assertTrue(compareTrees(expected, results),
                           "getTree() result != expected");
    expected = results;
  }));
}

chrome.test.runTests([
  function getTree() {
    chrome.bookmarks.getTree(pass(function(results) {
      chrome.test.assertTrue(compareTrees(results, expected),
                             "getTree() result != expected");
      expected = results;
    }));
  },

  function get() {
    chrome.bookmarks.get("1", pass(function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]));
    }));
    chrome.bookmarks.get("42", fail("Can't find bookmark for id."));
  },

  function getArray() {
    chrome.bookmarks.get(["1", "2"], pass(function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]),
                             "get() result != expected");
      chrome.test.assertTrue(compareNode(results[1], expected[0].children[1]),
                             "get() result != expected");
    }));
  },

  function getChildren() {
    chrome.bookmarks.getChildren("0", pass(function(results) {
      chrome.test.assertTrue(compareNode(results[0], expected[0].children[0]),
                             "getChildren() result != expected");
      chrome.test.assertTrue(compareNode(results[1], expected[0].children[1]),
                             "getChildren() result != expected");
    }));
  },

  function create() {
    var node = {parentId:"1", title:"google", url:"http://www.google.com/"};
    chrome.test.listenOnce(chrome.bookmarks.onCreated, function(id, created) {
      node.id = created.id;
      node.index = 0;
      chrome.test.assertEq(id, node.id);
      chrome.test.assertTrue(compareNode(node, created));
    });
    chrome.bookmarks.create(node, pass(function(results) {
      node.id = results.id;  // since we couldn't know this going in
      node.index = 0;
      chrome.test.assertTrue(compareNode(node, results),
                             "created node != source");
      expected[0].children[0].children.push(node);
    }));
  },

  function createFolder() {
    var node = {parentId:"1", title:"foo bar"};  // folder
    chrome.test.listenOnce(chrome.bookmarks.onCreated, function(id, created) {
      node.id = created.id;
      node.index = 1;
      node.children = [];
      chrome.test.assertTrue(compareNode(node, created));
    });
    chrome.bookmarks.create(node, pass(function(results) {
      node.id = results.id;  // since we couldn't know this going in
      node.index = 1;
      node.children = [];
      chrome.test.assertTrue(compareNode(node, results),
                             "created node != source");
      expected[0].children[0].children.push(node);
    }));
  },

  function move_setup() {
    createThreeNodes(node1, node2, node3);
  },

  function move() {
    // Move node1, node2, and node3 from their current location (the bookmark
    // bar) to be under the "foo bar" folder (created in createFolder()).
    // Then move that folder to be under the "other bookmarks" folder.
    var folder = expected[0].children[0].children[1];
    var old_node1 = expected[0].children[0].children[2];
    chrome.test.listenOnce(chrome.bookmarks.onMoved, function(id, moveInfo) {
      chrome.test.assertEq(node1.id, id);
      chrome.test.assertEq(moveInfo.parentId, folder.id);
      chrome.test.assertEq(moveInfo.index, 0);
      chrome.test.assertEq(moveInfo.oldParentId, old_node1.parentId);
      chrome.test.assertEq(moveInfo.oldIndex, old_node1.index);
    });
    chrome.bookmarks.move(node1.id, {parentId:folder.id},
                          pass(function(results) {
      chrome.test.assertEq(results.parentId, folder.id);
      node1.parentId = results.parentId;
      node1.index = 0;
    }));
    chrome.bookmarks.move(node2.id, {parentId:folder.id},
                          pass(function(results) {
      chrome.test.assertEq(results.parentId, folder.id);
      node2.parentId = results.parentId;
      node2.index = 1;
    }));
    // Insert node3 at index 1 rather than at the end.  This should result in
    // an order of node1, node3, node2.
    chrome.bookmarks.move(node3.id, {parentId:folder.id, index:1},
                          pass(function(results) {
      chrome.test.assertEq(results.parentId, folder.id);
      chrome.test.assertEq(results.index, 1);
      node3.parentId = results.parentId;
      node3.index = 1;
      node2.index = 2;
    }));

    chrome.bookmarks.getTree(pass(function(results) {
      expected[0].children[0].children.pop();
      expected[0].children[0].children.pop();
      expected[0].children[0].children.pop();
      folder.children.push(node1);
      folder.children.push(node3);
      folder.children.push(node2);
      chrome.test.assertTrue(compareTrees(expected, results),
                             "getTree() result != expected");
      expected = results;
    }));

    // Move folder (and its children) to be a child of Other Bookmarks.
    var other = expected[0].children[1];
    chrome.bookmarks.move(folder.id, {parentId:other.id},
                          pass(function(results) {
      chrome.test.assertEq(results.parentId, other.id);
      folder.parentId = results.parentId;
    }));

    chrome.bookmarks.getTree(pass(function(results) {
      var folder = expected[0].children[0].children.pop();
      folder.parentId = other.parentId;
      folder.index = 0;
      expected[0].children[1].children.push(folder);
      chrome.test.assertTrue(compareTrees(expected, results),
                             "getTree() result != expected");
      expected = results;
    }));
  },

  function search() {
    chrome.bookmarks.search("baz bar", pass(function(results) {
      // matches node1 & node3
      chrome.test.assertEq(2, results.length);
    }));
    chrome.bookmarks.search("www hello", pass(function(results) {
      // matches node1 & node3
      chrome.test.assertEq(2, results.length);
    }));
    chrome.bookmarks.search("bar example",
                            pass(function(results) {
      // matches node2
      chrome.test.assertEq(1, results.length);
    }));
    chrome.bookmarks.search("foo bar", pass(function(results) {
      // matches node1
      chrome.test.assertEq(1, results.length);
    }));
    chrome.bookmarks.search("quux", pass(function(results) {
      // matches node2 & node3
      chrome.test.assertEq(2, results.length);
    }));
  },

  function update() {
    var title = "hello world";
    chrome.test.listenOnce(chrome.bookmarks.onChanged, function(id, changes) {
      chrome.test.assertEq(title, changes.title);
    });
    chrome.bookmarks.update(node1.id, {"title": title}, pass(function(results) {
      chrome.test.assertEq(title, results.title);
    }));
  },

  function remove() {
    var parentId = node1.parentId;
    chrome.test.listenOnce(chrome.bookmarks.onRemoved,
                           function(id, removeInfo) {
      chrome.test.assertEq(id, node1.id);
      chrome.test.assertEq(removeInfo.parentId, parentId);
      chrome.test.assertEq(removeInfo.index, node1.index);
    });
    chrome.bookmarks.remove(node1.id, pass(function() {}));
    chrome.bookmarks.getTree(pass(function(results) {
      // We removed node1, which means that the index of the other two nodes
      // changes as well.
      expected[0].children[1].children[0].children.shift();
      expected[0].children[1].children[0].children[0].index = 0;
      expected[0].children[1].children[0].children[1].index = 1;
      chrome.test.assertTrue(compareTrees(expected, results),
                             "getTree() result != expected");
      expected = results;
    }));
  },

  function removeTree() {
    var parentId = node2.parentId;
    var folder = expected[0].children[1].children[0];
    chrome.test.listenOnce(chrome.bookmarks.onRemoved,
                           function(id, removeInfo) {
      chrome.test.assertEq(id, folder.id);
      chrome.test.assertEq(removeInfo.parentId, folder.parentId);
      chrome.test.assertEq(removeInfo.index, folder.index);
    });
    chrome.bookmarks.removeTree(parentId, pass(function(){}));
    chrome.bookmarks.getTree(pass(function(results) {
      expected[0].children[1].children.shift();
      chrome.test.assertTrue(compareTrees(expected, results),
                             "getTree() result != expected");
      expected = results;
    }));
  },

  function quotaLimitedCreate() {
    var node = {parentId:"1", title:"quotacreate", url:"http://www.quota.com/"};
    for (i = 0; i < 100; i++) {
      chrome.bookmarks.create(node, pass(function(results) {
        expected[0].children[0].children.push(results);
      }));
    }
    chrome.bookmarks.create(node,
                            fail("This request exceeds available quota."));

    chrome.test.resetQuota();

    // Also, test that > 100 creations of different items is fine.
    for (i = 0; i < 101; i++) {
      var changer = {parentId:"1", title:"" + i, url:"http://www.quota.com/"};
      chrome.bookmarks.create(changer, pass(function(results) {
        expected[0].children[0].children.push(results);
      }));
    }
  },

  function quota_setup() {
    createThreeNodes(quota_node1, quota_node2, quota_node3);
  },

  function quotaLimitedUpdate() {
    var title = "hello, world!";
    for (i = 0; i < 100; i++) {
      chrome.bookmarks.update(quota_node1.id, {"title": title},
          pass(function(results) {
                 chrome.test.assertEq(title, results.title);
               }
      ));
    }
    chrome.bookmarks.update(quota_node1.id, {"title": title},
        fail("This request exceeds available quota."));

    chrome.test.resetQuota();
  },
]);
