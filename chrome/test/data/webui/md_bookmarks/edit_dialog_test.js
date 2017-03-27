// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-edit-dialog>', function() {
  var dialog;
  var lastUpdate;

  suiteSetup(function() {
    chrome.bookmarks.update = function(id, edit) {
      lastUpdate.id = id;
      lastUpdate.edit = edit;
    }
  });

  setup(function() {
    lastUpdate = {};
    dialog = document.createElement('bookmarks-edit-dialog');
    replaceBody(dialog);
  });

  test('editing an item shows the url field', function() {
    var item = createItem('0');
    dialog.showEditDialog(item);

    assertFalse(dialog.$.url.hidden);
  });

  test('editing a folder hides the url field', function() {
    var folder = createFolder('0', []);
    dialog.showEditDialog(folder);

    assertTrue(dialog.$.url.hidden);
  });

  test('editing passes the correct details to the update', function() {
    // Editing an item without changing anything.
    var item = createItem('1', {url: 'http://website.com', title: 'website'});
    dialog.showEditDialog(item);

    MockInteractions.tap(dialog.$.saveButton);

    assertEquals(item.id, lastUpdate.id);
    assertEquals(item.url, lastUpdate.edit.url);
    assertEquals(item.title, lastUpdate.edit.title);

    // Editing a folder, changing the title.
    var folder = createFolder('2', [], {title: 'Cool Sites'});
    dialog.showEditDialog(folder);
    dialog.titleValue_ = 'Awesome websites';

    MockInteractions.tap(dialog.$.saveButton);

    assertEquals(folder.id, lastUpdate.id);
    assertEquals(undefined, lastUpdate.edit.url);
    assertEquals('Awesome websites', lastUpdate.edit.title);
  });

  test('pressing enter saves and closes dialog', function() {
    var item = createItem('1', {url: 'http://www.example.com'});
    dialog.showEditDialog(item);

    MockInteractions.pressEnter(dialog.$.url);
    assertFalse(dialog.$.dialog.open);
  });

  test('validates urls correctly', function() {
    dialog.urlValue_ = 'http://www.example.com';
    assertTrue(dialog.validateUrl_());

    dialog.urlValue_ = 'https://a@example.com:8080';
    assertTrue(dialog.validateUrl_());

    dialog.urlValue_ = 'example.com';
    assertTrue(dialog.validateUrl_());
    assertEquals('http://example.com', dialog.urlValue_);

    dialog.urlValue_ = '';
    assertFalse(dialog.validateUrl_());

    dialog.urlValue_ = '~~~example.com~~~';
    assertFalse(dialog.validateUrl_());
  });

  test('doesn\'t save when URL is invalid', function() {
    var item = createItem('0');
    dialog.showEditDialog(item);

    dialog.urlValue_ = '';

    MockInteractions.tap(dialog.$.saveButton);

    assertTrue(dialog.$.url.invalid);
    assertTrue(dialog.$.dialog.open);
  });
});
