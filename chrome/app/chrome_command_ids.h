// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file lists all the command IDs understood by e.g. the browser.
// It is used by Windows RC files, Mac NIB files, and other platforms too.

// Values below IDC_MinimumLabelValue are reserved for dynamic menu items.
#define IDC_MinimumLabelValue           4000

#define IDC_Messages                    4000
#define IDC_MessagesAll                 4007
#define IDC_MessagesNone                4008

// NOTE: Within each of the following sections, the IDs are ordered roughly by
// how they appear in the GUI/menus (left to right, top to bottom, etc.).

// Navigation commands
// TODO: Reorder to be in visible order; collapse holes
#define IDC_BACK                        33000
#define IDC_FORWARD                     33001
#define IDC_RELOAD                      33002
#define IDC_HOME                        33003
#define IDC_OPEN_CURRENT_URL            33004
#define IDC_STOP                        33006
#define IDC_RELOAD_IGNORING_CACHE       33007

// Window management commands
#define IDC_NEW_WINDOW                  34000
#define IDC_NEW_INCOGNITO_WINDOW        34001
#define IDC_CLOSE_WINDOW                34012
#define IDC_ALWAYS_ON_TOP               34013
#define IDC_NEW_TAB                     34014
#define IDC_CLOSE_TAB                   34015
#define IDC_SELECT_NEXT_TAB             34016
#define IDC_SELECT_PREVIOUS_TAB         34017
#define IDC_SELECT_TAB_0                34018
#define IDC_SELECT_TAB_1                34019
#define IDC_SELECT_TAB_2                34020
#define IDC_SELECT_TAB_3                34021
#define IDC_SELECT_TAB_4                34022
#define IDC_SELECT_TAB_5                34023
#define IDC_SELECT_TAB_6                34024
#define IDC_SELECT_TAB_7                34025
#define IDC_SELECT_LAST_TAB             34026
#define IDC_DUPLICATE_TAB               34027
#define IDC_RESTORE_TAB                 34028
#define IDC_SHOW_AS_TAB                 34029
#define IDC_FULLSCREEN                  34030
#define IDC_EXIT                        34031
#define IDC_MOVE_TAB_NEXT               34032
#define IDC_MOVE_TAB_PREVIOUS           34033
#define IDC_TOGGLE_VERTICAL_TABS        34034
#define IDC_SEARCH                      34035
#define IDC_TABPOSE                     34036

// Page-related commands
#define IDC_BOOKMARK_PAGE               35000
#define IDC_BOOKMARK_ALL_TABS           35001
#define IDC_VIEW_SOURCE                 35002
#define IDC_PRINT                       35003
#define IDC_SAVE_PAGE                   35004
#define IDC_ENCODING_MENU               35005
#define IDC_EMAIL_PAGE_LOCATION         35006

// When adding a new encoding to this list, be sure to append it to the
// EncodingMenuController::kValidEncodingIds array in
// encoding_menu_controller.cc.
#define IDC_ENCODING_AUTO_DETECT        35500
#define IDC_ENCODING_UTF8               35501
#define IDC_ENCODING_UTF16LE            35502
#define IDC_ENCODING_ISO88591           35503
#define IDC_ENCODING_WINDOWS1252        35504
#define IDC_ENCODING_GBK                35505
#define IDC_ENCODING_GB18030            35506
#define IDC_ENCODING_BIG5               35507
#define IDC_ENCODING_BIG5HKSCS          35508
#define IDC_ENCODING_KOREAN             35509
#define IDC_ENCODING_SHIFTJIS           35510
#define IDC_ENCODING_ISO2022JP          35511
#define IDC_ENCODING_EUCJP              35512
#define IDC_ENCODING_THAI               35513
#define IDC_ENCODING_ISO885915          35514
#define IDC_ENCODING_MACINTOSH          35515
#define IDC_ENCODING_ISO88592           35516
#define IDC_ENCODING_WINDOWS1250        35517
#define IDC_ENCODING_ISO88595           35518
#define IDC_ENCODING_WINDOWS1251        35519
#define IDC_ENCODING_KOI8R              35520
#define IDC_ENCODING_KOI8U              35521
#define IDC_ENCODING_ISO88597           35522
#define IDC_ENCODING_WINDOWS1253        35523
#define IDC_ENCODING_ISO88594           35524
#define IDC_ENCODING_ISO885913          35525
#define IDC_ENCODING_WINDOWS1257        35526
#define IDC_ENCODING_ISO88593           35527
#define IDC_ENCODING_ISO885910          35528
#define IDC_ENCODING_ISO885914          35529
#define IDC_ENCODING_ISO885916          35530
#define IDC_ENCODING_WINDOWS1254        35531
#define IDC_ENCODING_ISO88596           35532
#define IDC_ENCODING_WINDOWS1256        35533
#define IDC_ENCODING_ISO88598           35534
#define IDC_ENCODING_WINDOWS1255        35535
#define IDC_ENCODING_WINDOWS1258        35536
#define IDC_ENCODING_ISO88598I          35537

