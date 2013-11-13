// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_

#include "base/strings/string16.h"

namespace app_list {

class StartPageObserver {
 public:
  // Invoked when a search query happens from the start page.
  virtual void OnSearch(const base::string16& query) = 0;

  // Invoked when the online speech recognition state is changed. |recognizing|
  // is the new state and true when the speech recognition is running currently.
  virtual void OnSpeechRecognitionStateChanged(bool recognizing) = 0;

 protected:
  virtual ~StartPageObserver() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_OBSERVER_H_
