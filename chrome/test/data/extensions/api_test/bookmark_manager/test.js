// Bookmark Manager API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.BookmarkManager

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const bookmarks = chrome.bookmarks;
const bookmarkManager = chrome.experimental.bookmarkManager;
var fooNode, fooNode2, barNode, gooNode, count, emptyFolder, emptyFolder2;
var folder, nodeA, nodeB;
var childFolder, grandChildFolder, childNodeA, childNodeB;

var tests = [
  function getStrings() {
    bookmarkManager.getStrings(pass(function(strings) {
      assertEq('string', typeof strings['title']);
      assertEq('string', typeof strings['search_button']);
    }));
  },

  function sortChildren() {
    folder = {
      parentId: '1',
      title: 'Folder'
    };
    nodeA = {
      title: 'a',
      url: 'http://www.example.com/a'
    };
    nodeB = {
      title: 'b',
      url: 'http://www.example.com/b'
    };
    bookmarks.create(folder, pass(function(result) {
      folder.id = result.id;
      nodeA.parentId = folder.id;
      nodeB.parentId = folder.id;

      bookmarks.create(nodeB, pass(function(result) {
        nodeB.id = result.id;
      }));
      bookmarks.create(nodeA, pass(function(result) {
        nodeA.id = result.id;
      }));
    }));
  },

  function sortChildren2() {
    bookmarkManager.sortChildren(folder.id);

    bookmarks.getChildren(folder.id, pass(function(children) {
      assertEq(nodeA.id, children[0].id);
      assertEq(nodeB.id, children[1].id);
    }));
  },

  function setupSubtree() {
    childFolder = {
      parentId: folder.id,
      title: 'Child Folder'
    };
    childNodeA = {
      title: 'childNodeA',
      url: 'http://www.example.com/childNodeA'
    };
    childNodeB = {
      title: 'childNodeB',
      url: 'http://www.example.com/childNodeB'
    };
    grandChildFolder = {
      title: 'grandChildFolder'
    };
    bookmarks.create(childFolder, pass(function(result) {
      childFolder.id = result.id;
      childNodeA.parentId = childFolder.id;
      childNodeB.parentId = childFolder.id;
      grandChildFolder.parentId = childFolder.id;

      bookmarks.create(childNodeA, pass(function(result) {
        childNodeA.id = result.id;
      }));
      bookmarks.create(childNodeB, pass(function(result) {
        childNodeB.id = result.id;
      }));
      bookmarks.create(grandChildFolder, pass(function(result) {
        grandChildFolder.id = result.id;
      }));
    }))
  },

  function getSubtree() {
    bookmarkManager.getSubtree(childFolder.id, false, pass(function(result) {
      var children = result[0].children;
      assertEq(3, children.length);
      assertEq(childNodeA.id, children[0].id);
      assertEq(childNodeB.id, children[1].id);
      assertEq(grandChildFolder.id, children[2].id);
    }))
  },

  function getSubtreeFoldersOnly() {
    bookmarkManager.getSubtree(childFolder.id, true, pass(function(result) {
      var children = result[0].children;
      assertEq(1, children.length);
      assertEq(grandChildFolder.id, children[0].id);
    }))
  },

  // The clipboard test is split into different parts to allow asynchronous
  // operations to finish.
  function clipboard() {
    // Create a new bookmark.
    fooNode = {
      parentId: '1',
      title: 'Foo',
      url: 'http://www.example.com/foo'
    };

    emptyFolder = {
      parentId: '1',
      title: 'Empty Folder'
    }

    bookmarks.create(fooNode, pass(function(result) {
      fooNode.id = result.id;
      fooNode.index = result.index;
      count = result.index + 1;
    }));

    bookmarks.create(emptyFolder, pass(function(result) {
      emptyFolder.id = result.id;
      emptyFolder.index = result.index;
      count = result.index + 1;
    }));

    // Create a couple more bookmarks to test proper insertion of pasted items.
    barNode = {
      parentId: '1',
      title: 'Bar',
      url: 'http://www.example.com/bar'
    };

    bookmarks.create(barNode, pass(function(result) {
      barNode.id = result.id;
      barNode.index = result.index;
      count = result.index + 1;
    }));

    gooNode = {
      parentId: '1',
      title: 'Goo',
      url: 'http://www.example.com/goo'
    };

    bookmarks.create(gooNode, pass(function(result) {
      gooNode.id = result.id;
      gooNode.index = result.index;
      count = result.index + 1;
    }));
  },

  function clipboard2() {
    // Copy the fooNode.
    bookmarkManager.copy([fooNode.id]);

    // Ensure canPaste is now true.
    bookmarkManager.canPaste('1', pass(function(result) {
      assertTrue(result, 'Should be able to paste now');
    }));

    // Paste it.
    bookmarkManager.paste('1');

    // Ensure it got added at the end.
    bookmarks.getChildren('1', pass(function(result) {
      count++;
      assertEq(count, result.length);

      fooNode2 = result[result.length - 1];

      assertEq(fooNode.title, fooNode2.title);
      assertEq(fooNode.url, fooNode2.url);
      assertEq(fooNode.parentId, fooNode2.parentId);
    }));
  },

  function clipboard3() {
    // Cut fooNode bookmarks.
    bookmarkManager.cut([fooNode.id, fooNode2.id]);

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
    // Paste the cut bookmarks at a specific position between bar and goo.
    bookmarkManager.paste('1', [barNode.id]);

    // Check that the two bookmarks were pasted after bar.
    bookmarks.getChildren('1', pass(function(result) {
      count += 2;
      assertEq(count, result.length);

      // Look for barNode's index.
      for (var barIndex = 0; barIndex < result.length; barIndex++) {
        if (result[barIndex].id == barNode.id)
          break;
      }
      assertTrue(barIndex + 2 < result.length);

      var last = result[barIndex + 1];
      var last2 = result[barIndex + 2];
      assertEq(fooNode.title, last.title);
      assertEq(fooNode.url, last.url);
      assertEq(fooNode.parentId, last.parentId);
      assertEq(last.title, last2.title);
      assertEq(last.url, last2.url);
      assertEq(last.parentId, last2.parentId);

      // Remember last2 id, so we can use it in next test.
      fooNode2.id = last2.id;
    }));
  },

  // Ensure we can copy empty folders
  function clipboard5() {
    // Copy it.
    bookmarkManager.copy([emptyFolder.id]);

    // Ensure canPaste is now true.
    bookmarkManager.canPaste('1', pass(function(result) {
      assertTrue(result, 'Should be able to paste now');
    }));

    // Paste it at the end of a multiple selection.
    bookmarkManager.paste('1', [barNode.id, fooNode2.id]);

    // Ensure it got added at the right place.
    bookmarks.getChildren('1', pass(function(result) {
      count++;
      assertEq(count, result.length);

      // Look for fooNode2's index.
      for (var foo2Index = 0; foo2Index < result.length; foo2Index++) {
        if (result[foo2Index].id == fooNode2.id)
          break;
      }
      assertTrue(foo2Index + 1 < result.length);

      emptyFolder2 = result[foo2Index + 1];

      assertEq(emptyFolder2.title, emptyFolder.title);
      assertEq(emptyFolder2.url, emptyFolder.url);
      assertEq(emptyFolder2.parentId, emptyFolder.parentId);
    }));
  }
];

chrome.test.runTests(tests);