// Clipboard commands
#define IDC_CUT                         36000
#define IDC_COPY                        36001
#define IDC_COPY_URL                    36002
#define IDC_PASTE                       36003
#define IDC_EDIT_MENU                   36004

// Find-in-page
#define IDC_FIND                        37000
#define IDC_FIND_NEXT                   37001
#define IDC_FIND_PREVIOUS               37002

// Zoom
#define IDC_ZOOM_MENU                   38000
#define IDC_ZOOM_PLUS                   38001
#define IDC_ZOOM_NORMAL                 38002
#define IDC_ZOOM_MINUS                  38003
#define IDC_ZOOM_PERCENT_DISPLAY        38004

// Focus various bits of UI
#define IDC_FOCUS_TOOLBAR               39000
#define IDC_FOCUS_LOCATION              39001
#define IDC_FOCUS_SEARCH                39002
#define IDC_FOCUS_MENU_BAR              39003
#define IDC_FOCUS_NEXT_PANE             39004
#define IDC_FOCUS_PREVIOUS_PANE         39005
#define IDC_FOCUS_BOOKMARKS             39006
#define IDC_FOCUS_CHROMEOS_STATUS       39007

// Show various bits of UI
#define IDC_OPEN_FILE                   40000
#define IDC_CREATE_SHORTCUTS            40001
#define IDC_DEVELOPER_MENU              40002
#define IDC_DEV_TOOLS                   40003
#define IDC_DEV_TOOLS_CONSOLE           40004
#define IDC_TASK_MANAGER                40005
#define IDC_FEEDBACK                    40008
#define IDC_SHOW_BOOKMARK_BAR           40009
#define IDC_SHOW_HISTORY                40010
#define IDC_SHOW_BOOKMARK_MANAGER       40011
#define IDC_SHOW_DOWNLOADS              40012
#define IDC_CLEAR_BROWSING_DATA         40013
#define IDC_IMPORT_SETTINGS             40014
#define IDC_OPTIONS                     40015
#define IDC_EDIT_SEARCH_ENGINES         40016
#define IDC_VIEW_PASSWORDS              40017
#define IDC_ABOUT                       40018
#define IDC_HELP_PAGE                   40019
#define IDC_SHOW_APP_MENU               40020
#define IDC_MANAGE_EXTENSIONS           40022
#define IDC_AUTOFILL_DEFAULT            40023
#define IDC_DEV_TOOLS_INSPECT           40025
#define IDC_UPGRADE_DIALOG              40026
#define IDC_VIEW_INCOMPATIBILITIES      40027
#define IDC_VIEW_BACKGROUND_PAGES       40028
#define IDC_SHOW_KEYBOARD_OVERLAY       40029
#define IDC_PROFILING_ENABLED           40030

// Spell-check
// Insert any additional suggestions before _LAST; these have to be consecutive.
#define IDC_SPELLCHECK_SUGGESTION_0     41000
#define IDC_SPELLCHECK_SUGGESTION_1     41001
#define IDC_SPELLCHECK_SUGGESTION_2     41002
#define IDC_SPELLCHECK_SUGGESTION_3     41003
#define IDC_SPELLCHECK_SUGGESTION_4     41004
#define IDC_SPELLCHECK_SUGGESTION_LAST  IDC_SPELLCHECK_SUGGESTION_4
#define IDC_SPELLCHECK_MENU             41005
// Language entries are inserted using autogenerated values between
// [_FIRST, _LAST).
#define IDC_SPELLCHECK_LANGUAGES_FIRST  41006
#define IDC_SPELLCHECK_LANGUAGES_LAST   41106
#define IDC_CHECK_SPELLING_OF_THIS_FIELD  41107
#define IDC_SYNC_BOOKMARKS              41108
#define IDC_SPELLPANEL_TOGGLE           41109
#define IDC_SPELLCHECK_ADD_TO_DICTIONARY 41110

// Writing direction
#define IDC_WRITING_DIRECTION_MENU       41120     // OSX only
#define IDC_WRITING_DIRECTION_DEFAULT    41121     // OSX only
#define IDC_WRITING_DIRECTION_LTR        41122     // OSX only
#define IDC_WRITING_DIRECTION_RTL        41123     // OSX only

