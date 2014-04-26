// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_H_
#define CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_H_

#include "chrome/browser/guest_view/guest_view_base.h"

template <typename T>
class GuestView : public GuestViewBase {
 public:
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

  // GuestViewBase implementation.
  virtual const std::string& GetViewType() const OVERRIDE { return T::Type; }

 protected:
  GuestView(content::WebContents* guest_web_contents,
            const std::string& embedder_extension_id)
      : GuestViewBase(guest_web_contents, embedder_extension_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestView);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_H_
