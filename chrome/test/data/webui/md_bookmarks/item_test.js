// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-item>', function() {
  var item;
  var TEST_ITEM = createItem('0');

  setup(function() {
    item = document.createElement('bookmarks-item');
    replaceBody(item);
    item.item = TEST_ITEM;
  });

  test('changing the url changes the favicon', function() {
    var favicon = item.$.icon.style.backgroundImage;
    item.set('item.url', 'http://www.mail.google.com');
    assertNotEquals(favicon, item.$.icon.style.backgroundImage);
  });

  test('changing isFolder_ hides/unhides the folder/icon', function() {
    item.isFolder_ = true;
    assertFalse(item.$['folder-icon'].hidden);
    assertTrue(item.$.icon.hidden);

    item.isFolder_ = false;
    assertTrue(item.$['folder-icon'].hidden);
    assertFalse(item.$.icon.hidden);
  });

  test('pressing the buttons fires the right event', function() {
    var counter = [0, 0, 0];
    document.addEventListener('select-item', function(e) {
      if (e.detail.range)
        counter[0]++;
      else if (e.detail.add)
        counter[1]++;
      else
        counter[2]++;
    });

    customClick(item);
    assertDeepEquals([0, 0, 1], counter);

    customClick(item, {shiftKey: true});
    assertDeepEquals([1, 0, 1], counter);

    customClick(item, {ctrlKey: true});
    assertDeepEquals([1, 1, 1], counter);

    customClick(item, {shiftKey: true, ctrlKey: true});
    assertDeepEquals([2, 1, 1], counter);
  });
});
