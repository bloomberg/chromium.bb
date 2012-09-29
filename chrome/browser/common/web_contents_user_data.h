// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMON_WEB_CONTENTS_USER_DATA_H_
#define CHROME_BROWSER_COMMON_WEB_CONTENTS_USER_DATA_H_

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

// A base class for classes attached to, and scoped to, the lifetime of a
// WebContents. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public WebContentsUserData<FooTabHelper> {
//  public:
//   virtual ~FooTabHelper();
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(content::WebContents* contents);
//   friend class WebContentsUserData<FooTabHelper>;
//   // ... more private stuff here ...
// }
// --- in foo_tab_helper.cc ---
// DEFINE_WEB_CONTENTS_USER_DATA_KEY(FooTabHelper)
//
template <typename T>
class WebContentsUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebContents.
  // If an instance is already attached, does nothing.
  static void CreateForWebContents(content::WebContents* contents) {
    DCHECK(contents);
    if (!FromWebContents(contents))
      contents->SetUserData(&kLocatorKey, new T(contents));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebContents (via CreateForWebContents above) and returns it. If no instance
  // of the type was attached, returns NULL.
  static T* FromWebContents(content::WebContents* contents) {
    DCHECK(contents);
    return static_cast<T*>(contents->GetUserData(&kLocatorKey));
  }
  static const T* FromWebContents(const content::WebContents* contents) {
    DCHECK(contents);
    return static_cast<const T*>(contents->GetUserData(&kLocatorKey));
  }

  // The user data key.
  static int kLocatorKey;
};

// The macro to define the locator key. This key must be defined in the .cc file
// of the tab helper otherwise different instances for different template types
// will be collapsed by the Visual Studio linker.
//
// The "= 0" is surprising, but is required to effect a definition rather than
// a declaration. Without it, this would be merely a declaration of a template
// specialization. (C++98: 14.7.3.15; C++11: 14.7.3.13)
//
#define DEFINE_WEB_CONTENTS_USER_DATA_KEY(TYPE) \
template<>                                      \
int WebContentsUserData<TYPE>::kLocatorKey = 0;

#endif  // CHROME_BROWSER_COMMON_WEB_CONTENTS_USER_DATA_H_
