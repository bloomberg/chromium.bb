// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/webui/media_router/query_result_manager.h"
#include "content/public/browser/web_ui_data_source.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class IssuesObserver;
class MediaRoute;
class MediaRouter;
class MediaRouterDialogCallbacks;
class MediaRouterMojoImpl;
class MediaRouterWebUIMessageHandler;
class MediaRoutesObserver;
class MediaSink;
class MediaSinksObserver;
class CreatePresentationSessionRequest;

// Implements the chrome://media-router user interface.
class MediaRouterUI
    : public ConstrainedWebDialogUI,
      public QueryResultManager::Observer,
      public PresentationServiceDelegateImpl::DefaultMediaSourceObserver {
 public:
  // |web_ui| owns this object and is used to initialize the base class.
  explicit MediaRouterUI(content::WebUI* web_ui);
  ~MediaRouterUI() override;

  // Initializes internal state (e.g. starts listening for MediaSinks) for
  // targeting the default MediaSource (if any) of the initiator tab that owns
  // |delegate|, as well as mirroring sources of that tab.
  // The contents of the UI will change as the default MediaSource changes.
  // If there is a default MediaSource, then DEFAULT MediaCastMode will be
  // added to |cast_modes_|.
  // Init* methods can only be called once.
  // |delegate|: PresentationServiceDelegateImpl of the initiator tab.
  //             Must not be null.
  // TODO(imcheng): Replace use of impl with an intermediate abstract
  // interface.
  void InitWithDefaultMediaSource(
      const base::WeakPtr<PresentationServiceDelegateImpl>& delegate);

  // Initializes internal state targeting the presentation specified in
  // |request|. Also sets up mirroring sources based on |initiator|.
  // This is different from |InitWithDefaultMediaSource| in that it does not
  // listen for default media source changes, as the UI is fixed to the source
  // in |request|.
  // Init* methods can only be called once.
  // |initiator|: Reference to the WebContents that initiated the dialog.
  //              Must not be null.
  // |delegate|: PresentationServiceDelegateImpl of the initiator tab.
  //             Must not be null.
  // |presentation_request|: The presentation request. This instance will take
  //                         ownership of it. Must not be null.
  void InitWithPresentationSessionRequest(
      content::WebContents* initiator,
      const base::WeakPtr<PresentationServiceDelegateImpl>& delegate,
      scoped_ptr<CreatePresentationSessionRequest> presentation_request);

  // Closes the media router UI.
  void Close();

  // Notifies this instance that the UI has been initialized.
  void UIInitialized();

  // Requests a route be created from the source determined by the preferred
  // MediaCastMode, to the sink given by |sink_id|.
  // The preferred cast mode is determined from the set of currently supported
  // cast modes in |cast_modes_|.
  // Returns false if unable to request the route.
  // |OnRouteResponseReceived()| will be invoked when the route request
  // completes.
  bool CreateRoute(const MediaSink::Id& sink_id);

  // Requests a route be created from the source mapped to
  // |cast_mode_override|, to the sink given by |sink_id|.
  // Returns true if a route request is successfully submitted.
  bool CreateRouteWithCastModeOverride(const MediaSink::Id& sink_id,
                                       MediaCastMode cast_mode_override);

  // Calls MediaRouter to close the given route.
  void CloseRoute(const MediaRoute::Id& route_id);

  // Calls MediaRouter to clear the given issue.
  void ClearIssue(const Issue::Id& issue_id);

  // Returns the header text that should be displayed in the UI when it is
  // initially loaded. The header text is determined by the preferred cast mode.
  std::string GetInitialHeaderText() const;

  // Returns the tooltip text for the header that should be displayed
  // in the UI when it is initially loaded. At present, this text is
  // just the full hostname of the current site.
  std::string GetInitialHeaderTextTooltip() const;

  // Returns the hostname of the default source's parent frame URL.
  std::string GetFrameURLHost() const;
  bool has_pending_route_request() const { return has_pending_route_request_; }
  const GURL& frame_url() const { return frame_url_; }
  const std::vector<MediaSinkWithCastModes>& sinks() const { return sinks_; }
  const std::vector<MediaRoute>& routes() const { return routes_; }
  const std::set<MediaCastMode>& cast_modes() const { return cast_modes_; }
  const content::WebContents* initiator() const { return initiator_; }

  const std::string& GetRouteProviderExtensionId() const;

 private:
  class UIIssuesObserver;
  class UIMediaRoutesObserver;

  // QueryResultManager::Observer
  void OnResultsUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // Called by |issues_observer_| when the top issue has changed.
  // If the UI is already initialized, notifies |handler_| to update the UI.
  // Ignored if the UI is not yet initialized.
  void SetIssue(const Issue* issue);

  // Called by |routes_observer_| when the set of active routes has changed.
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes);

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  void OnRouteResponseReceived(const MediaSink::Id& sink_id,
                               const MediaRoute* route,
                               const std::string& presentation_id,
                               const std::string& error);

  bool DoCreateRoute(const MediaSink::Id& sink_id, MediaCastMode cast_mode);

  // Sets the source host name to be displayed in the UI.
  // Gets cast modes from |query_result_manager_| and forwards it to UI.
  // One of the Init* functions must have been called before.
  void UpdateSourceHostAndCastModes(const GURL& frame_url);

  // Initializes the dialog with mirroring sources derived from |initiator|,
  // and optional |default_source| and |default_frame_url| if any.
  void InitCommon(content::WebContents* initiator,
                  const MediaSource& default_source,
                  const GURL& default_frame_url);

  // PresentationServiceDelegateImpl::DefaultMediaSourceObserver
  void OnDefaultMediaSourceChanged(const MediaSource& source,
                                   const GURL& frame_url) override;

  // Owned by the |web_ui| passed in the ctor, and guaranteed to be deleted
  // only after it has deleted |this|.
  MediaRouterWebUIMessageHandler* handler_;

  // These are non-null while this instance is registered to receive
  // updates from them.
  scoped_ptr<IssuesObserver> issues_observer_;
  scoped_ptr<MediaRoutesObserver> routes_observer_;

  // Set to true by |handler_| when the UI has been initialized.
  bool ui_initialized_;

  // Set to |true| if there is a pending route request for this UI.
  bool has_pending_route_request_;

  bool requesting_route_for_default_source_;

  std::vector<MediaSinkWithCastModes> sinks_;
  std::vector<MediaRoute> routes_;
  CastModeSet cast_modes_;
  GURL frame_url_;

  scoped_ptr<QueryResultManager> query_result_manager_;

  // If set, then the result of the next presentation route request will
  // be handled by this object.
  scoped_ptr<CreatePresentationSessionRequest> presentation_request_;

  // It's possible for PresentationServiceDelegateImpl to be destroyed before
  // this class.
  // (e.g. if a tab with the UI open is closed, then the tab WebContents will
  // be destroyed first momentarily before the UI WebContents).
  // Holding a WeakPtr to PresentationServiceDelegateImpl is the cleanest way to
  // handle this.
  // TODO(imcheng): hold a weak ptr to an abstract type instead.
  base::WeakPtr<PresentationServiceDelegateImpl> presentation_service_delegate_;

  content::WebContents* initiator_;

  // Pointer to the MediaRouter for this instance's BrowserContext.
  MediaRouterMojoImpl* router_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
