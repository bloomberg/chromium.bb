// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chromecast/common/mojom/feature_manager.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// Simplified WebContents wrapper class for Cast platforms.
class CastWebContents {
 public:
  class Delegate {
   public:
    // Advertises page state for the CastWebContents.
    // Use CastWebContents::page_state() to get the new state.
    virtual void OnPageStateChanged(CastWebContents* cast_web_contents) = 0;

    // Called when the page has stopped. e.g.: A 404 occurred when loading the
    // page or if the render process for the main frame crashes. |error_code|
    // will return a net::Error describing the failure, or net::OK if the page
    // closed naturally.
    //
    // After this method, the page state will be one of the following:
    // CLOSED: Page was closed as expected and the WebContents exists.
    // DESTROYED: Page was closed due to deletion of WebContents. The
    //     CastWebContents instance is no longer usable and should be deleted.
    // ERROR: Page is in an error state. It should be reloaded or deleted.
    virtual void OnPageStopped(CastWebContents* cast_web_contents,
                               int error_code) = 0;

    // Collects the set of delegate-specific renderer features that needs to be
    // configured when `CastWebContents::RenderFrameCreated` is invoked.
    virtual std::vector<chromecast::shell::mojom::FeaturePtr>
    GetRendererFeatures();

   protected:
    virtual ~Delegate() {}
  };

  class Observer {
   public:
    Observer();

    virtual void RenderFrameCreated(int render_process_id,
                                    int render_frame_id) {}
    virtual void OnInterfaceRequestFromFrame(
        const std::string& interface_name,
        mojo::ScopedMessagePipeHandle* interface_pipe) {}

    // Adds |this| to the ObserverList in the implementation of
    // |cast_web_contents|.
    void Observe(CastWebContents* cast_web_contents);

    // Removes |this| from the ObserverList in the implementation of
    // |cast_web_contents_|. This is only invoked by CastWebContents and is used
    // to ensure that once the observed CastWebContents object is destructed the
    // CastWebContents::Observer does not invoke any additional function calls
    // on it.
    void ResetCastWebContents();

   protected:
    virtual ~Observer();

    CastWebContents* cast_web_contents_;
  };

  // Page state for the main frame.
  enum class PageState {
    IDLE,       // Main frame has not started yet.
    LOADING,    // Main frame is loading resources.
    LOADED,     // Main frame is loaded, but sub-frames may still be loading.
    CLOSED,     // Page is closed and should be cleaned up.
    DESTROYED,  // The WebContents is destroyed and can no longer be used.
    ERROR,      // Main frame is in an error state.
  };

  CastWebContents() = default;
  virtual ~CastWebContents() = default;

  virtual content::WebContents* web_contents() const = 0;
  virtual PageState page_state() const = 0;

  // Navigates the underlying WebContents to |url|. Delegate will be notified of
  // page progression events via OnPageStateChanged().
  virtual void LoadUrl(const GURL& url) = 0;

  // Initiate closure of the page. This invokes the appropriate unload handlers.
  // Eventually the delegate will be notified with OnPageStopped().
  virtual void ClosePage() = 0;

  // Stop the page immediately. This will automatically invoke
  // Delegate::OnPageStopped(error_code), allowing the delegate to delete or
  // reload the page without waiting for page teardown, which may be handled
  // independently.
  virtual void Stop(int error_code) = 0;

  // Set the delegate. SetDelegate(nullptr) can be used to stop notifications.
  virtual void SetDelegate(Delegate* delegate) = 0;

  virtual void AllowWebAndMojoWebUiBindings() = 0;
  virtual void ClearRenderWidgetHostView() = 0;

  // Used to add or remove |observer| to the ObserverList in the implementation.
  // These functions should only be invoked by CastWebContents::Observer in a
  // valid sequence, enforced via SequenceChecker.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebContents);
};

std::ostream& operator<<(std::ostream& os, CastWebContents::PageState state);

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