// Remoting
#define IDC_REMOTING_SETUP               41140

// Translate
#define IDC_TRANSLATE_OPTIONS_ALWAYS                42000
#define IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG  42001
#define IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE  42002
#define IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION 42003
#define IDC_TRANSLATE_OPTIONS_ABOUT                 42004
#define IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE        42100
#define IDC_TRANSLATE_TARGET_LANGUAGE_BASE          42400

// Identifiers for platform-specific items.
// Placed in a common file to help insure they never collide.
#define IDC_BOOKMARK_MENU               43000     // OSX only
#define IDC_VIEW_MENU                   44000     // OSX only
#define IDC_SYSTEM_OPTIONS              45000     // ChromeOS only
#define IDC_INTERNET_OPTIONS            45100     // ChromeOS only
#define IDC_LANGUAGE_OPTIONS            45200     // ChromeOS only
#define IDC_HISTORY_MENU                46000     // OSX only
#define IDC_INPUT_METHODS_MENU          46300     // Linux only

// Custom context menu entries
#define IDC_CONTENT_CONTEXT_CUSTOM_FIRST 47000
#define IDC_CONTENT_CONTEXT_CUSTOM_LAST  48000

// Context menu items added by extensions.
#define IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST 49000
#define IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST 50000

// Context menu items in the render view.
// Link items.
#define IDC_CONTENT_CONTEXT_OPENLINKNEWTAB 50100
#define IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW 50101
#define IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD 50102
#define IDC_CONTENT_CONTEXT_SAVELINKAS 50103
#define IDC_CONTENT_CONTEXT_COPYLINKLOCATION 50104
#define IDC_CONTENT_CONTEXT_COPYEMAILADDRESS 50105
// Image items.
#define IDC_CONTENT_CONTEXT_SAVEIMAGEAS 50110
#define IDC_CONTENT_CONTEXT_COPYIMAGELOCATION 50111
#define IDC_CONTENT_CONTEXT_COPYIMAGE 50112
#define IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB 50113
// Audio/video items.
#define IDC_CONTENT_CONTEXT_SAVEAVAS 50120
#define IDC_CONTENT_CONTEXT_COPYAVLOCATION 50121
#define IDC_CONTENT_CONTEXT_OPENAVNEWTAB 50122
// Media items.
#define IDC_CONTENT_CONTEXT_PLAYPAUSE 50130
#define IDC_CONTENT_CONTEXT_MUTE 50131
#define IDC_CONTENT_CONTEXT_LOOP 50132
#define IDC_CONTENT_CONTEXT_CONTROLS 50133
// Edit items.
#define IDC_CONTENT_CONTEXT_COPY 50140
#define IDC_CONTENT_CONTEXT_CUT 50141
#define IDC_CONTENT_CONTEXT_PASTE 50142
#define IDC_CONTENT_CONTEXT_DELETE 50143
#define IDC_CONTENT_CONTEXT_UNDO 50144
#define IDC_CONTENT_CONTEXT_REDO 50145
#define IDC_CONTENT_CONTEXT_SELECTALL 50146
// Other items.
#define IDC_CONTENT_CONTEXT_TRANSLATE 50150
#define IDC_CONTENT_CONTEXT_INSPECTELEMENT 50151
#define IDC_CONTENT_CONTEXT_VIEWPAGEINFO 50152
#define IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS 50153
#define IDC_CONTENT_CONTEXT_LOOK_UP_IN_DICTIONARY 50154
// Frame items.
#define IDC_CONTENT_CONTEXT_RELOADFRAME 50160
#define IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE 50161
#define IDC_CONTENT_CONTEXT_VIEWFRAMEINFO 50162
// Search items.
#define IDC_CONTENT_CONTEXT_GOTOURL 50170
#define IDC_CONTENT_CONTEXT_SEARCHWEBFOR 50171

// Context menu items in the bookmark bar
#define IDC_BOOKMARK_BAR_OPEN_ALL 51000
#define IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW 51001
#define IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO 51002
#define IDC_BOOKMARK_BAR_OPEN_INCOGNITO 51003
#define IDC_BOOKMARK_BAR_RENAME_FOLDER 51004
#define IDC_BOOKMARK_BAR_EDIT 51005
#define IDC_BOOKMARK_BAR_REMOVE 51006
#define IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK 51007
#define IDC_BOOKMARK_BAR_NEW_FOLDER 51008
#define IDC_BOOKMARK_MANAGER 51009
#define IDC_BOOKMARK_BAR_ALWAYS_SHOW 51010
