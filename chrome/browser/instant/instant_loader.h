// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/instant/instant_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

struct InstantAutocompleteResult;
class InstantController;
struct ThemeBackgroundInfo;

namespace chrome {
namespace search {
struct Mode;
}
}

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// InstantLoader is used to communicate with a preview WebContents that it owns
// and loads the "Instant URL" into. This preview can appear and disappear at
// will as the user types in the omnibox (compare: InstantTab, which talks to a
// committed tab on the tab strip).
class InstantLoader : public InstantClient::Delegate,
                      public content::NotificationObserver {
 public:
  // Returns the Instant loader for |contents| if it's used for Instant.
  static InstantLoader* FromWebContents(const content::WebContents* contents);

  // Doesn't take ownership of |controller|.
  InstantLoader(InstantController* controller, const std::string& instant_url);
  virtual ~InstantLoader();

  // The preview WebContents. InstantLoader retains ownership. This will be
  // non-NULL after InitFromContents(), and until ReleaseContents() is called.
  content::WebContents* contents() const { return contents_.get(); }

  // Creates a new WebContents and loads |instant_url_| into it. |active_tab| is
  // the page the preview will be shown on top of and potentially replace.
  void InitContents(const content::WebContents* active_tab);

  // Releases the preview WebContents passing ownership to the caller. This
  // should be called when the preview is committed.
  content::WebContents* ReleaseContents() WARN_UNUSED_RESULT;

  // Returns the URL that we're loading.
  const std::string& instant_url() const { return instant_url_; }

  // Returns true if the preview is known to support the Instant API. This
  // starts out false, and becomes true whenever we get any message from the
  // page. Once true, it never becomes false (the page isn't expected to drop
  // Instant API support suddenly).
  bool supports_instant() const { return supports_instant_; }

  // Returns true if the mouse or a touch pointer is down due to activating the
  // preview contents.
  bool is_pointer_down_from_activate() const {
    return is_pointer_down_from_activate_;
  }

  // Returns info about the last navigation by the Instant page. If the page
  // hasn't navigated since the last Update(), the URL is empty.
  const history::HistoryAddPageArgs& last_navigation() const {
    return last_navigation_;
  }

  // Called by the history tab helper with information that it would have added
  // to the history service had this WebContents not been used for Instant.
  void DidNavigate(const history::HistoryAddPageArgs& add_page_args);

  // Calls through to methods of the same name on InstantClient.
  void Update(const string16& text,
              size_t selection_start,
              size_t selection_end,
              bool verbatim);
  void Submit(const string16& text);
  void Cancel(const string16& text);
  void SetOmniboxBounds(const gfx::Rect& bounds);
  void SendAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);
  void UpOrDownKeyPressed(int count);
  void SearchModeChanged(const chrome::search::Mode& mode);
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);
  void SendThemeAreaHeight(int height);
  void SetDisplayInstantResults(bool display_instant_results);

 private:
  class WebContentsDelegateImpl;

  // Overridden from InstantClient::Delegate:
  virtual void SetSuggestions(
      const std::vector<InstantSuggestion>& suggestions) OVERRIDE;
  virtual void InstantSupportDetermined(bool supports_instant) OVERRIDE;
  virtual void ShowInstantPreview(InstantShownReason reason,
                                  int height,
                                  InstantSizeUnits units) OVERRIDE;
  virtual void StartCapturingKeyStrokes() OVERRIDE;
  virtual void StopCapturingKeyStrokes() OVERRIDE;
  virtual void RenderViewGone() OVERRIDE;
  virtual void AboutToNavigateMainFrame(const GURL& url) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void SetupPreviewContents();
  void CleanupPreviewContents();
  void ReplacePreviewContents(content::WebContents* old_contents,
                              content::WebContents* new_contents);

  InstantClient client_;
  InstantController* const controller_;

  // Delegate of the preview WebContents. Used when the user does some gesture
  // on the preview and it needs to be activated.
  scoped_ptr<WebContentsDelegateImpl> delegate_;
  scoped_ptr<content::WebContents> contents_;

  const std::string instant_url_;
  bool supports_instant_;
  bool is_pointer_down_from_activate_;
  history::HistoryAddPageArgs last_navigation_;

  // Used to get notifications about renderers coming and going.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoader);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
