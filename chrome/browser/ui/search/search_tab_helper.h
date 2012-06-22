// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class AutocompleteEditModel;
class TabContents;

namespace content {
class RenderWidgetHost;
}

namespace chrome {
namespace search {

class SearchModel;

// Per-tab search "helper".  Acts as the owner and controller of the tab's
// search UI model.
class SearchTabHelper : public content::WebContentsObserver,
                        public content::NotificationObserver {
 public:
  SearchTabHelper(TabContents* contents, bool is_search_enabled);
  virtual ~SearchTabHelper();

  SearchModel* model() const {
    return model_.get();
  }

  // Invoked when the AutocompleteEditModel changes state in some way that might
  // affect the search mode.
  void AutocompleteEditModelChanged(AutocompleteEditModel* edit_model);

  // content::WebContentsObserver overrides:
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentLoadedInFrame(int64 frame_id) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Enum of the load states for the NTP.
  //
  // Once the user loads the NTP the |ntp_load_state_| changes to
  // WAITING_FOR_FRAME_ID and the chrome::search::mode::Type changes to
  // MODE_NTP_LOADING. The |ntp_load_state_| progresses through the remaining
  // states and when done chrome::search::mode::Type is changed to MODE_NTP.
  //
  // This code is intended to avoid a flash between white (default background
  // color) and the background the pages wants (gray). We know the CSS has been
  // applied once we get DocumentLoadedInFrame() and we know the backing store
  // has been updated once we get a paint.
  enum NTPLoadState {
    // The default initial state.
    DEFAULT,

    // The user loaded the NTP and we're waiting for the id of the main frame.
    WAITING_FOR_FRAME_ID,

    // We got the frame id (in |main_frame_id_|) and are waiting for the frame
    // to complete loading.
    WAITING_FOR_FRAME_LOAD,

    // The document finished loading. We're now waiting for a paint.
    WAITING_FOR_PAINT,
  };

  // Returns the current RenderWidgetHost of the |web_contents()|.
  content::RenderWidgetHost* GetRenderWidgetHost();

  const bool is_search_enabled_;

  // Model object for UI that cares about search state.
  scoped_ptr<SearchModel> model_;

  content::NotificationRegistrar registrar_;

  // See description above NTPLoadState.
  NTPLoadState ntp_load_state_;

  // See description above NTPLoadState.
  int64 main_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
