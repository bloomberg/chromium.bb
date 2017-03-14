// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-item>', function() {
  var item;
  var store;
  var TEST_ITEM = createItem('0');

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder('1', [createItem(['2'])])),
    });
    bookmarks.Store.instance_ = store;

    item = document.createElement('bookmarks-item');
    item.itemId = '2';
    replaceBody(item);
  });

  test('changing the url changes the favicon', function() {
    var favicon = item.$.icon.style.backgroundImage;
    store.data.nodes['2'] = createItem('0', {url: 'https://mail.google.com'});
    store.notifyObservers();
    assertNotEquals(favicon, item.$.icon.style.backgroundImage);
  });

  test('changing to folder hides/unhides the folder/icon', function() {
    // Starts test as an item.
    assertTrue(item.$['folder-icon'].hidden);
    assertFalse(item.$.icon.hidden);

    // Change to a folder.
    item.itemId = '1';

    assertFalse(item.$['folder-icon'].hidden);
    assertTrue(item.$.icon.hidden);
  });
});
