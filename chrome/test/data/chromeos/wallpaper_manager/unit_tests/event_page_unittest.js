// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var mockController;

function setUp() {
  mockController = new MockController();
  installMockXMLHttpRequest();
}

function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
  uninstallMockXMLHttpRequest();
}

// Test sync online wallpaper. When the synced wallpaper info is not the same as
// the local wallpaper info, wallpaper should set to the synced one.
function testSyncOnlineWallpaper() {
  // Setup sync value.
  var changes = {};
  changes[Constants.AccessSyncWallpaperInfoKey] = {};
  changes[Constants.AccessSyncWallpaperInfoKey].newValue = {
    'url': TestConstants.wallpaperURL,
    'layout': 'custom',
    'source': Constants.WallpaperSourceEnum.Online
  };

  chrome.wallpaperPrivate = {};
  var mockSetWallpaperIfExists = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setWallpaperIfExists');
  mockSetWallpaperIfExists.addExpectation(
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.url,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.layout);
  mockSetWallpaperIfExists.callbackData = [false];

  var mockSetWallpaper = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setWallpaper');
  mockSetWallpaper.addExpectation(
      TestConstants.image,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.layout,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.url);

  chrome.storage.onChanged.dispatch(changes);
}
