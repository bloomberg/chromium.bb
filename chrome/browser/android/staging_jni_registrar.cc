// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/staging_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/android/bookmark/edit_bookmark_helper.h"
#include "chrome/browser/android/compositor/compositor_view.h"
#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/reader_mode_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/tab_list_scene_layer.h"
#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"
#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"
#include "chrome/browser/android/contextualsearch/contextual_search_tab_helper.h"
#include "chrome/browser/android/document/document_web_contents_delegate.h"
#include "chrome/browser/android/history_report/history_report_jni_bridge.h"
#include "chrome/browser/android/policy/policy_manager.h"
#include "chrome/browser/android/rlz/revenue_stats.h"
#include "chrome/browser/android/tab/background_content_view_helper.h"
#include "chrome/browser/android/tab/thumbnail_tab_helper_android.h"

namespace chrome {
namespace android {
static base::android::RegistrationMethod kRegistrationMethods[] = {
    {"BackgroundContentViewHelper", BackgroundContentViewHelper::Register},
    {"CompositorView", RegisterCompositorView},
    {"ContextualSearchManager", RegisterContextualSearchManager},
    {"ContextualSearchSceneLayer", RegisterContextualSearchSceneLayer},
    {"ContextualSearchTabHelper", RegisterContextualSearchTabHelper},
    {"DocumentWebContentsDelegate", DocumentWebContentsDelegate::Register},
    {"EditBookmarkHelper", RegisterEditBookmarkHelper},
    {"HistoryReportJniBridge", history_report::RegisterHistoryReportJniBridge},
    {"PolicyManager", RegisterPolicyManager},
    {"ReaderModeSceneLayer", RegisterReaderModeSceneLayer},
    {"RevenueStats", RegisterRevenueStats},
    {"TabListSceneLayer", RegisterTabListSceneLayer},
    {"TabStripSceneLayer", RegisterTabStripSceneLayer},
    {"ThumbnailTabHelperAndroid", RegisterThumbnailTabHelperAndroid},
};

bool RegisterStagingJNI(JNIEnv* env) {
  return base::android::RegisterNativeMethods(env, kRegistrationMethods,
                                              arraysize(kRegistrationMethods));
  return true;
}

}  // namespace android
}  // namespace chrome
