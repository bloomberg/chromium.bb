// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('management', function() {
  document.title = loadTimeData.getString('title');
  cr.addWebUIListener(
      'browser-reporting-info-updated',
      () => document.title = loadTimeData.getString('title'));
});
