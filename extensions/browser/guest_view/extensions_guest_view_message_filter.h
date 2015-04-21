// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Size;
}

namespace extensions {

// This class filters out incoming extensions GuestView-specific IPC messages
// from thw renderer process. It is created on the UI thread. Messages may be
// handled on the IO thread or the UI thread.
class ExtensionsGuestViewMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionsGuestViewMessageFilter(int render_process_id,
                                   content::BrowserContext* context);

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ExtensionsGuestViewMessageFilter>;

  ~ExtensionsGuestViewMessageFilter() override;

  // content::BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the UI thread.
  void OnCanExecuteContentScript(int render_view_id,
                                 int script_id,
                                 bool* allowed);

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
  base::WeakPtrFactory<ExtensionsGuestViewMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsGuestViewMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
