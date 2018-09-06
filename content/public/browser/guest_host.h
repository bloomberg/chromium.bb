// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_
#define CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_

#include "ui/gfx/geometry/size.h"

namespace content {

class WebContents;

// A GuestHost is the content API for a guest WebContents.
// Guests are top-level frames that can be embedded within other pages.
// The content module manages routing of input events and compositing, but all
// other operations are managed outside of content. To limit exposure of
// implementation details within content, content embedders must use this
// interface for loading, sizing, and cleanup of guests.
//
// This class currently only serves as a base class for BrowserPluginGuest, and
// its API can only be accessed by a BrowserPluginGuestDelegate.
class GuestHost {
 public:
  // Loads a URL using the specified |load_params| and returns a routing ID for
  // a proxy for the guest.
  virtual int LoadURLWithParams(
      const NavigationController::LoadURLParams& load_params) = 0;

  // Sets the size of the guest WebContents.
  virtual void SizeContents(const gfx::Size& new_size) = 0;

  // Called when the GuestHost is about to be destroyed.
  virtual void WillDestroy() = 0;

  // A cue to start the browser side attaching. When a GuestView is rendered
  // inside a plugin element's frame attaching the guest WebContents to the
  // |embedder_web_contents| is initiated right after creating the guest on the
  // browser side. |instance_id| is used to uniquely identify the internal
  // instance of GuestHost inside content layer ad |is_full_page_plugin| is
  // notifies the content layer whether or not the MimeHandlerView is embedded
  // (|is_full_page_plugin| is set to true if a frame (i.e., main frame) is
  // navigated to a MimeHandlerView type and therefore the whole page is
  // dedicated to rendering its content).
  virtual void BeginAttach(WebContents* embedder_web_contents,
                           int instance_id,
                           bool is_full_page_plugin) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_
