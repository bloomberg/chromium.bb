// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_model.h"

class PartnerBookmarksShim;
class Profile;

// Generates a partner bookmark hierarchy and handles submitting the results to
// the global PartnerBookmarksShim.
class PartnerBookmarksReader {
 public:
  PartnerBookmarksReader(PartnerBookmarksShim* partner_bookmarks_shim,
                         Profile* profile);
  ~PartnerBookmarksReader();

  // JNI methods
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Reset(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jlong AddPartnerBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle,
      jboolean is_folder,
      jlong parent_id,
      const base::android::JavaParamRef<jbyteArray>& favicon,
      const base::android::JavaParamRef<jbyteArray>& touchicon);
  void PartnerBookmarksCreationComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  PartnerBookmarksShim* partner_bookmarks_shim_;
  Profile* profile_;

  // JNI
  std::unique_ptr<bookmarks::BookmarkNode> wip_partner_bookmarks_root_;
  int64_t wip_next_available_id_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksReader);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
