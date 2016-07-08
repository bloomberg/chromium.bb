// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This structure maps the various cookie type names from C++ (hence the
// underscores) to arrays of the different types of data each has, along with
// the i18n name for the description of that data type.
// This structure serves three purposes:
// 1) to list what subset of the cookie data we want to show in the UI.
// 2) What order to show it in.
// 3) What user friendly label to prefix the data with.
/** @const */ var cookieInfo = {
  'cookie': [['name', 'cookieName'],
             ['content', 'cookieContent'],
             ['domain', 'cookieDomain'],
             ['path', 'cookiePath'],
             ['sendfor', 'cookieSendFor'],
             ['accessibleToScript', 'cookieAccessibleToScript'],
             ['created', 'cookieCreated'],
             ['expires', 'cookieExpires']],
  'app_cache': [['manifest', 'appCacheManifest'],
                ['size', 'localStorageSize'],
                ['created', 'cookieCreated'],
                ['accessed', 'cookieLastAccessed']],
  'database': [['name', 'cookieName'],
               ['desc', 'webdbDesc'],
               ['size', 'localStorageSize'],
               ['modified', 'localStorageLastModified']],
  'local_storage': [['origin', 'localStorageOrigin'],
                    ['size', 'localStorageSize'],
                    ['modified', 'localStorageLastModified']],
  'indexed_db': [['origin', 'indexedDbOrigin'],
                 ['size', 'indexedDbSize'],
                 ['modified', 'indexedDbLastModified']],
  'file_system': [['origin', 'fileSystemOrigin'],
                  ['persistent', 'fileSystemPersistentUsage'],
                  ['temporary', 'fileSystemTemporaryUsage']],
  'channel_id': [['serverId', 'channelIdServerId'],
                 ['certType', 'channelIdType'],
                 ['created', 'channelIdCreated']],
  'service_worker': [['origin', 'serviceWorkerOrigin'],
                     ['size', 'serviceWorkerSize'],
                     ['scopes', 'serviceWorkerScopes']],
  'cache_storage': [['origin', 'cacheStorageOrigin'],
                    ['size', 'cacheStorageSize'],
                    ['modified', 'cacheStorageLastModified']],
  'flash_lso': [['domain', 'cookieDomain']],
};
