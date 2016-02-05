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
#include "base/timer/timer.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/webui/media_router/query_result_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/icu/source/common/unicode/uversion.h"

namespace content {
class WebContents;
}

namespace extensions {
class ExtensionRegistry;
}

namespace U_ICU_NAMESPACE {
class Collator;
}

namespace media_router {

class CreatePresentationConnectionRequest;
class IssuesObserver;
class MediaRoute;
class MediaRouter;
class MediaRouterDialogCallbacks;
class MediaRoutesObserver;
class MediaRouterWebUIMessageHandler;
class MediaSink;
class MediaSinksObserver;
class RouteRequestResult;

// Implements the chrome://media-router user interface.
class MediaRouterUI : public ConstrainedWebDialogUI,
                      public QueryResultManager::Observer,
                      public PresentationServiceDelegateImpl::
                          DefaultPresentationRequestObserver {
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
      scoped_ptr<CreatePresentationConnectionRequest> presentation_request);

  // Closes the media router UI.
  void Close();

  // Notifies this instance that the UI has been initialized.
  void UIInitialized();

  // Requests a route be created from the source mapped to
  // |cast_mode|, to the sink given by |sink_id|.
  // Returns true if a route request is successfully submitted.
  // |OnRouteResponseReceived()| will be invoked when the route request
  // completes.
  bool CreateRoute(const MediaSink::Id& sink_id, MediaCastMode cast_mode);

  // Calls MediaRouter to join the given route.
  bool ConnectRoute(const MediaSink::Id& sink_id,
                    const MediaRoute::Id& route_id);

  // Calls MediaRouter to close the given route.
  void CloseRoute(const MediaRoute::Id& route_id);

  // Calls MediaRouter to add the given issue.
  void AddIssue(const Issue& issue);

  // Calls MediaRouter to clear the given issue.
  void ClearIssue(const Issue::Id& issue_id);

  // Returns the hostname of the default source's parent frame URL.
  std::string GetPresentationRequestSourceName() const;
  std::string GetTruncatedPresentationRequestSourceName() const;
  bool HasPendingRouteRequest() const {
    return current_route_request_id_ != -1;
  }
  const std::vector<MediaSinkWithCastModes>& sinks() const { return sinks_; }
  const std::vector<MediaRoute>& routes() const { return routes_; }
  const std::vector<MediaRoute::Id>& joinable_route_ids() const {
    return joinable_route_ids_;
  }
  const std::set<MediaCastMode>& cast_modes() const { return cast_modes_; }
  const content::WebContents* initiator() const { return initiator_; }

  // Marked virtual for tests.
  virtual const std::string& GetRouteProviderExtensionId() const;

  // Called to track UI metrics.
  void SetUIInitializationTimer(const base::Time& start_time);
  void OnUIInitiallyLoaded();
  void OnUIInitialDataReceived();

  void UpdateMaxDialogHeight(int height);

  void InitForTest(MediaRouter* router,
                   content::WebContents* initiator,
                   MediaRouterWebUIMessageHandler* handler);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, SortedSinks);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverFiltersNonDisplayRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
      UIMediaRoutesObserverFiltersNonDisplayJoinableRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest, GetExtensionNameExtensionPresent);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           GetExtensionNameEmptyWhenNotInstalled);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           GetExtensionNameEmptyWhenNotExtensionURL);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           RouteCreationTimeoutForPresentation);

  class UIIssuesObserver;

  class UIMediaRoutesObserver : public MediaRoutesObserver {
   public:
    using RoutesUpdatedCallback =
        base::Callback<void(const std::vector<MediaRoute>&,
            const std::vector<MediaRoute::Id>&)>;
    UIMediaRoutesObserver(MediaRouter* router, const MediaSource::Id& source_id,
                          const RoutesUpdatedCallback& callback);
    ~UIMediaRoutesObserver() override;

    // MediaRoutesObserver
    void OnRoutesUpdated(
        const std::vector<MediaRoute>& routes,
        const std::vector<MediaRoute::Id>& joinable_route_ids) override;

   private:
    // Callback to the owning MediaRouterUI instance.
    RoutesUpdatedCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(UIMediaRoutesObserver);
  };

  static std::string GetExtensionName(const GURL& url,
                                      extensions::ExtensionRegistry* registry);

  // QueryResultManager::Observer
  void OnResultsUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // Called by |issues_observer_| when the top issue has changed.
  // If the UI is already initialized, notifies |handler_| to update the UI.
  // Ignored if the UI is not yet initialized.
  void SetIssue(const Issue* issue);

  // Called by |routes_observer_| when the set of active routes has changed.
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids);

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  void OnRouteResponseReceived(int route_request_id,
                               const MediaSink::Id& sink_id,
                               const RouteRequestResult& result);

  // Creates and sends an issue if route creation timed out.
  void SendIssueForRouteTimeout();

  // Initializes the dialog with mirroring sources derived from |initiator|.
  void InitCommon(content::WebContents* initiator);

  // PresentationServiceDelegateImpl::DefaultPresentationObserver
  void OnDefaultPresentationChanged(
      const PresentationRequest& presentation_request) override;
  void OnDefaultPresentationRemoved() override;

  // Creates a brand new route or, if a |route_id| is supplied, connects to a
  // non-local route. This is used for connecting to a non-local route.
  // Returns true if a route request is successfully submitted.
  // OnRouteResponseReceived() will be invoked when the route request
  // completes.
  bool CreateOrConnectRoute(const MediaSink::Id& sink_id,
                            MediaCastMode cast_mode,
                            const MediaRoute::Id& route_id);

  // Updates the set of supported cast modes and sends the updated set to
  // |handler_|.
  void UpdateCastModes();

  // Returns the default presentation request's frame URL if there is one.
  // Otherwise returns an empty GURL.
  GURL GetFrameURL() const;

  // Owned by the |web_ui| passed in the ctor, and guaranteed to be deleted
  // only after it has deleted |this|.
  MediaRouterWebUIMessageHandler* handler_;

  // These are non-null while this instance is registered to receive
  // updates from them.
  scoped_ptr<IssuesObserver> issues_observer_;
  scoped_ptr<MediaRoutesObserver> routes_observer_;

  // Set to true by |handler_| when the UI has been initialized.
  bool ui_initialized_;

  // Set to -1 if not tracking a pending route request.
  int current_route_request_id_;

  // Sequential counter for route requests. Used to update
  // |current_route_request_id_| when there is a new route request.
  int route_request_counter_;

  // Used for locale-aware sorting of sinks by name. Set during |InitCommon()|
  // using the current locale. Set to null
  scoped_ptr<icu::Collator> collator_;

  std::vector<MediaSinkWithCastModes> sinks_;
  std::vector<MediaRoute> routes_;
  std::vector<MediaRoute::Id> joinable_route_ids_;
  CastModeSet cast_modes_;

  scoped_ptr<QueryResultManager> query_result_manager_;

  // If set, then the result of the next presentation route request will
  // be handled by this object.
  scoped_ptr<CreatePresentationConnectionRequest> create_session_request_;

  // Set to the presentation request corresponding to the presentation cast
  // mode, if supported. Otherwise set to nullptr.
  scoped_ptr<PresentationRequest> presentation_request_;

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
  MediaRouter* router_;

  // The start time for UI initialization metrics timer. When a dialog has been
  // been painted and initialized with initial data, this should be cleared.
  base::Time start_time_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
