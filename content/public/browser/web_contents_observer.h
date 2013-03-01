// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include "base/process.h"
#include "base/process_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/base/window_open_disposition.h"

namespace content {

class RenderViewHost;
class WebContents;
class WebContentsImpl;
struct FaviconURL;
struct FrameNavigateParams;
struct LoadCommittedDetails;
struct Referrer;

// An observer API implemented by classes which are interested in various page
// load events from WebContents.  They also get a chance to filter IPC messages.
//
// Since a WebContents can be a delegate to almost arbitrarly many
// RenderViewHosts, it is important to check in those WebContentsObserver
// methods which take a RenderViewHost that the event came from the
// RenderViewHost the observer cares about.
//
// Usually, observers should only care about the current RenderViewHost as
// returned by GetRenderViewHost().
//
// TODO(creis, jochen): Hide the fact that there are several RenderViewHosts
// from the WebContentsObserver API. http://crbug.com/173325
class CONTENT_EXPORT WebContentsObserver : public IPC::Listener,
                                           public IPC::Sender {
 public:
  // Only one of the two methods below will be called when a RVH is created for
  // a WebContents, depending on whether it's for an interstitial or not.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}
  virtual void RenderViewForInterstitialPageCreated(
      RenderViewHost* render_view_host) {}

  // This method is invoked when the RenderView of the current RenderViewHost
  // is ready, e.g. because we recreated it after a crash.
  virtual void RenderViewReady() {}

  // This method is invoked when a RenderViewHost of the WebContents is
  // deleted. Note that this does not always happen when the WebContents starts
  // to use a different RenderViewHost, as the old RenderViewHost might get
  // just swapped out.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // This method is invoked when the current RenderView crashes. The WebContents
  // continues to use the RenderViewHost, e.g. when the user reloads the current
  // page.
  // When the RenderViewHost is deleted, the RenderViewDeleted method will be
  // invoked.
  virtual void RenderViewGone(base::TerminationStatus status) {}

  // This method is invoked after the WebContents decided which RenderViewHost
  // to use for the next navigation, but before the navigation starts.
  virtual void AboutToNavigateRenderView(
      RenderViewHost* render_view_host) {}

  // This method is invoked right after the navigation was initiated.
  virtual void NavigateToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) {}

