// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_WEB_CONTENTS_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_WEB_CONTENTS_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window_observer.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class RemoteViewProvider;
}

class ChromeKeyboardBoundsObserver;

// WebContents manager for the virtual keyboard. This observes the web
// contents, manages the content::HostZoomMap, and informs the virtual
// keyboard controller when the contents have loaded. It also provides a
// WebContentsDelegate implementation.
class ChromeKeyboardWebContents : public content::WebContentsObserver,
                                  public aura::WindowObserver {
 public:
  using LoadCallback = base::OnceCallback<void(const base::UnguessableToken&,
                                               const gfx::Size& size)>;

  // Immediately starts loading |url| in a WebContents. |callback| is called
  // when the WebContents finishes loading.
  ChromeKeyboardWebContents(content::BrowserContext* context,
                            const GURL& url,
                            LoadCallback load_callback);
  ~ChromeKeyboardWebContents() override;

  // Updates the keyboard URL if |url| does not match the existing url.
  void SetKeyboardUrl(const GURL& url);

  // Called via ash.mojo.KeyboardControllerObserver to provide an initial
  // size for the keyboard contents, necessary to trigger SetContentsBounds
  // in the delegate.
  void SetInitialContentsSize(const gfx::Size& size);

  // Provide access to the native view (aura::Window) and frame
  // (RenderWidgetHostView) through WebContents. TODO(stevenjb): Remove this
  // once host window ownership is moved to ash.
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  // content::WebContentsObserver overrides
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void DidStopLoading() override;

  void OnGotEmbedToken(const base::UnguessableToken& token);

  // Loads the web contents for the given |url|.
  void LoadContents(const GURL& url);

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // |calback_| is run once the contents have stopped loading, the embed
  // token has been received, and the window size is known.
  void MaybeRunLoadCallback();

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ChromeKeyboardBoundsObserver> window_bounds_observer_;

  // Called from DidStopLoading(). If the WindowService is running, passes a
  // token for embedding the contents, otherwise passes an empty token.
  LoadCallback load_callback_;

  base::UnguessableToken token_;
  gfx::Size contents_size_;

  // Helper to prepare the native view of |web_contents_| to be embedded.
  std::unique_ptr<views::RemoteViewProvider> remote_view_provider_;

  base::WeakPtrFactory<ChromeKeyboardWebContents> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardWebContents);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_WEB_CONTENTS_H_
