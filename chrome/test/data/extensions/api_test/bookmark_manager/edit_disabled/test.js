// Bookmark Manager API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.BookmarkManagerEditDisabled

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const bookmarks = chrome.bookmarks;
const bookmarkManager = chrome.experimental.bookmarkManager;

var ERROR = "Bookmark editing is disabled.";

// Bookmark model within this test:
//  <root>/
//    Bookmarks Bar/
//      Folder/
//        "BBB"
//      "AAA"
//    Other Bookmarks/

var tests = [
  function verifyModel() {
    bookmarks.getTree(pass(function(result) {
      assertEq(1, result.length);
      var root = result[0];
      assertEq(2, root.children.length);
      bar = root.children[0];
      assertEq(2, bar.children.length);
      folder = bar.children[0];
      aaa = bar.children[1];
      assertEq('Folder', folder.title);
      assertEq('AAA', aaa.title);
      bbb = folder.children[0];
      assertEq('BBB', bbb.title);
    }));
  },

  function createDisabled() {
    bookmarks.create({ parentId: bar.id, title: 'Folder2' }, fail(ERROR));
  },

  function moveDisabled() {
    bookmarks.move(aaa.id, { parentId: folder.id }, fail(ERROR));
  },

  function removeDisabled() {
    bookmarks.remove(aaa.id, fail(ERROR));
  },

  function removeTreeDisabled() {
    bookmarks.removeTree(folder.id, fail(ERROR));
  },

  function updateDisabled() {
    bookmarks.update(aaa.id, { title: 'CCC' }, fail(ERROR));
  },

  function importDisabled() {
    bookmarks.import(fail(ERROR));
  },

  function cutDisabled() {
    bookmarkManager.cut([bbb.id], fail(ERROR));
  },

  function canPasteDisabled() {
    bookmarkManager.canPaste(folder.id, fail(ERROR));
  },

  function pasteDisabled() {
    bookmarkManager.paste(folder.id, [bbb.id], fail(ERROR));
  },

];

chrome.test.runTests(tests);