  // |render_view_host| is the RenderViewHost for which the provisional load is
  // happening. |frame_id| is a positive, non-zero integer identifying the
  // navigating frame in the given |render_view_host|. |parent_frame_id| is the
  // frame identifier of the frame containing the navigating frame, or -1 if the
  // frame is not contained in another frame.
  //
  // Since the URL validation will strip error URLs, or srcdoc URLs, the boolean
  // flags |is_error_page| and |is_iframe_srcdoc| will indicate that the not
  // validated URL was either an error page or an iframe srcdoc.
  //
  // Note that during a cross-process navigation, several provisional loads
  // can be on-going in parallel.
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      RenderViewHost* render_view_host) {}

  // This method is invoked right after the DidStartProvisionalLoadForFrame if
  // the provisional load affects the main frame, or if the provisional load
  // was redirected. The latter use case is DEPRECATED. You should listen to
  // the ResourceDispatcherHost's RESOURCE_RECEIVED_REDIRECT notification
  // instead.
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      RenderViewHost* render_view_host) {}

  // This method is invoked when the provisional load was successfully
  // commited. The |render_view_host| is now the current RenderViewHost of the
  // WebContents.
  //
  // If the navigation only changed the reference fragment, or was triggered
  // using the history API (e.g. window.history.replaceState), we will receive
  // this signal without a prior DidStartProvisionalLoadForFrame signal.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) {}

  // This method is invoked when the provisional load failed.
  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code,
                                      const string16& error_description,
                                      RenderViewHost* render_view_host) {}

  // If the provisional load corresponded to the main frame, this method is
  // invoked in addition to DidCommitProvisionalLoadForFrame.
  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}

  // And regardless of what frame navigated, this method is invoked after
  // DidCommitProvisionalLoadForFrame was invoked.
  virtual void DidNavigateAnyFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}

  // This method is invoked once the window.document object was created.
  virtual void DocumentAvailableInMainFrame() {}

  // This method is invoked when the document in the given frame finished
  // loading. At this point, scripts marked as defer were executed, and
  // content scripts marked "document_end" get injected into the frame.
  virtual void DocumentLoadedInFrame(int64 frame_id,
                                     RenderViewHost* render_view_host) {}

  // This method is invoked when the navigation is done, i.e. the spinner of
  // the tab will stop spinning, and the onload event was dispatched.
  //
  // If the WebContents is displaying replacement content, e.g. network error
  // pages, DidFinishLoad is invoked for frames that were not sending
  // navigational events before. It is safe to ignore these events.
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             RenderViewHost* render_view_host) {}

  // This method is like DidFinishLoad, but when the load failed or was
  // cancelled, e.g. window.stop() is invoked.
  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const string16& error_description,
                           RenderViewHost* render_view_host) {}

  // This method is invoked when a new WebContents was created in response to
  // an action in the observed WebContents, e.g. a link with target=_blank was
  // clicked. The |source_frame_id| indicates in which frame the action took
  // place.
  virtual void DidOpenRequestedURL(WebContents* new_contents,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   PageTransition transition,
                                   int64 source_frame_id) {}

  virtual void FrameDetached(RenderViewHost* render_view_host,
                             int64 frame_id) {}

  // These two methods correspond to the points in time when the spinner of the
  // tab starts and stops spinning.
  virtual void DidStartLoading(RenderViewHost* render_view_host) {}
  virtual void DidStopLoading(RenderViewHost* render_view_host) {}

  // This method is invoked when the navigation from the browser process. If
  // there are ongoing navigations, the respective failure methods will also be
  // invoked.
  virtual void StopNavigation() {}

  // This indicates that the next navigation was triggered by a user gesture.
  virtual void DidGetUserGesture() {}

  // This method is invoked when a RenderViewHost of this WebContents was
  // configured to ignore UI events, and an UI event took place.
  virtual void DidGetIgnoredUIEvent() {}

  // This method is invoked every time the WebContents becomes visible.
  virtual void WasShown() {}

  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) {}

  // Notification that a plugin has crashed.
  // |plugin_pid| is the process ID identifying the plugin process. Note that
  // this ID is supplied by the renderer, so should not be trusted. Besides, the
  // corresponding process has probably died at this point. The ID may even have
  // been reused by a new process.
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) {}

  // Notication that the given plugin has hung or become unhung. This
  // notification is only for Pepper plugins.
  //
  // The plugin_child_id is the unique child process ID from the plugin. Note
  // that this ID is supplied by the renderer, so should be validated before
  // it's used for anything in case there's an exploited renderer.
  virtual void PluginHungStatusChanged(int plugin_child_id,
                                       const base::FilePath& plugin_path,
                                       bool is_hung) {}

  // Invoked when WebContents::Clone() was used to clone a WebContents.
  virtual void DidCloneToNewWebContents(WebContents* old_web_contents,
                                        WebContents* new_web_contents) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. At the time this is invoked |web_contents()| returns NULL.
  // It is safe to delete 'this' from here.
  virtual void WebContentsDestroyed(WebContents* web_contents) {}

  // Called when the user agent override for a WebContents has been changed.
  virtual void UserAgentOverrideSet(const std::string& user_agent) {}

  // Invoked when new FaviconURL candidates are received from the renderer.
  virtual void DidUpdateFaviconURL(int32 page_id,
                                   const std::vector<FaviconURL>& candidates) {}

  // Invoked when a pepper plugin creates and shows or destroys a fullscreen
  // render widget.
  virtual void DidShowFullscreenWidget(int routing_id) {}
  virtual void DidDestroyFullscreenWidget(int routing_id) {}

  // Invoked when visible SSL state (as defined by SSLStatus) changes.
  virtual void DidChangeVisibleSSLState() {}

  // Invoked when an interstitial page is attached or detached.
  virtual void DidAttachInterstitialPage() {}
  virtual void DidDetachInterstitialPage() {}

  // Invoked before a form repost warning is shown.
  virtual void BeforeFormRepostWarningShow() {}

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation.
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
  friend class WebContentsImpl;

  // Invoked from WebContentsImpl. Invokes WebContentsDestroyed and NULL out
  // |web_contents_|.
  void WebContentsImplDestroyed();

  WebContentsImpl* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
