// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_

#include <vector>

#include "chrome/browser/sessions/session_id.h"

class Profile;
class TabModel;

namespace chrome {
struct NavigateParams;
}

namespace content {
class WebContents;
}

// Stores a list of all TabModel objects.
class TabModelList {
 public:
  typedef std::vector<TabModel*> TabModelVector;
  typedef TabModelVector::iterator iterator;
  typedef TabModelVector::const_iterator const_iterator;

  static void HandlePopupNavigation(chrome::NavigateParams* params);
  static void AddTabModel(TabModel* tab_model);
  static void RemoveTabModel(TabModel* tab_model);

  static TabModel* GetTabModelForWebContents(
      content::WebContents* web_contents);
  static TabModel* FindTabModelWithId(SessionID::id_type desired_id);
  static bool IsOffTheRecordSessionActive();

  static const_iterator begin();
  static const_iterator end();
  static bool empty();
  static size_t size();

  static TabModel* get(size_t index);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TabModelList);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
