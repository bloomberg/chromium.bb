// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/webui/media_router/query_result_manager.h"
#include "content/public/browser/web_ui_data_source.h"

namespace media_router {

class IssuesObserver;
class MediaRouterWebUIMessageHandler;
class MediaRoutesObserver;

// Implements the chrome://media-router user interface.
class MediaRouterUI : public ConstrainedWebDialogUI,
                      public QueryResultManager::Observer {
 public:
  // |web_ui| owns this object and is used to initialize the base class.
  explicit MediaRouterUI(content::WebUI* web_ui);
  ~MediaRouterUI() override;

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
  bool CreateRoute(const MediaSinkId& sink_id);

  // Requests a route be created from the source mapped to
  // |cast_mode_override|, to the sink given by |sink_id|.
  // Returns true if a route request is successfully submitted.
  bool CreateRouteWithCastModeOverride(const MediaSinkId& sink_id,
                                       MediaCastMode cast_mode_override);

  // Calls MediaRouter to close the given route.
  void CloseRoute(const MediaRouteId& route_id);

  // Calls MediaRouter to clear the given issue.
  void ClearIssue(const Issue::IssueId& issue_id);

  // Returns the header text that should be displayed in the UI when it is
  // initially loaded. The header text is determined by the preferred cast mode.
  std::string GetInitialHeaderText() const;

  bool has_pending_route_request() const { return has_pending_route_request_; }
  const std::vector<MediaSinkWithCastModes>& sinks() const { return sinks_; }
  const std::vector<MediaRoute>& routes() const { return routes_; }
  const std::set<MediaCastMode>& cast_modes() const { return cast_modes_; }

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
  void OnRouteResponseReceived(scoped_ptr<MediaRoute> route,
                               const std::string& error);

  bool DoCreateRoute(const MediaSinkId& sink_id, MediaCastMode cast_mode);

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

  std::vector<MediaSinkWithCastModes> sinks_;
  std::vector<MediaRoute> routes_;
  CastModeSet cast_modes_;

  scoped_ptr<QueryResultManager> query_result_manager_;

  // Cached pointer to the MediaRouter for this instance's BrowserContext.
  MediaRouter* router_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
