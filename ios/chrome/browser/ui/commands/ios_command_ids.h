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
#define IDC_VIEW_SOURCE                                35002
#define IDC_FIND                                       37000
#define IDC_FIND_NEXT                                  37001
#define IDC_FIND_PREVIOUS                              37002
#define IDC_SHOW_HISTORY                               40010
#define IDC_SHOW_BOOKMARK_MANAGER                      40011
#define IDC_HELP_PAGE_VIA_MENU                         40020
#define IDC_TOGGLE_TAB_SWITCHER                        40901
#define IDC_VOICE_SEARCH                               40902
#define IDC_CLOSE_ALL_TABS                             40904
#define IDC_SHOW_SIGNIN_IOS                            40905
#define IDC_FIND_CLOSE                                 40907
#define IDC_FIND_UPDATE                                40908
#define IDC_CLOSE_MODALS                               40909
#define IDC_SHOW_ADD_ACCOUNT                           40910
#define IDC_SHOW_PAGE_INFO                             40911
#define IDC_HIDE_PAGE_INFO                             40912
#define IDC_SHOW_SECURITY_HELP                         40913
#define IDC_SHOW_SYNC_SETTINGS                         40914
#define IDC_OPEN_URL                                   40915
#define IDC_SHOW_OTHER_DEVICES                         40917
#define IDC_CLOSE_ALL_INCOGNITO_TABS                   40918
#define IDC_CLOSE_SETTINGS_AND_OPEN_URL                40920
#define IDC_REQUEST_DESKTOP_SITE                       40921
#define IDC_REQUEST_MOBILE_SITE                        40922
#define IDC_CLEAR_BROWSING_DATA_IOS                    40924
#define IDC_SHOW_MAIL_COMPOSER                         40926
#define IDC_BACK_FORWARD_IN_TAB_HISTORY                40930
#define IDC_REPORT_AN_ISSUE                            40936
#define IDC_PRELOAD_VOICE_SEARCH                       40937
#define IDC_SHOW_BACK_HISTORY                          40938
#define IDC_SHOW_FORWARD_HISTORY                       40939
#define IDC_CLOSE_SETTINGS                             40944
#define IDC_SHOW_SAVE_PASSWORDS_SETTINGS               40945
#define IDC_READER_MODE                                40947
#define IDC_RATE_THIS_APP                              40948
#define IDC_SHOW_READING_LIST                          40950
#define IDC_SHOW_CLEAR_BROWSING_DATA_SETTINGS          40951
#define IDC_SHOW_SYNC_PASSPHRASE_SETTINGS              40952
#define IDC_SHOW_QR_SCANNER                            40953
#define IDC_SHOW_AUTOFILL_SETTINGS                     40956
// clang-format on

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_IOS_COMMAND_IDS_H_
