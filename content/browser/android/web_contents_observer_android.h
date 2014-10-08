// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/process/kill.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "url/gurl.h"

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
  virtual void RenderProcessGone(
      base::TerminationStatus termination_status) override;
  virtual void DidStartLoading(RenderViewHost* render_view_host) override;
  virtual void DidStopLoading(RenderViewHost* render_view_host) override;
  virtual void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) override;
  virtual void DidFailLoad(RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           int error_code,
                           const base::string16& error_description) override;
  virtual void DidNavigateMainFrame(const LoadCommittedDetails& details,
                                    const FrameNavigateParams& params) override;
  virtual void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                                   const LoadCommittedDetails& details,
                                   const FrameNavigateParams& params) override;
  virtual void DidFirstVisuallyNonEmptyPaint() override;
  virtual void DidStartProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  virtual void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  virtual void DidFinishLoad(RenderFrameHost* render_frame_host,
                             const GURL& validated_url) override;
  virtual void DocumentLoadedInFrame(
      RenderFrameHost* render_frame_host) override;
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  virtual void WebContentsDestroyed() override;
  virtual void DidAttachInterstitialPage() override;
  virtual void DidDetachInterstitialPage() override;
  virtual void DidChangeThemeColor(SkColor color) override;

  void DidFailLoadInternal(bool is_provisional_load,
                           bool is_main_frame,
                           int error_code,
                           const base::string16& description,
                           const GURL& url);

  JavaObjectWeakGlobalRef weak_java_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverAndroid);
};

bool RegisterWebContentsObserverAndroid(JNIEnv* env);
}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_ANDROID_H_
