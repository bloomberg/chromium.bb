// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testModel() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel =
      new MockFolderShortcutDataModel(['/drive/root/shortcut']);
  var model = new NavigationListModel(volumeManager, shortcutListModel);
  assertEquals(3, model.length);
  assertEquals('/drive/root', model.item(0).path);
  assertEquals('/Downloads', model.item(1).path);
  assertEquals('/drive/root/shortcut', model.item(2).path);
}

function testAddAndRemoveShortcuts() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel =
      new MockFolderShortcutDataModel(['/drive/root/shortcut']);
  var model = new NavigationListModel(volumeManager, shortcutListModel);

  assertEquals(3, model.length);

  // Add a shortcut at the tail.
  shortcutListModel.splice(1, 0, '/drive/root/shortcut2');
  assertEquals(4, model.length);
  assertEquals('/drive/root/shortcut2', model.item(3).path);

  // Add a shortcut at the head.
  shortcutListModel.splice(0, 0, '/drive/root/hoge');
  assertEquals(5, model.length);
  assertEquals('/drive/root/hoge', model.item(2).path);
  assertEquals('/drive/root/shortcut', model.item(3).path);
  assertEquals('/drive/root/shortcut2', model.item(4).path);

  // Remove the last shortcut.
  shortcutListModel.splice(2, 1);
  assertEquals(4, model.length);
  assertEquals('/drive/root/hoge', model.item(2).path);
  assertEquals('/drive/root/shortcut', model.item(3).path);

  // Remove the first shortcut.
  shortcutListModel.splice(0, 1);
  assertEquals(3, model.length);
  assertEquals('/drive/root/shortcut', model.item(2).path);
}

function testAddAndRemoveVolumes() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel =
      new MockFolderShortcutDataModel(['/drive/root/shortcut']);
  var model = new NavigationListModel(volumeManager, shortcutListModel);

  assertEquals(3, model.length);

  // Removable volume 'hoge' is mounted.
  volumeManager.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      util.VolumeType.REMOVABLE, '/removable/hoge'));
  assertEquals(4, model.length);
  assertEquals('/drive/root', model.item(0).path);
  assertEquals('/Downloads', model.item(1).path);
  assertEquals('/removable/hoge', model.item(2).path);
  assertEquals('/drive/root/shortcut', model.item(3).path);

  // Removable volume 'fuga' is mounted.
  volumeManager.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      util.VolumeType.REMOVABLE, '/removable/fuga'));
  assertEquals(5, model.length);
  assertEquals('/drive/root', model.item(0).path);
  assertEquals('/Downloads', model.item(1).path);
  assertEquals('/removable/hoge', model.item(2).path);
  assertEquals('/removable/fuga', model.item(3).path);
  assertEquals('/drive/root/shortcut', model.item(4).path);

  // A shortcut is created on the 'hoge' volume.
  shortcutListModel.splice(1, 0, '/removable/hoge/shortcut2');
  assertEquals(6, model.length);
  assertEquals('/drive/root', model.item(0).path);
  assertEquals('/Downloads', model.item(1).path);
  assertEquals('/removable/hoge', model.item(2).path);
  assertEquals('/removable/fuga', model.item(3).path);
  assertEquals('/drive/root/shortcut', model.item(4).path);
  assertEquals('/removable/hoge/shortcut2', model.item(5).path);

  // The 'hoge' is unmounted. A shortcut on 'hoge' is removed.
  volumeManager.volumeInfoList.splice(2, 1);
  assertEquals(4, model.length);
  assertEquals('/drive/root', model.item(0).path);
  assertEquals('/Downloads', model.item(1).path);
  assertEquals('/removable/fuga', model.item(2).path);
  assertEquals('/drive/root/shortcut', model.item(3).path);

  // The Drive is unmounted. A shortcut on the Drive is removed.
  volumeManager.volumeInfoList.splice(0, 1);
  assertEquals(2, model.length);
  assertEquals('/Downloads', model.item(0).path);
  assertEquals('/removable/fuga', model.item(1).path);
}
