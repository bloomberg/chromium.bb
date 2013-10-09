// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for URL constants.
 */
var urlConstants = {};

/**
 * Location of the FAQ about the downloads directory.
 * @const {string}
 */
urlConstants.DOWNLOADS_FAQ_URL =
    'http://support.google.com/chromeos/bin/answer.py?answer=1061547';

/**
 * Location of Files App specific help.
 * @const {string}
 */
urlConstants.FILES_APP_HELP =
    'https://support.google.com/chromeos/?p=gsg_files_app';

/**
 * Location of the page to buy more storage for Google Drive.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_BUY_STORAGE =
    'https://www.google.com/settings/storage';

/**
 * Location of the help page about connecting to Google Drive.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_ERROR_HELP_URL =
    'https://support.google.com/chromeos/?p=filemanager_driveerror';

/**
 * Location of the FAQ about Google Drive.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_FAQ_URL =
    'https://support.google.com/chromeos/?p=filemanager_drive';

/**
 * Location of Google Drive specific help.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_HELP =
    'https://support.google.com/chromeos/?p=filemanager_drivehelp';

/**
 * Location of Google drive redeem page.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_REDEEM =
    'http://www.google.com/intl/en/chrome/devices/goodies.html' +
    '?utm_medium=filesappbanner';

/**
 * Location of Google Drive root.
 * @const {string}
 */
urlConstants.GOOGLE_DRIVE_ROOT = 'https://drive.google.com';

// Make the namespace immutable.
Object.freeze(urlConstants);
