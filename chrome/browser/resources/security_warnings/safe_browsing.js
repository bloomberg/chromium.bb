// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Other constants defined in safe_browsing_blocking_page.cc.
var SB_BOX_CHECKED = 'boxchecked';
var SB_DISPLAY_CHECK_BOX = 'displaycheckbox';

// This sets up the Extended Safe Browsing Reporting opt-in.
function setupCheckbox() {
  if (loadTimeData.getString('type') != 'SAFEBROWSING' ||
      !loadTimeData.getBoolean(SB_DISPLAY_CHECK_BOX)) {
    return;
  }

  $('opt-in-label').innerHTML = loadTimeData.getString('optInLink');
  $('opt-in-checkbox').checked = loadTimeData.getBoolean(SB_BOX_CHECKED);
  $('malware-opt-in').classList.remove('hidden');
  $('body').classList.add('safe-browsing-has-checkbox');

  $('opt-in-checkbox').addEventListener('click', function() {
    sendCommand(
        $('opt-in-checkbox').checked ? CMD_DO_REPORT : CMD_DONT_REPORT);
  });
}
