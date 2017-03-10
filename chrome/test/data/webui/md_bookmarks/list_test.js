// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-list>', function() {
  var list;
  var store;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '0',
          [
            createItem('1'),
            createFolder('3', []),
            createItem('5'),
            createItem('7'),
          ])),
      selectedFolder: '0',
    });
    bookmarks.Store.instance_ = store;

    list = document.createElement('bookmarks-list');
    replaceBody(list);
    Polymer.dom.flush();
  });

  test('folder menu item hides the url field', function() {
    // Bookmark editor shows the url field.
    list.menuItem_ = store.data.nodes['1'];
    assertFalse(list.$['url'].hidden);

    // Folder editor hides the url field.
    list.menuItem_ = store.data.nodes['3'];
    assertTrue(list.$['url'].hidden);
  });

  test('saving edit passes correct details to the update', function() {
    // Saving bookmark edit.
    var menuItem = store.data.nodes['1'];
    chrome.bookmarks.update = function(id, edit) {
      assertEquals(menuItem.id, id);
      assertEquals(menuItem.url, edit.url);
      assertEquals(menuItem.title, edit.title);
    };
    list.menuItem_ = menuItem;
    list.$.editBookmark.showModal();
    MockInteractions.tap(list.$.saveButton);

    // Saving folder rename.
    menuItem = store.data.nodes['3'];
    chrome.bookmarks.update = function(id, edit) {
      assertEquals(menuItem.id, id);
      assertEquals(menuItem.title, edit.title);
      assertEquals(undefined, edit.url);
    };
    list.menuItem_ = menuItem;
    list.$.editBookmark.showModal();
    MockInteractions.tap(list.$.saveButton);
  });

  test('renders correct <bookmark-item> elements', function() {
    var items = list.root.querySelectorAll('bookmarks-item');
    var ids = Array.from(items).map((item) => item.itemId);

    assertDeepEquals(['1', '3', '5', '7'], ids);
  });
});
