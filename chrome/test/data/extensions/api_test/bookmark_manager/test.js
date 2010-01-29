// Bookmark Manager API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.BookmarkManager

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const bookmarks = chrome.bookmarks;
const bookmarkManager = chrome.experimental.bookmarkManager;
const MAC = /Mac/.test(navigator.platform);
var node, node2, count;

var tests = [
  function getStrings() {
    bookmarkManager.getStrings(pass(function(strings) {
      assertEq('string', typeof strings['title']);
      assertEq('string', typeof strings['search_button']);
    }));
  }
];

// Mac does not yet support the clipboard API.
if (!MAC) {
  tests.push(

    // The clipboard test is split into different parts to allow asynchronous
    // operations to finish.
    function clipboard() {
      // Create a new bookmark.
      node = {
        parentId: '1',
        title: 'Foo',
        url: 'http://www.example.com/foo'
      };

      bookmarks.create(node, pass(function(result) {
        node.id = result.id;
        node.index = result.index;
        count = result.index + 1;
      }));
    },

    function clipboard2() {
      // Copy it.
      bookmarkManager.copy([node.id]);

      // Ensure canPaste is now true.
      bookmarkManager.canPaste('1', pass(function(result) {
        assertTrue(result, 'Should be able to paste now');
      }));

      // Paste it.
      bookmarkManager.paste('1');

      // Ensure it got added.
      bookmarks.getChildren('1', pass(function(result) {
        count++;
        assertEq(count, result.length);

        node2 = result[result.length - 1];

        assertEq(node.title, node2.title);
        assertEq(node.url, node2.url);
        assertEq(node.parentId, node2.parentId);
      }));
    },

    function clipboard3() {
      // Cut this and the previous bookmark.
      bookmarkManager.cut([node.id, node2.id]);

      // Ensure count decreased by 2.
      bookmarks.getChildren('1', pass(function(result) {
        count -= 2;
        assertEq(count, result.length);
      }));

      // Ensure canPaste is still true.
      bookmarkManager.canPaste('1', pass(function(result) {
        assertTrue(result, 'Should be able to paste now');
      }));
    },

    function clipboard4() {
      // Paste.
      bookmarkManager.paste('1');

      // Check the last two bookmarks.
      bookmarks.getChildren('1', pass(function(result) {
        count += 2;
        assertEq(count, result.length);

        var last = result[result.length - 2];
        var last2 = result[result.length - 1];
        assertEq(last.title, last2.title);
        assertEq(last.url, last2.url);
        assertEq(last.parentId, last2.parentId);
      }));
    }
  );
}

chrome.test.runTests(tests);
