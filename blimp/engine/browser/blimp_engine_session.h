// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_
#define BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_message_processor.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/completion_callback.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class WindowTreeHost;

namespace client {
class DefaultCaptureClient;
class WindowTreeClient;
}  // namespace client
}  // namespace aura

namespace content {
class BrowserContext;
class WebContents;
}

namespace wm {
class FocusController;
}

namespace blimp {

class BlimpConnection;
class BlimpMessage;

namespace engine {

class BlimpBrowserContext;
class BlimpFocusClient;
class BlimpScreen;
class BlimpUiContextFactory;
class BlimpWindowTreeHost;

class BlimpEngineSession : public BlimpMessageProcessor,
                           public content::WebContentsDelegate {
 public:
  explicit BlimpEngineSession(scoped_ptr<BlimpBrowserContext> browser_context);
  ~BlimpEngineSession() override;

  void Initialize();

  BlimpBrowserContext* browser_context() { return browser_context_.get(); }

  // BlimpMessageProcessor implementation.
  // TODO(haibinlu): Delete this and move to BlimpMessageDemultiplexer.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // ControlMessage handler methods.
  // Creates a new WebContents, which will be indexed by |target_tab_id|.
  void CreateWebContents(const int target_tab_id);
  void CloseWebContents(const int target_tab_id);

  // NavigationMessage handler methods.
  // Navigates the target tab to the |url|.
  void LoadUrl(const int target_tab_id, const GURL& url);
  void GoBack(const int target_tab_id);
  void GoForward(const int target_tab_id);
  void Reload(const int target_tab_id);

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

  // Context factory for compositor.
  scoped_ptr<BlimpUiContextFactory> context_factory_;

  // Represents the (currently single) browser window into which tab(s) will
  // be rendered.
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;

  // Used to apply standard focus conventions to the windows in the
  // WindowTreeHost hierarchy.
  scoped_ptr<wm::FocusController> focus_client_;

  // Used to manage input capture.
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  // Only one web_contents is supported for blimp 0.5
  scoped_ptr<content::WebContents> web_contents_;

  // Currently attached client connection.
  scoped_ptr<BlimpConnection> client_connection_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEngineSession);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_BLIMP_ENGINE_SESSION_H_
