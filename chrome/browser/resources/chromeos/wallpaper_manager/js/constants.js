// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @const */ var Constants = {
  /**
   * Key to access wallpaper rss in chrome.local.storage.
   */
  AccessRssKey: 'wallpaper-picker-surprise-rss-key',

  /**
   * Key to access wallpaper manifest in chrome.local.storage.
   */
  AccessManifestKey: 'wallpaper-picker-manifest-key',

  /**
   * Key to access last changed date of a surprise wallpaper.
   */
  AccessLastSurpriseWallpaperChangedDate: 'wallpaper-last-changed-date-key',

  /**
   * Key to access if surprise me feature is enabled or not in
   * chrome.local.storage.
   */
  AccessSurpriseMeEnabledKey: 'surprise-me-enabled-key',

  /**
   * Suffix to append to baseURL if requesting high resoultion wallpaper.
   */
  HighResolutionSuffix: '_high_resolution.jpg',

  /**
   * URL to get latest wallpaper RSS feed.
   */
  WallpaperRssURL: 'https://commondatastorage.googleapis.com/' +
      'chromeos-wallpaper-public/wallpaper.rss',

  /**
   * cros-wallpaper namespace URI.
   */
  WallpaperNameSpaceURI: 'http://commondatastorage.googleapis.com/' +
      'chromeos-wallpaper-public/cros-wallpaper-uri',

  /**
   * Wallpaper sources enum.
   */
  WallpaperSourceEnum: {
      Online: 'ONLINE',
      Custom: 'CUSTOM',
      AddNew: 'ADDNEW'
  },

  /**
   * Local storage.
   */
  WallpaperLocalStorage: chrome.storage.local
};
