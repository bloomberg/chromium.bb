// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_NAMES_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_NAMES_H_

class CommandNames {
 public:
  static const char kNewSession[];
  static const char kStatus[];
  static const char kClose[];
  static const char kQuit[];
  static const char kGet[];
  static const char kGoBack[];
  static const char kGoForward[];
  static const char kRefresh[];
  static const char kAddCookie[];
  static const char kGetCookie[];
  static const char kGetCookies[];
  static const char kDeleteCookie[];
  static const char kDeleteAllCookies[];
  static const char kFindElement[];
  static const char kFindElements[];
  static const char kFindChildElement[];
  static const char kFindChildElements[];
  static const char kClearElement[];
  static const char kClickElement[];
  static const char kHoverOverElement[];
  static const char kSendKeysToElement[];
  static const char kSendKeysToActiveElement[];
  static const char kSubmitElement[];
  static const char kUploadFile[];
  static const char kGetCurrentWindowHandle[];
  static const char kGetWindowHandles[];
  static const char kSwitchToWindow[];
  static const char kSwitchToFrame[];
  static const char kGetActiveElement[];
  static const char kGetCurrentUrl[];
  static const char kGetPageSource[];
  static const char kGetTitle[];
  static const char kExecuteScript[];
  static const char kExecuteAsyncScript[];
  static const char kSetBrowserVisible[];
  static const char kIsBrowserVisible[];
  static const char kGetElementText[];
  static const char kGetElementValue[];
  static const char kGetElementTagName[];
  static const char kDragElement[];
  static const char kIsElementSelected[];
  static const char kIsElementEnabled[];
  static const char kIsElementDisplayed[];
  static const char kGetElementLocation[];
  static const char kGetElementLocationOnceScrolledIntoView[];
  static const char kGetElementSize[];
  static const char kGetElementAttribute[];
  static const char kGetElementValueOfCssProperty[];
  static const char kElementEquals[];
  static const char kScreenshot[];
  static const char kGetAlert[];
  static const char kAcceptAlert[];
  static const char kDismissAlert[];
  static const char kGetAlertText[];
  static const char kSetAlertValue[];
  static const char kSetTimeout[];
  static const char kImplicitlyWait[];
  static const char kSetScriptTimeout[];
  static const char kExecuteSQL[];
  static const char kGetLocation[];
  static const char kSetLocation[];
  static const char kGetAppCache[];
  static const char kGetStatus[];
  static const char kClearAppCache[];
  static const char kIsBrowserOnline[];
  static const char kSetBrowserOnline[];
  static const char kGetLocalStorageItem[];
  static const char kGetLocalStorageKeys[];
  static const char kSetLocalStorageItem[];
  static const char kRemoveLocalStorageItem[];
  static const char kClearLocalStorage[];
  static const char kGetLocalStorageSize[];
  static const char kGetSessionStorageItem[];
  static const char kGetSessionStorageKey[];
  static const char kSetSessionStorageItem[];
  static const char kRemoveSessionStorageItem[];
  static const char kClearSessionStorage[];
  static const char kGetSessionStorageSize[];
  static const char kSetScreenOrientation[];
  static const char kGetScreenOrientation[];
  static const char kMouseClick[];
  static const char kMouseDoubleClick[];
  static const char kMouseButtonDown[];
  static const char kMouseButtonUp[];
  static const char kMouseMoveTo[];
  static const char kSendKeys[];
  static const char kImeGetAvailableEngines[];
  static const char kImeGetActiveEngine[];
  static const char kImeIsActivated[];
  static const char kImeDeactivate[];
  static const char kImeActivateEngine[];
  static const char kTouchSingleTap[];
  static const char kTouchDown[];
  static const char kTouchUp[];
  static const char kTouchMove[];
  static const char kTouchScroll[];
  static const char kTouchDoubleTap[];
  static const char kTouchLongPress[];
  static const char kTouchFlick[];
  static const char kSetWindowSize[];
  static const char kSetWindowPosition[];
  static const char kGetWindowSize[];
  static const char kGetWindowPosition[];
  static const char kMaximizeWindow[];
  static const char kGetAvailableLogTypes[];
  static const char kGetLog[];
  static const char kGetSessionLogs[];

  // Custom Chrome commands:
  static const char kQuitAll[];
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_NAMES_H_
