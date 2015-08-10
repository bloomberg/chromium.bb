// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_

#include "base/memory/weak_ptr.h"
#include "components/guest_view/browser/guest_view.h"

namespace content {
class WebContents;
struct ContextMenuParams;
struct StreamInfo;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuestDelegate;

// A container for a StreamHandle and any other information necessary for a
// MimeHandler to handle a resource stream.
class StreamContainer {
 public:
  StreamContainer(scoped_ptr<content::StreamInfo> stream,
                  int tab_id,
                  bool embedded,
                  const GURL& handler_url,
                  const std::string& extension_id);
  ~StreamContainer();

  // Aborts the stream.
  void Abort(const base::Closure& callback);

  base::WeakPtr<StreamContainer> GetWeakPtr();

  const content::StreamInfo* stream_info() const { return stream_.get(); }
  bool embedded() const { return embedded_; }
  int tab_id() const { return tab_id_; }
  GURL handler_url() const { return handler_url_; }
  std::string extension_id() const { return extension_id_; }

 private:
  const scoped_ptr<content::StreamInfo> stream_;
  const bool embedded_;
  const int tab_id_;
  const GURL handler_url_;
  const std::string extension_id_;

  base::WeakPtrFactory<StreamContainer> weak_factory_;
};

class MimeHandlerViewGuest :
    public guest_view::GuestView<MimeHandlerViewGuest> {
 public:
  static guest_view::GuestViewBase* Create(
      content::WebContents* owner_web_contents);

  static const char Type[];

  // GuestViewBase implementation.
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize(const base::DictionaryValue& create_params) override;
  bool ShouldHandleFindRequestsForEmbedder() const override;
  bool ZoomPropagatesFromEmbedderToGuest() const override;

  // WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  bool SaveFrame(const GURL& url, const content::Referrer& referrer) override;

  // content::WebContentsObserver implementation.
  void DocumentOnLoadCompletedInMainFrame() override;

  std::string view_id() const { return view_id_; }
  base::WeakPtr<StreamContainer> GetStream() const;

 protected:
  explicit MimeHandlerViewGuest(content::WebContents* owner_web_contents);
  ~MimeHandlerViewGuest() override;

 private:
  scoped_ptr<MimeHandlerViewGuestDelegate> delegate_;
  scoped_ptr<StreamContainer> stream_;
  std::string view_id_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
