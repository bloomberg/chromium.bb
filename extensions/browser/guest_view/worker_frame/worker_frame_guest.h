// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_GUEST_H_

#include "extensions/browser/guest_view/guest_view.h"

namespace extensions {
class Extension;
class ExtensionHost;

// An WorkerFrameGuest provides the browser-side implementation of the
// prototype <wtframe> API.
class WorkerFrameGuest : public GuestView<WorkerFrameGuest> {
 public:
  static const char Type[];

  static GuestViewBase* Create(content::BrowserContext* browser_context,
                               content::WebContents* owner_web_contents,
                               int guest_instance_id);

  // content::WebContentsDelegate implementation.
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // GuestViewBase implementation.
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidAttachToEmbedder() override;

 private:
  WorkerFrameGuest(content::BrowserContext* browser_context,
                   content::WebContents* owner_web_contents,
                   int guest_instance_id);

  ~WorkerFrameGuest() override;

  GURL url_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<WorkerFrameGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WorkerFrameGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_GUEST_H_
