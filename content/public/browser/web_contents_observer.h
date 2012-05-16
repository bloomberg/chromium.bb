// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include "base/process_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_channel.h"
#include "webkit/glue/window_open_disposition.h"

class WebContentsImpl;

namespace content {

class RenderViewHost;
class WebContents;
struct FrameNavigateParams;
struct LoadCommittedDetails;
struct Referrer;

// An observer API implemented by classes which are interested in various page
// load events from WebContents.  They also get a chance to filter IPC messages.
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
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      const GURL& opener_url,
      RenderViewHost* render_view_host) {}
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) {}
  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code,
                                      const string16& error_description,
                                      RenderViewHost* render_view_host) {}
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

  // Notification that a plugin has crashed.
  virtual void PluginCrashed(const FilePath& plugin_path) {}

  // Notication that the given plugin has hung or become unhung. This
  // notification is only for Pepper plugins.
  //
  // The plugin_child_id is the unique child process ID from the plugin. Note
  // that this ID is supplied by the renderer, so should be validated before
  // it's used for anything in case there's an exploited renderer.
  virtual void PluginHungStatusChanged(int plugin_child_id,
                                       const FilePath& plugin_path,
                                       bool is_hung) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. At the time this is invoked |web_contents()| returns NULL.
  // It is safe to delete 'this' from here.
  virtual void WebContentsDestroyed(WebContents* web_contents) {}

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;
  int routing_id() const;

 protected:
  // Use this constructor when the object is tied to a single WebContents for
  // its entire lifetime.
  explicit WebContentsObserver(WebContents* web_contents);

  // Use this constructor when the object wants to observe a WebContents for
  // part of its lifetime.  It can then call Observe() to start and stop
  // observing.
  WebContentsObserver();

  virtual ~WebContentsObserver();

  // Start observing a different WebContents; used with the default constructor.
  void Observe(WebContents* web_contents);

  WebContents* web_contents() const;

 private:
  friend class ::WebContentsImpl;

  // Invoked from WebContentsImpl. Invokes WebContentsDestroyed and NULL out
  // |web_contents_|.
  void WebContentsImplDestroyed();

  WebContentsImpl* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
