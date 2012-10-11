// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"

TabContents* TabAndroid::GetOrCreateTabContents(
    content::WebContents* web_contents) {
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents) {
    tab_contents = TabContents::Factory::CreateTabContents(web_contents);
    InitTabHelpers(web_contents);
  }
  return tab_contents;
}

void TabAndroid::InitTabHelpers(content::WebContents* web_contents) {
  WindowAndroidHelper::CreateForWebContents(web_contents);
}

TabContents* TabAndroid::InitTabContentsFromView(JNIEnv* env,
                                                 jobject content_view) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env, content_view);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());
  return GetOrCreateTabContents(content_view_core->GetWebContents());
}

TabAndroid::TabAndroid() : tab_id_(-1) {
}

TabAndroid::~TabAndroid() {
}
