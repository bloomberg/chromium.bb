// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-store>', function() {
  var store;
  var TEST_TREE;

  function replaceStore() {
    store = document.createElement('bookmarks-store');
    replaceBody(store);
    store.setupStore_(TEST_TREE);
  }

  function navigateTo(route) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('location-changed'));
  }

  /**
   * Overrides the chrome.bookmarks.search to pass results into the callback.
   * @param {Array} results
   */
  function overrideBookmarksSearch(results) {
    chrome.bookmarks.search = function(searchTerm, callback) {
      callback(results);
    };
  }

  /**
   * Overrides the chrome.bookmarks.getSubTree to pass results into the
   * callback.
   * @param {Array} results
   */
  function overrideBookmarksGetSubTree(results) {
    chrome.bookmarks.getSubTree = function(parentId, callback) {
      callback(results);
    };
  }

  setup(function() {
    TEST_TREE = createFolder('0', [
      createFolder(
          '1',
          [
            createItem('2', {url: 'link2'}),
            createFolder('3', []),
            createItem('6', {url: 'link4'}),
            createItem('7', {url: 'link5'}),
          ]),
      createItem('4', {url: 'link4'}),
      createItem('5', {url: 'link5'}),
      createFolder('8', []),
    ]);

    replaceStore();
  });

  teardown(function() {
    // Clean up anything left in URL.
    navigateTo('/');
  });

  //////////////////////////////////////////////////////////////////////////////
  // store initialization tests:

  test('initNodes inserts nodes into idToNodeMap', function() {
    assertEquals(TEST_TREE, store.idToNodeMap_['0']);
    assertEquals(TEST_TREE.children[0], store.idToNodeMap_['1']);
    assertEquals(TEST_TREE.children[0].children[0], store.idToNodeMap_['2']);
    assertEquals(TEST_TREE.children[0].children[1], store.idToNodeMap_['3']);
    assertEquals(TEST_TREE.children[1], store.idToNodeMap_['4']);
    assertEquals(TEST_TREE.children[2], store.idToNodeMap_['5']);
  });

  test('correct paths generated for nodes', function() {
    var TEST_PATHS = {
      '0': 'rootNode',
      '1': 'rootNode.children.#0',
      '2': 'rootNode.children.#0.children.#0',
      '3': 'rootNode.children.#0.children.#1',
      '4': 'rootNode.children.#1',
      '5': 'rootNode.children.#2',
      '6': 'rootNode.children.#0.children.#2',
      '7': 'rootNode.children.#0.children.#3',
      '8': 'rootNode.children.#3',
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);
  });

  //////////////////////////////////////////////////////////////////////////////
  // editing bookmarks tree tests:

  test('changing selectedId changes the displayedList', function() {
    store.selectedId = '0';
    assertEquals(TEST_TREE.children, store.displayedList);
    store.selectedId = '1';
    assertEquals(TEST_TREE.children[0].children, store.displayedList);
    store.selectedId = '3';
    assertEquals(
        TEST_TREE.children[0].children[1].children, store.displayedList);

    // Selecting an item selects the default folder.
    store.selectedId = '5';
    assertEquals(TEST_TREE.children[0].children, store.displayedList);
  });

  test('store updates on selected event', function() {
    // First child of root is selected by default.
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelectedFolder);

    // Selecting a selected folder doesn't deselect it.
    store.fire('selected-folder-changed', '1');
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelectedFolder);

    // Select a deeply nested descendant.
    store.fire('selected-folder-changed', '3');
    assertEquals('3', store.selectedId);
    assertTrue(store.idToNodeMap_['3'].isSelectedFolder);
    assertFalse(store.idToNodeMap_['1'].isSelectedFolder);

    // Select a folder in separate subtree.
    store.fire('selected-folder-changed', '8');
    assertEquals('8', store.selectedId);
    assertTrue(store.idToNodeMap_['8'].isSelectedFolder);
    assertFalse(store.idToNodeMap_['3'].isSelectedFolder);
  });

  test('store updates on open and close', function() {
    // All folders are open by default.
    for (var id in store.idToNodeMap_) {
      if (store.idToNodeMap_[id].url)
        continue;

      assertTrue(store.idToNodeMap_[id].isOpen);
    }

    // Closing a folder doesn't close any descendants.
    store.fire('folder-open-changed', {id: '1', open: false});
    assertFalse(store.idToNodeMap_['1'].isOpen);
    assertTrue(store.idToNodeMap_['3'].isOpen);
    store.fire('folder-open-changed', {id: '1', open: true});

    // Closing an ancestor folder of a selected folder selects the ancestor.
    store.fire('selected-folder-changed', '3');
    store.fire('folder-open-changed', {id: '1', open: false});
    assertFalse(store.idToNodeMap_['1'].isOpen);
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelectedFolder);
    assertFalse(store.idToNodeMap_['3'].isSelectedFolder);
  });

  test('parent folder opens when descendant folder is selected', function() {
    store.idToNodeMap_['0'].isOpen = false;
    store.idToNodeMap_['1'].isOpen = false;
    store.idToNodeMap_['3'].isOpen = false;
    store.fire('selected-folder-changed', '3');
    assertTrue(store.idToNodeMap_['0'].isOpen);
    assertTrue(store.idToNodeMap_['1'].isOpen);
    assertFalse(store.idToNodeMap_['3'].isOpen);
  });

  test('deleting a node updates the tree', function() {
    removeChild(TEST_TREE, 1);
    overrideBookmarksGetSubTree([TEST_TREE]);
    // Remove an empty folder/bookmark.
    store.onBookmarkRemoved_('4', {parentId: '0', index: 1});

    // Check the tree is correct.
    assertEquals('5', store.rootNode.children[1].id);

    // idToNodeMap_ has been updated.
    assertEquals(undefined, store.idToNodeMap_['4']);
    assertEquals(store.rootNode.children[1], store.idToNodeMap_['5']);

    // Paths have been updated.
    var TEST_PATHS = {
      '0': 'rootNode',
      '1': 'rootNode.children.#0',
      '2': 'rootNode.children.#0.children.#0',
      '3': 'rootNode.children.#0.children.#1',
      '5': 'rootNode.children.#1',
      '6': 'rootNode.children.#0.children.#2',
      '7': 'rootNode.children.#0.children.#3',
      '8': 'rootNode.children.#2',
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);

    // Remove a folder with children.
    removeChild(TEST_TREE, 0);
    overrideBookmarksGetSubTree([TEST_TREE]);

    store.onBookmarkRemoved_('1', {parentId: '0', index: '0'});

    // Check the tree is correct.
    assertEquals('5', store.rootNode.children[0].id);
    assertEquals('8', store.rootNode.children[1].id);

    // idToNodeMap_ has been updated.
    assertEquals(undefined, store.idToNodeMap_['1']);
    assertEquals(undefined, store.idToNodeMap_['2']);
    assertEquals(undefined, store.idToNodeMap_['3']);
    assertEquals(undefined, store.idToNodeMap_['4']);
    assertEquals(store.rootNode.children[0], store.idToNodeMap_['5']);
    assertEquals(store.rootNode.children[1], store.idToNodeMap_['8']);

    // Paths have been updated.
    TEST_PATHS = {
      '0': 'rootNode',
      '5': 'rootNode.children.#0',
      '8': 'rootNode.children.#1'
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);
  });

  test('selectedId updates after removing a selected folder', function() {
    // Selected folder gets removed.
    store.selectedId = '8';
    removeChild(TEST_TREE, 3);
    overrideBookmarksGetSubTree([TEST_TREE]);

    store.onBookmarkRemoved_('8', {parentId:'0', index:'3'});
    assertTrue(store.idToNodeMap_['0'].isSelectedFolder);
    assertEquals('0', store.selectedId);

    // A folder with selected folder in it gets removed.
    store.selectedId = '3';
    removeChild(TEST_TREE, 0);
    overrideBookmarksGetSubTree([TEST_TREE]);

    store.onBookmarkRemoved_('1', {parentId:'0', index:'0'});
    assertTrue(store.idToNodeMap_['0'].isSelectedFolder);
    assertEquals('0', store.selectedId);
  });

  test('bookmark gets updated after editing', function() {
    // Edit title updates idToNodeMap_ properly.
    store.onBookmarkChanged_('4', {'title': 'test'});
    assertEquals('test', store.idToNodeMap_['4'].title);
    assertEquals('link4', store.idToNodeMap_['4'].url);

    // Edit url updates idToNodeMap_ properly.
    store.onBookmarkChanged_('5', {'url': 'http://www.google.com'});
    assertEquals('', store.idToNodeMap_['5'].title);
    assertEquals('http://www.google.com', store.idToNodeMap_['5'].url);

    // Edit url and title updates idToNodeMap_ properly.
    store.onBookmarkChanged_('2', {
      'title': 'test',
      'url': 'http://www.google.com',
    });
    assertEquals('test', store.idToNodeMap_['2'].title);
    assertEquals('http://www.google.com', store.idToNodeMap_['2'].url);
  });

  test('folder gets updated after renaming', function() {
    store.onBookmarkChanged_('3', {'title': 'Main Folder'});
    assertEquals('Main Folder', store.idToNodeMap_['3'].title);
    assertEquals(undefined, store.idToNodeMap_['3'].url);
  });

  //////////////////////////////////////////////////////////////////////////////
  // search tests:

  test('displayedList updates after searchTerm changes', function() {
    var SEARCH_RESULTS = [
      createItem('1', {title: 'cat'}),
      createItem('2', {title: 'apple'}),
      createItem('3', {title: 'paris'}),
    ];
    overrideBookmarksSearch(SEARCH_RESULTS);

    // Search for a non-empty string.
    store.searchTerm = 'a';
    assertFalse(store.rootNode.children[0].isSelectedFolder);
    assertEquals(null, store.selectedId);
    assertEquals(SEARCH_RESULTS, store.displayedList);

    // Clear the searchTerm.
    store.searchTerm = '';
    var defaultFolder = store.rootNode.children[0];
    assertTrue(defaultFolder.isSelectedFolder);
    assertEquals(defaultFolder.id, store.selectedId);
    assertEquals(defaultFolder.children, store.displayedList);

    // Search with no bookmarks returned.
    overrideBookmarksSearch([]);
    store.searchTerm = 'asdf';
    assertEquals(0, store.displayedList.length);
  });

  //////////////////////////////////////////////////////////////////////////////
  // router tests:

  test('search updates from route', function() {
    overrideBookmarksSearch([]);
    searchTerm = 'Pond';
    navigateTo('/?q=' + searchTerm);
    assertEquals(searchTerm, store.searchTerm);
  });

  test('search updates from route on setup', function() {
    overrideBookmarksSearch([]);
    var searchTerm = 'Boat24';
    navigateTo('/?q=' + searchTerm);
    replaceStore();
    assertEquals(searchTerm, store.searchTerm);
  });

  test('route updates from search', function() {
    overrideBookmarksSearch([]);
    var searchTerm = 'Boat24';
    store.searchTerm = searchTerm;
    assertEquals('chrome://bookmarks/?q=' + searchTerm, window.location.href);
  });

  test('selectedId updates from route', function() {
    // Folder id routes to the corresponding folder.
    var selectedId = '3';
    navigateTo('/?id=' + selectedId);
    assertEquals(selectedId, store.selectedId);

    // Bookmark id routes to the default Bookmarks Bar.
    var selectedId = '2';
    navigateTo('/?id=' + selectedId);
    assertEquals(store.rootNode.children[0].id, store.selectedId);

    // Invalid id routes to the default Bookmarks Bar.
    selectedId = 'foo';
    navigateTo('/?id=' + selectedId);
    assertEquals(store.rootNode.children[0].id, store.selectedId);
  });

  test('selectedId updates from route on setup', function() {
    selectedId = '3';
    navigateTo('/?id=' + selectedId);
    replaceStore();
    assertEquals(selectedId, store.selectedId);
  });

  test('route updates from selectedId', function() {
    var selectedId = '2';
    store.selectedId = selectedId;
    assertEquals('chrome://bookmarks/?id=' + selectedId, window.location.href);
  });

  //////////////////////////////////////////////////////////////////////////////
  // selection tests:

  test('single select selects the correct bookmark', function() {
    for (var id in store.idToNodeMap_)
      assertFalse(store.idToNodeMap_[id].isSelectedItem);

    store.fire('select-item', {item: store.idToNodeMap_['2']});
    assertDeepEquals(
        [true, false, false, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(0, store.anchorIndex_);

    // Select other item will remove the previous selection.
    store.fire('select-item', {item: store.idToNodeMap_['3']});
    assertDeepEquals(
        [false, true, false, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(1, store.anchorIndex_);

    // Deleting the selected item will unselect everything.
    store.selectedId = '1';
    store.fire('select-item', {item: store.idToNodeMap_['2']});
    removeChild(TEST_TREE.children[0], 0);
    overrideBookmarksGetSubTree([TEST_TREE.children[0]]);
    store.onBookmarkRemoved_('2', {parentId: '1', index: 0});
    assertDeepEquals(
        [false, false, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(null, store.anchorIndex_);

    // Changing the selected folder will remove the select status of the
    // bookmark.
    store.selectedId = '3';
    assertDeepEquals(
        [false, false, false],
        store.idToNodeMap_['1'].children.map(i => i.isSelectedItem));
    assertEquals(null, store.anchorIndex_);
  });

  test('shift select selects the correct bookmarks', function() {
    // When nothing has been selected, it selects a single item.
    assertEquals(null, store.anchorIndex_);
    store.fire('select-item', {item: store.idToNodeMap_['6'], range: true});
    assertDeepEquals(
        [false, false, true, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(2, store.anchorIndex_);

    // Select an item below the previous selected item.
    store.fire('select-item', {item: store.idToNodeMap_['7'], range: true});
    assertEquals(2, store.anchorIndex_);
    assertDeepEquals(
        [false, false, true, true],
        store.displayedList.map(i => i.isSelectedItem));

    // Select an item above the previous selected item.
    store.fire('select-item', {item: store.idToNodeMap_['2'], range: true});
    assertEquals(2, store.anchorIndex_);
    assertDeepEquals(
        [true, true, true, false],
        store.displayedList.map(i => i.isSelectedItem));
  });

  test('ctrl select selects the correct bookmarks', function() {
    // When nothing has been selected, it selects a single item.
    assertEquals(null, store.anchorIndex_);
    store.fire('select-item', {item: store.idToNodeMap_['6'], add: true});
    assertDeepEquals(
        [false, false, true, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(2, store.anchorIndex_);

    // Select a new item will not deselect the previous item, but will update
    // anchorIndex_.
    store.fire('select-item', {item: store.idToNodeMap_['2'], add: true});
    assertDeepEquals(
        [true, false, true, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(0, store.anchorIndex_);
  });

  test('shift + ctrl select selects the correct bookmarks', function() {
    store.fire('select-item', {item: store.displayedList[0]});
    store.fire(
        'select-item', {item: store.displayedList[2], add: true, range: false});
    store.fire(
        'select-item', {item: store.displayedList[3], add: true, range: true});
    assertDeepEquals(
        [true, false, true, true],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(2, store.anchorIndex_);
  });

  test('selection in search mode', function() {
    // Item gets unselected in search.
    overrideBookmarksSearch([
      createItem('4', {url: 'link4'}),
      createItem('2', {url: 'link2'}),
      createItem('5', {url: 'link5'}),
    ]);

    store.selectedId = '1';
    store.fire('select-item', {item: store.idToNodeMap_['3']});
    store.searchTerm = 'a';
    assertFalse(store.idToNodeMap_['3'].isSelectedItem);
    assertEquals(null, store.anchorIndex_);

    // anchorIndex_ gets updated properly in single select.
    store.fire('select-item', {item: store.displayedList[1]});
    assertDeepEquals(
        [false, true, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(1, store.anchorIndex_);

    // anchorIndex_ gets updated properly in ctrl select.
    store.fire('select-item', {item: store.displayedList[0], add: true});
    assertDeepEquals(
        [true, true, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(0, store.anchorIndex_);

    // Deleting the selected item will unselect everything.
    store.fire('select-item', {item: store.displayedList[1]});
    overrideBookmarksSearch([
      createItem('4', {url: 'link4'}),
      createItem('5', {url: 'link5'}),
    ]);
    removeChild(TEST_TREE.children[0], 0);
    overrideBookmarksGetSubTree([TEST_TREE.children[0]]);

    store.onBookmarkRemoved_('2', {parentId: '1', index: 0});
    assertDeepEquals(
        [false, false],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(null, store.anchorIndex_);

    // Shift+Ctrl select selects the right items.
    overrideBookmarksSearch([
      createItem('4', {url: 'link4'}),
      createFolder('3', []),
      createItem('5', {url: 'link5'}),
      createItem('6', {url: 'link4'}),
      createItem('7', {url: 'link5'}),
    ]);
    store.searchTerm = 'b';
    store.fire('select-item', {item: store.displayedList[0]});
    store.fire(
        'select-item', {item: store.displayedList[2], add: true, range: false});
    store.fire(
        'select-item', {item: store.displayedList[4], add: true, range: true});
    assertDeepEquals(
        [true, false, true, true, true],
        store.displayedList.map(i => i.isSelectedItem));
    assertEquals(2, store.anchorIndex_);
  });
});
