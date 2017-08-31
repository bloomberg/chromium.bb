// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_IOS_COMMAND_IDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_IOS_COMMAND_IDS_H_

// This file lists all the command IDs understood by e.g. the browser.
// It is used by NIB files.

// NIB files include ID numbers rather than the corresponding #define labels.
// If you change a given command's number, any NIB files that refer to it will
// also need to be updated.

// clang-format off
#define IDC_SHOW_BOOKMARK_MANAGER                      40011
#define IDC_SHOW_SIGNIN_IOS                            40905
#define IDC_SHOW_ADD_ACCOUNT                           40910
#define IDC_SHOW_SYNC_SETTINGS                         40914
#define IDC_SHOW_OTHER_DEVICES                         40917
#define IDC_REQUEST_DESKTOP_SITE                       40921
#define IDC_REQUEST_MOBILE_SITE                        40922
#define IDC_CLEAR_BROWSING_DATA_IOS                    40924
#define IDC_SHOW_MAIL_COMPOSER                         40926
#define IDC_RATE_THIS_APP                              40948
#define IDC_SHOW_SYNC_PASSPHRASE_SETTINGS              40952
// clang-format on

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_IOS_COMMAND_IDS_H_
