// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/scoped_ptr.h"
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
  void Destroy(JNIEnv* env, jobject obj);
  void Reset(JNIEnv* env, jobject obj);
  jlong AddPartnerBookmark(JNIEnv* env,
                           jobject obj,
                           jstring jurl,
                           jstring jtitle,
                           jboolean is_folder,
                           jlong parent_id,
                           jbyteArray favicon,
                           jbyteArray touchicon);
  void PartnerBookmarksCreationComplete(JNIEnv* env, jobject obj);

  // JNI registration
  static bool RegisterPartnerBookmarksReader(JNIEnv* env);

 private:
  PartnerBookmarksShim* partner_bookmarks_shim_;
  Profile* profile_;

  // JNI
  scoped_ptr<bookmarks::BookmarkNode> wip_partner_bookmarks_root_;
  int64 wip_next_available_id_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksReader);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_READER_H_
