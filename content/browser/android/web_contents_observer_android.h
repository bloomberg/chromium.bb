// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "googleurl/src/gurl.h"

namespace content {

class RenderViewHost;
class WebContents;

// Extends WebContentsObserver for providing a public Java API for some of the
// the calls it receives.
class WebContentsObserverAndroid : public WebContentsObserver {
 public:
  WebContentsObserverAndroid(JNIEnv* env,
                             jobject obj,
                             WebContents* web_contents);
  virtual ~WebContentsObserverAndroid();

  void Destroy(JNIEnv* env, jobject obj);

 private:
  virtual void DidStartLoading(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const string16& error_description,
                           RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidNavigateMainFrame(const LoadCommittedDetails& details,
                                    const FrameNavigateParams& params) OVERRIDE;
  virtual void DidNavigateAnyFrame(const LoadCommittedDetails& details,
                                   const FrameNavigateParams& params) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;
  virtual void DidChangeVisibleSSLState() OVERRIDE;

  void DidFailLoadInternal(bool is_provisional_load,
                           bool is_main_frame,
                           int error_code,
                           const string16& description,
                           const GURL& url);

  JavaObjectWeakGlobalRef weak_java_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverAndroid);
};

bool RegisterWebContentsObserverAndroid(JNIEnv* env);
}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_
