// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (document.location != 'chrome://settings-frame/options_settings_app.html')
    return;

  document.documentElement.classList.add('settings-app');

  // Override the offset in the options page.
  OptionsPage.setHorizontalOffset(38);

  loadTimeData.overrideValues(loadTimeData.getValue('settingsApp'));
}());
