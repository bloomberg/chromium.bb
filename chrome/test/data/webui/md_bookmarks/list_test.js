// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-list>', function() {
  var list;
  var TEST_LIST =
      [createItem('0'), createItem('1'), createFolder('2', [], null)];

  setup(function() {
    list = document.createElement('bookmarks-list');
    replaceBody(list);
    list.displayedList = TEST_LIST;
  });

  test('folder menu item hides the url field', function() {
    // Bookmark editor shows the url field.
    list.menuItem_ = TEST_LIST[0];
    assertFalse(list.$['url'].hidden);

    // Folder editor hides the url field.
    list.menuItem_ = TEST_LIST[2];
    assertTrue(list.$['url'].hidden);
  });

  test('saving edit passes correct details to the update', function() {
    // Saving bookmark edit.
    var menuItem = TEST_LIST[0];
    chrome.bookmarks.update = function(id, edit) {
      assertEquals(menuItem.id, id);
      assertEquals(menuItem.url, edit.url);
      assertEquals(menuItem.title, edit.title);
    };
    list.menuItem_ = menuItem;
    list.$.editBookmark.showModal();
    MockInteractions.tap(list.$.saveButton);

    // Saving folder rename.
    menuItem = TEST_LIST[2];
    chrome.bookmarks.update = function(id, edit) {
      assertEquals(menuItem.id, id);
      assertEquals(menuItem.title, edit.title);
      assertEquals(undefined, edit.url);
    };
    list.menuItem_ = menuItem;
    list.$.editBookmark.showModal();
    MockInteractions.tap(list.$.saveButton);
  });
});
