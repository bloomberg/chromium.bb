// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/edit_bookmark_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/EditBookmarkHelper_jni.h"

using bookmarks::BookmarkNode;

void SetPartnerBookmarkTitle(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& jprofile,
                             jlong bookmark_id,
                             const JavaParamRef<jstring>& new_title) {
  PartnerBookmarksShim* partner_bookmarks_shim =
      PartnerBookmarksShim::BuildForBrowserContext(
          chrome::GetBrowserContextRedirectedInIncognito(
              ProfileAndroid::FromProfileAndroid(jprofile)));
  if (!partner_bookmarks_shim)
    return;

  const BookmarkNode* node = partner_bookmarks_shim->GetNodeByID(bookmark_id);
  if (!node)
    return;

  partner_bookmarks_shim->RenameBookmark(
      node,
      base::android::ConvertJavaStringToUTF16(env, new_title));
}

// static
bool RegisterEditBookmarkHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
