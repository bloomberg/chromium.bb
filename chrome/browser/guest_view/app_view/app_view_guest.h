// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
#define CHROME_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_

#include "chrome/browser/guest_view/guest_view.h"

class AppViewGuest : public GuestView<AppViewGuest> {
 public:
  static const char Type[];

  AppViewGuest(content::BrowserContext* browser_context,
               int guest_instance_id);

  // GuestViewBase implementation.
  virtual void CreateWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback) OVERRIDE;
  virtual void DidAttachToEmbedder() OVERRIDE;

 private:
  virtual ~AppViewGuest();
};

#endif  // CHROME_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
