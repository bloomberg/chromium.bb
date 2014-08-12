// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_H_

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/guest_view/guest_view_base.h"

namespace extensions {

// A GuestView is the templated base class for out-of-process frames in the
// chrome layer. GuestView is templated on its derived type to allow for type-
// safe access. See GuestViewBase for more information.
template <typename T>
class GuestView : public GuestViewBase {
 public:
  static void Register() {
    GuestViewBase::RegisterGuestViewType(T::Type, base::Bind(&T::Create));
  }

  static T* From(int embedder_process_id, int guest_instance_id) {
    GuestViewBase* guest =
        GuestViewBase::From(embedder_process_id, guest_instance_id);
    if (!guest)
      return NULL;
    return guest->As<T>();
  }

  static T* FromWebContents(content::WebContents* contents) {
    GuestViewBase* guest = GuestViewBase::FromWebContents(contents);
    return guest ? guest->As<T>() : NULL;
  }

  static T* FromFrameID(int render_process_id, int render_frame_id) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (!render_frame_host) {
      return NULL;
    }
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    return FromWebContents(web_contents);
  }

  T* GetOpener() const {
    GuestViewBase* guest = GuestViewBase::GetOpener();
    if (!guest)
      return NULL;
    return guest->As<T>();
  }

  void SetOpener(T* opener) {
    GuestViewBase::SetOpener(opener);
  }

  // GuestViewBase implementation.
  virtual const char* GetViewType() const OVERRIDE {
    return T::Type;
  }

 protected:
  GuestView(content::BrowserContext* browser_context,
            int guest_instance_id)
      : GuestViewBase(browser_context, guest_instance_id) {}
  virtual ~GuestView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestView);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_H_
