// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include "base/process_util.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_channel.h"
#include "webkit/glue/window_open_disposition.h"

class RenderViewHost;
class TabContents;

namespace content {

class WebContents;
struct FrameNavigateParams;
struct Referrer;

// An observer API implemented by classes which are interested in various page
// load events from TabContents.  They also get a chance to filter IPC messages.
class CONTENT_EXPORT WebContentsObserver : public IPC::Channel::Listener,
                                           public IPC::Message::Sender {
 public:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}
  virtual void RenderViewReady() {}
  virtual void RenderViewGone(base::TerminationStatus status) {}
  virtual void NavigateToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) {}
  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}
  virtual void DidNavigateAnyFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}
  // |render_view_host| is the RenderViewHost for which the provisional load is
  // happening.
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      RenderViewHost* render_view_host) {}
  virtual void ProvisionalChangeToMainFrameUrl(const GURL& url,
                                               const GURL& opener_url) {}
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type) {}
  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code,
                                      const string16& error_description) {}
  virtual void DocumentAvailableInMainFrame() {}
  virtual void DocumentLoadedInFrame(int64 frame_id) {}
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame) {}
  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const string16& error_description) {}
  virtual void DidGetUserGesture() {}
  virtual void DidGetIgnoredUIEvent() {}
  virtual void DidBecomeSelected() {}

  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}
  virtual void StopNavigation() {}

  virtual void DidOpenURL(const GURL& url,
                          const Referrer& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition transition) {}

  virtual void DidOpenRequestedURL(WebContents* new_contents,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   PageTransition transition,
                                   int64 source_frame_id) {}

  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. At the time this is invoked |tab_contents()| returns NULL.
  // It is safe to delete 'this' from here.
  virtual void WebContentsDestroyed(WebContents* tab) {}

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;
  int routing_id() const;

 protected:
  // Use this constructor when the object is tied to a single WebContents for
  // its entire lifetime.
  explicit WebContentsObserver(WebContents* web_contents);

  // Use this constructor when the object wants to observe a TabContents for
  // part of its lifetime.  It can then call Observe() to start and stop
  // observing.
  WebContentsObserver();

  virtual ~WebContentsObserver();

  // Start observing a different WebContents; used with the default constructor.
  void Observe(WebContents* web_contents);

  WebContents* web_contents() const;
  // TODO(jam): remove me
  TabContents* tab_contents() const { return tab_contents_; }

 private:
  friend class ::TabContents;

  // Invoked from TabContents. Invokes TabContentsDestroyed and NULL out
  // |tab_contents_|.
  void TabContentsDestroyed();

  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
