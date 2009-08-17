var completed = false;

function complete() {
  completed = true;
  // a bit of a hack just to try to get the script to stop running at this point
  throw "completed";
}

function fail(message) {
  if (completed) throw "completed";

  console.log("FAIL: " + message);
  chrome.test.fail(message);
  complete();
}

function succeed() {
  if (completed) throw "completed";

  chrome.test.pass();
  complete();
}

window.onerror = function(message, url, code) {
  if (completed) return;

  fail(message);
};

function expectTrue(test, message) {
  if (!test) {
    fail(message);
  }
}

var expected = [
  {"children": [
    {"children": [], "id": "1", "parentId": "0", "title":"Bookmarks bar"},
    {"children": [], "id": "2", "parentId": "0", "title":"Other bookmarks"}
    ],
   "id": "0", "title": ""
  }
];

function compareTrees(left, right) {
  console.log("compare");
  console.log(JSON.stringify(right));
  console.log(JSON.stringify(left));
  // TODO(erikkay): do some comparison of dateAdded
  if (left == null && right == null) {
    console.log("both left and right are NULL");
    return true;
  }
  if (left == null || right == null)
    return false;
  if (left.length < right.length)
    return false;
  for (var i = 0; i < left.length; i++) {
    if (left[i].id != right[i].id)
      return false;
    console.log(left[i].title + " ? " + right[i].title);
    if (left[i].title != right[i].title)
      return false;
    if (!compareTrees(left[i].children, right[i].children))
      return false;
  }
  return true;
}

chrome.bookmarks.getTree(function(results) {
  expectTrue(compareTrees(results, expected),
             "getTree() result doesn't match expected");
  expected = results;
  console.log("done");
  succeed();
});
