// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  // Override VolumeInfo.prototype.resolveDisplayRoot.
  VolumeInfo.prototype.resolveDisplayRoot = function() {};
}

function testModel() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry('drive', '/root/shortcut')]);
  var model = new NavigationListModel(volumeManager, shortcutListModel);

  assertEquals(3, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(2).entry.fullPath);
}

function testAddAndRemoveShortcuts() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry('drive', '/root/shortcut')]);
  var model = new NavigationListModel(volumeManager, shortcutListModel);

  assertEquals(3, model.length);

  // Add a shortcut at the tail.
  shortcutListModel.splice(1, 0, new MockFileEntry('drive', '/root/shortcut2'));
  assertEquals(4, model.length);
  assertEquals('/root/shortcut2', model.item(3).entry.fullPath);

  // Add a shortcut at the head.
  shortcutListModel.splice(0, 0, new MockFileEntry('drive', '/root/hoge'));
  assertEquals(5, model.length);
  assertEquals('/root/hoge', model.item(2).entry.fullPath);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);
  assertEquals('/root/shortcut2', model.item(4).entry.fullPath);

  // Remove the last shortcut.
  shortcutListModel.splice(2, 1);
  assertEquals(4, model.length);
  assertEquals('/root/hoge', model.item(2).entry.fullPath);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);

  // Remove the first shortcut.
  shortcutListModel.splice(0, 1);
  assertEquals(3, model.length);
  assertEquals('/root/shortcut', model.item(2).entry.fullPath);
}

function testAddAndRemoveVolumes() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry('drive', '/root/shortcut')]);
  var model = new NavigationListModel(volumeManager, shortcutListModel);

  assertEquals(3, model.length);

  // Removable volume 'hoge' is mounted.
  volumeManager.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge'));
  assertEquals(4, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);

  // Removable volume 'fuga' is mounted.
  volumeManager.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga'));
  assertEquals(5, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(3).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(4).entry.fullPath);

  // A shortcut is created on the 'hoge' volume.
  shortcutListModel.splice(
      1, 0, new MockFileEntry('removable:hoge', '/shortcut2'));
  assertEquals(6, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(3).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(4).entry.fullPath);
  assertEquals('/shortcut2', model.item(5).entry.fullPath);
}
