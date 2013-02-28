// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_
#define ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_

#include "content/public/browser/web_contents_observer.h"

#include "android_webview/common/aw_hit_test_data.h"
#include "base/callback_forward.h"
#include "base/threading/non_thread_safe.h"

class GURL;

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
}  // namespace content

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class AwRenderViewHostExt : public content::WebContentsObserver,
                            public base::NonThreadSafe {
 public:
  // To send receive messages to a RenderView we take the WebContents instance,
  // as it internally handles RenderViewHost instances changing underneath us.
  AwRenderViewHostExt(content::WebContents* contents);
  virtual ~AwRenderViewHostExt();

  // |result| will be invoked with the outcome of the request.
  typedef base::Callback<void(bool)> DocumentHasImagesResult;
  void DocumentHasImages(DocumentHasImagesResult result);

  // Clear all WebCore memory cache (not only for this view).
  void ClearCache();

  // Do a hit test at the view port coordinates and asynchronously update
  // |last_hit_test_data_|. |view_x| and |view_y| are in density independent
  // pixels used by WebKit::WebView.
  void RequestNewHitTestDataAt(int view_x, int view_y);

  // Optimization to avoid unnecessary Java object creation on hit test.
  bool HasNewHitTestData() const;
  void MarkHitTestDataRead();

  // Return |last_hit_test_data_|. Note that this is unavoidably racy;
  // the corresponding public WebView API is as well.
  const AwHitTestData& GetLastHitTestData() const;

  // Set whether fixed layout mode is enabled. Must be updated together
  // with WebSettings.viewport_enabled.
  // TODO(mnaganov): Leave only one setting. See the comments on
  //   https://bugs.webkit.org/show_bug.cgi?id=109946
  void SetEnableFixedLayoutMode(bool enable);

  // Sets the zoom level for text only. Used in layout modes other than
  // Text Autosizing.
  void SetTextZoomLevel(double level);

  void ResetScrollAndScaleState();

  // Sets the initial page scale. This overrides initial scale set by
  // the meta viewport tag.
  void SetInitialPageScale(double page_scale_factor);

 private:
  // content::WebContentsObserver implementation.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDocumentHasImagesResponse(int msg_id, bool has_images);
  void OnUpdateHitTestData(const AwHitTestData& hit_test_data);
  void OnPictureUpdated();

  bool IsRenderViewReady() const;

  std::map<int, DocumentHasImagesResult> pending_document_has_images_requests_;

  // Master copy of hit test data on the browser side. This is updated
  // as a result of DoHitTest called explicitly or when the FocusedNodeChanged
  // is called in AwRenderViewExt.
  AwHitTestData last_hit_test_data_;

  bool has_new_hit_test_data_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderViewHostExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_
