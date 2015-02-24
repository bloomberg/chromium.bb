// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MESSAGE_FILTER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Size;
}

namespace extensions {

// This class filters out incoming GuestView-specific IPC messages from the
// renderer process. It is created on the UI thread. Messages may be handled on
// the IO thread or the UI thread.
class GuestViewMessageFilter : public content::BrowserMessageFilter {
 public:
  GuestViewMessageFilter(int render_process_id,
                         content::BrowserContext* context);

  int render_process_id() const { return render_process_id_; }

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<GuestViewMessageFilter>;

  ~GuestViewMessageFilter() override;

  // content::BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the UI thread.
  void OnAttachGuest(int element_instance_id,
                     int guest_instance_id,
                     const base::DictionaryValue& attach_params);
  void OnCreateMimeHandlerViewGuest(int render_frame_id,
                                    const std::string& view_id,
                                    int element_instance_id,
                                    const gfx::Size& element_size);
  void OnResizeGuest(int render_frame_id,
                     int element_instance_id,
                     const gfx::Size& new_size);

  // Runs on UI thread.
  void MimeHandlerViewGuestCreatedCallback(int element_instance_id,
                                           int embedder_render_process_id,
                                           int embedder_render_frame_id,
                                           const gfx::Size& element_size,
                                           content::WebContents* web_contents);

  const int render_process_id_;

  // Should only be accessed on the UI thread.
  content::BrowserContext* const browser_context_;

  // Weak pointers produced by this factory are bound to the IO thread.
  base::WeakPtrFactory<GuestViewMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MESSAGE_FILTER_H_
