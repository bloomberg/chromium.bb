// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_USER_DATA_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_USER_DATA_H_

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

// A base class for classes attached to, and scoped to, the lifetime of a
// WebContents. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public WebContentsUserData<FooTabHelper> {
//  public:
//   explicit FooTabHelper(content::WebContents* contents);
//   virtual ~FooTabHelper();
//   static int kUserDataKey;
//  // ... more stuff here ...
// }
// --- in foo_tab_helper.cc ---
// int FooTabHelper::kUserDataKey;
//
template <typename T>
class WebContentsUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebContents.
  // If an instance is already attached, does nothing.
  static void CreateForWebContents(content::WebContents* contents) {
    if (!FromWebContents(contents))
      contents->SetUserData(&T::kUserDataKey, new T(contents));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebContents (via CreateForWebContents above) and returns it. If no instance
  // of the type was attached, returns NULL.
  static T* FromWebContents(content::WebContents* contents) {
    return static_cast<T*>(contents->GetUserData(&T::kUserDataKey));
  }
  static const T* FromWebContents(const content::WebContents* contents) {
    return static_cast<const T*>(contents->GetUserData(&T::kUserDataKey));
  }
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_USER_DATA_H_
