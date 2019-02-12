// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromecast/common/mojom/feature_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

struct RendererFeature {
  const std::string name;
  base::Value value;
};

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

    // Notify that a inner WebContents was created. |inner_contents| is created
    // in a default-initialized state with no delegate, and can be safely
    // initialized by the delegate.
    virtual void InnerContentsCreated(CastWebContents* inner_contents,
                                      CastWebContents* outer_contents) {}

   protected:
    virtual ~Delegate() {}
  };

  class Observer {
   public:
    Observer();

    virtual void RenderFrameCreated(int render_process_id,
                                    int render_frame_id) {}

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

  struct InitParams {
    Delegate* delegate;
    bool enabled_for_dev;
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

  // TODO(seantopping): Hide this, clients shouldn't use WebContents directly.
  virtual content::WebContents* web_contents() const = 0;
  virtual PageState page_state() const = 0;

  // ===========================================================================
  // Initialization and Setup
  // ===========================================================================

  // Set the delegate. SetDelegate(nullptr) can be used to stop notifications.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Add a set of features for all renderers in the WebContents. Features are
  // configured when `CastWebContents::RenderFrameCreated` is invoked.
  virtual void AddRendererFeatures(std::vector<RendererFeature> features) = 0;

  virtual void AllowWebAndMojoWebUiBindings() = 0;
  virtual void ClearRenderWidgetHostView() = 0;

  // ===========================================================================
  // Page Lifetime
  // ===========================================================================

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

  // Used to add or remove |observer| to the ObserverList in the implementation.
  // These functions should only be invoked by CastWebContents::Observer in a
  // valid sequence, enforced via SequenceChecker.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Used to expose CastWebContents's |binder_registry_| to Delegate.
  // Delegate should register its mojo interface binders via this function
  // when it is ready.
  virtual service_manager::BinderRegistry* binder_registry() = 0;

  // Used for owner to pass its |InterfaceProviderPtr|s to CastWebContents.
  // It is owner's respoinsibility to make sure each |InterfaceProviderPtr| has
  // distinct mojo interface set.
  using InterfaceSet = base::flat_set<std::string>;
  virtual void RegisterInterfaceProvider(
      const InterfaceSet& interface_set,
      service_manager::InterfaceProvider* interface_provider) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebContents);
};

std::ostream& operator<<(std::ostream& os, CastWebContents::PageState state);

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
