// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_LOADER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_LOADER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"

class GURL;
class Profile;

namespace content {
struct OpenURLParams;
class WebContents;
}

// InstantLoader is used to create and maintain a WebContents where we can
// preload a page into. It is used by InstantOverlay and InstantNTP to
// preload an Instant page.
class InstantLoader : public content::NotificationObserver,
                      public content::WebContentsDelegate,
                      public CoreTabHelperDelegate {
 public:
  // InstantLoader calls these methods on its delegate in response to certain
  // changes in the underlying contents.
  class Delegate {
   public:
    // Called after someone has swapped in a different WebContents for ours.
    virtual void OnSwappedContents() = 0;

    // Called when the underlying contents receive focus.
    virtual void OnFocus() = 0;

    // Called when the mouse pointer is down.
    virtual void OnMouseDown() = 0;

    // Called when the mouse pointer is released (or a drag event ends).
    virtual void OnMouseUp() = 0;

    // Called to open a URL using the underlying contents (see
    // WebContentsDelegate::OpenURLFromTab). The Delegate should return the
    // WebContents the URL is opened in, or NULL if the URL wasn't opened
    // immediately.
    virtual content::WebContents* OpenURLFromTab(
        content::WebContents* source,
        const content::OpenURLParams& params) = 0;

    // Called when a main frame load is complete.
    virtual void LoadCompletedMainFrame() = 0;

   protected:
    ~Delegate();
  };

  explicit InstantLoader(Delegate* delegate);
  virtual ~InstantLoader();

  // Creates a new WebContents in the context of |profile| that will be used to
  // load |instant_url|. The page is not actually loaded until Load() is
  // called. Uses |active_contents|, if non-NULL, to initialize the size of the
  // new contents. |on_stale_callback| will be called after kStalePageTimeoutMS
  // has elapsed after Load() being called.
  void Init(const GURL& instant_url,
            Profile* profile,
            const content::WebContents* active_contents,
            const base::Closure& on_stale_callback);

  // Loads |instant_url_| in |contents_|.
  void Load();

  // Returns the contents currently held. May be NULL.
  content::WebContents* contents() const { return contents_.get(); }

  // Replaces the contents held with |contents|. Any existing contents is
  // deleted. The expiration timer is not restarted.
  void SetContents(scoped_ptr<content::WebContents> contents);

  // Releases the contents currently held. Must only be called if contents() is
  // not NULL.
  scoped_ptr<content::WebContents> ReleaseContents() WARN_UNUSED_RESULT;

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  virtual void SwapTabContents(content::WebContents* old_contents,
                               content::WebContents* new_contents) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual void CanDownload(content::RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method,
                           const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandlePointerActivate() OVERRIDE;
  virtual void HandleGestureEnd() OVERRIDE;
  virtual void DragEnded() OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  Delegate* const delegate_;
  scoped_ptr<content::WebContents> contents_;

  // The URL we will be loading.
  GURL instant_url_;

  // Called when |stale_page_timer_| fires.
  base::Closure on_stale_callback_;

  // Used to mark when the page is stale.
  base::Timer stale_page_timer_;

  // Used to get notifications about renderers.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoader);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_LOADER_H_
