// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

#define WEB_CONTENTS_USER_DATA_KEY_DECL() static constexpr int kUserDataKey = 0;

#define WEB_CONTENTS_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;

namespace content {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebContents. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public content::WebContentsUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(content::WebContents* contents);
//   friend class content::WebContentsUserData<FooTabHelper>;
//   WEB_CONTENTS_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// WEB_CONTENTS_USER_DATA_KEY_IMPL(FooTabHelper)
template <typename T>
class WebContentsUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebContents.
  // If an instance is already attached, does nothing.
  static void CreateForWebContents(WebContents* contents) {
    DCHECK(contents);
    if (!FromWebContents(contents))
      contents->SetUserData(UserDataKey(), base::WrapUnique(new T(contents)));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebContents (via CreateForWebContents above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromWebContents(WebContents* contents) {
    DCHECK(contents);
    return static_cast<T*>(contents->GetUserData(UserDataKey()));
  }
  static const T* FromWebContents(const WebContents* contents) {
    DCHECK(contents);
    return static_cast<const T*>(contents->GetUserData(UserDataKey()));
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
