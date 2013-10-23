// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace extensions {
class Extension;
struct Message;

// A web contents observer that's used for WebContents in renderer and extension
// processes.
class ExtensionWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExtensionWebContentsObserver> {
 public:
  virtual ~ExtensionWebContentsObserver();

 private:
  explicit ExtensionWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ExtensionWebContentsObserver>;

  // content::WebContentsObserver overrides.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnPostMessage(int port_id, const Message& message);

  // Gets the extension or app (if any) that is associated with a RVH.
  const Extension* GetExtension(content::RenderViewHost* render_view_host);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
