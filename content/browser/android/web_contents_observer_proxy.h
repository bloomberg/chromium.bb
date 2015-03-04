// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_
#define CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_

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
class WebContentsObserverProxy : public WebContentsObserver {
 public:
  WebContentsObserverProxy(JNIEnv* env, jobject obj, WebContents* web_contents);
  ~WebContentsObserverProxy() override;

  void Destroy(JNIEnv* env, jobject obj);

 private:
  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus termination_status) override;
  void DidStartLoading(RenderViewHost* render_view_host) override;
  void DidStopLoading(RenderViewHost* render_view_host) override;
  void DidFailProvisionalLoad(RenderFrameHost* render_frame_host,
                              const GURL& validated_url,
                              int error_code,
                              const base::string16& error_description) override;
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void DidNavigateMainFrame(const LoadCommittedDetails& details,
                            const FrameNavigateParams& params) override;
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override;
  void DocumentAvailableInMainFrame() override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStartProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                       const GURL& validated_url,
                                       bool is_error_page,
                                       bool is_iframe_srcdoc) override;
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DocumentLoadedInFrame(RenderFrameHost* render_frame_host) override;
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void DidChangeThemeColor(SkColor color) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) override;

  void DidFailLoadInternal(bool is_provisional_load,
                           bool is_main_frame,
                           int error_code,
                           const base::string16& description,
                           const GURL& url);

  JavaObjectWeakGlobalRef weak_java_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverProxy);
};

bool RegisterWebContentsObserverProxy(JNIEnv* env);
}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_
