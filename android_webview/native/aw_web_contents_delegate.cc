// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_web_contents_delegate.h"

#include "android_webview/browser/find_helper.h"
#include "android_webview/native/aw_contents.h"
#include "android_webview/native/aw_javascript_dialog_creator.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwWebContentsDelegate_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace android_webview {

static base::LazyInstance<AwJavaScriptDialogCreator>::Leaky
    g_javascript_dialog_creator = LAZY_INSTANCE_INITIALIZER;

AwWebContentsDelegate::AwWebContentsDelegate(
    JNIEnv* env,
    jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

AwWebContentsDelegate::~AwWebContentsDelegate() {
}

content::JavaScriptDialogCreator*
AwWebContentsDelegate::GetJavaScriptDialogCreator() {
  return g_javascript_dialog_creator.Pointer();
}

void AwWebContentsDelegate::FindReply(WebContents* web_contents,
                                      int request_id,
                                      int number_of_matches,
                                      const gfx::Rect& selection_rect,
                                      int active_match_ordinal,
                                      bool final_update) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents)
    return;

  aw_contents->GetFindHelper()->HandleFindReply(request_id,
                                                number_of_matches,
                                                active_match_ordinal,
                                                final_update);
}

bool AwWebContentsDelegate::CanDownload(content::RenderViewHost* source,
                                        int request_id,
                                        const std::string& request_method) {
  // Android webview intercepts download in its resource dispatcher host
  // delegate, so should not reach here.
  NOTREACHED();
  return false;
}

void AwWebContentsDelegate::OnStartDownload(WebContents* source,
                                            content::DownloadItem* download) {
  NOTREACHED();  // Downloads are cancelled in ResourceDispatcherHostDelegate.
}

void AwWebContentsDelegate::AddNewContents(content::WebContents* source,
                                           content::WebContents* new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture,
                                           bool* was_blocked) {
  JNIEnv* env = AttachCurrentThread();

  bool is_dialog = disposition == NEW_POPUP;
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  bool create_popup = false;

  if (java_delegate.obj()) {
    create_popup = Java_AwWebContentsDelegate_addNewContents(env,
        java_delegate.obj(), is_dialog, user_gesture);
  }

  if (create_popup) {
    // The embedder would like to display the popup and we will receive
    // a callback from them later with an AwContents to use to display
    // it. The source AwContents takes ownership of the new WebContents
    // until then, and when the callback is made we will swap the WebContents
    // out into the new AwContents.
    AwContents::FromWebContents(source)->SetPendingWebContentsForPopup(
        make_scoped_ptr(new_contents));
    // Hide the WebContents for the pop up now, we will show it again
    // when the user calls us back with an AwContents to use to show it.
    new_contents->WasHidden();
  } else {
    // The embedder has forgone their chance to display this popup
    // window, so we're done with the WebContents now. We use
    // DeleteSoon as WebContentsImpl may call methods on |new_contents|
    // after this method returns.
    MessageLoop::current()->DeleteSoon(FROM_HERE, new_contents);
  }

  if (was_blocked) {
    *was_blocked = !create_popup;
  }
}

void AwWebContentsDelegate::CloseContents(content::WebContents* source) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_closeContents(env, java_delegate.obj());
  }
}

void AwWebContentsDelegate::ActivateContents(content::WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_activateContents(env, java_delegate.obj());
  }
}

bool RegisterAwWebContentsDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
