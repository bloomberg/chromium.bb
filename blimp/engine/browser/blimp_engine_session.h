// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_
#define BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_message_receiver.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace blimp {

class BlimpConnection;
class BlimpMessage;

namespace engine {

class BlimpBrowserContext;
class BlimpScreen;

class BlimpEngineSession : public BlimpMessageReceiver,
                           public content::WebContentsDelegate {
 public:
  explicit BlimpEngineSession(scoped_ptr<BlimpBrowserContext> browser_context);
  ~BlimpEngineSession() override;

  void Initialize();

  BlimpBrowserContext* browser_context() { return browser_context_.get(); }

  void AttachClientConnection(scoped_ptr<BlimpConnection> client_connection);

 private:
  // Creates a new WebContents, which will be indexed by |target_tab_id|.
  void CreateWebContents(const int target_tab_id);

  // Navigates the target tab to the |url|.
  void LoadUrl(const int target_tab_id, const GURL& url);

  // BlimpMessageReceiver implementation.
  net::Error OnBlimpMessage(const BlimpMessage& message) override;

  // content::WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;

  // Sets up and owns |new_contents|.
  void PlatformSetContents(scoped_ptr<content::WebContents> new_contents);

  scoped_ptr<BlimpBrowserContext> browser_context_;
  scoped_ptr<BlimpScreen> screen_;

  // Only one web_contents is supported for blimp 0.5
  scoped_ptr<content::WebContents> web_contents_;

  // Currently attached client connection.
  scoped_ptr<BlimpConnection> client_connection_;
  DISALLOW_COPY_AND_ASSIGN(BlimpEngineSession);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_
