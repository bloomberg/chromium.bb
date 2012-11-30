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
#include "chrome/browser/instant/instant_commit_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

struct InstantAutocompleteResult;
class InstantController;
class TabContents;
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

// InstantLoader is created with an "Instant URL". It loads the URL and tells
// the InstantController of all interesting events. For example, it determines
// if the page supports the Instant API (http://dev.chromium.org/searchbox) and
// forwards messages (such as queries and autocomplete suggestions) between the
// page and the controller.
class InstantLoader : public content::NotificationObserver {
 public:
  // Returns the Instant loader for |web_contents| if it's used for Instant.
  static InstantLoader* FromWebContents(content::WebContents* web_contents);

  // Creates a new empty WebContents. Use Init() to actually load |instant_url|.
  // |tab_contents| is the page the preview will be shown on top of and
  // potentially replace. |instant_url| is typically the instant_url field of
  // the default search engine's TemplateURL, with the "{searchTerms}" parameter
  // replaced with an empty string.
  InstantLoader(InstantController* controller,
                const std::string& instant_url,
                const TabContents* tab_contents);
  virtual ~InstantLoader();

  // Initializes |preview_contents_| and loads |instant_url_|.
  void Init();

  // Tells the preview page that the user typed |user_text| into the omnibox.
  // If |verbatim| is false, the page predicts the query the user means to type
  // and fetches results for the prediction. If |verbatim| is true, |user_text|
  // is taken as the exact query (no prediction is made).
  void Update(const string16& user_text, bool verbatim);

  // Tells the preview page of the bounds of the omnibox dropdown (in screen
  // coordinates). This is used by the page to offset the results to avoid them
  // being covered by the omnibox dropdown.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Tells the preview page about the available autocomplete results.
  void SendAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);

  // Tells the preview page about the current theme background.
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);

  // Tells the preview page about the current theme area height.
  void SendThemeAreaHeight(int height);

  // Tells the preview page whether it is allowed to display Instant results.
  void SetDisplayInstantResults(bool display_instant_results);

  // Tells the preview page that the user pressed the up or down key. |count|
  // is a repeat count, negative for moving up, positive for moving down.
  void OnUpOrDownKeyPressed(int count);

  // Tells the preview page that the active tab's search mode has changed.
  void SearchModeChanged(const chrome::search::Mode& mode);

  // Called by the history tab helper with the information that it would have
  // added to the history service had this web contents not been used for
  // Instant.
  void DidNavigate(const history::HistoryAddPageArgs& add_page_args);

  // Releases the preview TabContents passing ownership to the caller. This
  // should be called when the preview is committed. Notifies the page but not
  // the controller. |text| is the final omnibox text being committed. NOTE: The
  // caller should destroy this loader object right after this method, since
  // none of the other methods will work once the preview has been released.
  TabContents* ReleasePreviewContents(InstantCommitType type,
                                      const string16& text) WARN_UNUSED_RESULT;

  // Severs delegate and observer connections, resets popup blocking, etc., on
  // the |preview_contents_|.
  void CleanupPreviewContents();

  // The preview TabContents. The loader retains ownership. This will be
  // non-NULL until ReleasePreviewContents() is called.
  TabContents* preview_contents() const { return preview_contents_.get(); }

  // Returns true if the preview page is known to support the Instant API. This
  // starts out false, and becomes true whenever we get any message from the
  // page. Once true, it never becomes false (the page isn't expected to drop
  // Instant API support suddenly).
  bool supports_instant() const { return supports_instant_; }

  // Returns the URL that we're loading.
  const std::string& instant_url() const { return instant_url_; }

  // Returns info about the last navigation by the Instant page. If the page
  // hasn't navigated since the last Update(), the URL is empty.
  const history::HistoryAddPageArgs& last_navigation() const {
    return last_navigation_;
  }

  // Returns true if the mouse or a touch pointer is down due to activating the
  // preview content.
  bool IsPointerDownFromActivate() const;

 private:
  class WebContentsDelegateImpl;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void SetupPreviewContents();
  void ReplacePreviewContents(content::WebContents* old_contents,
                              content::WebContents* new_contents);

  InstantController* const controller_;

  // Delegate of the preview WebContents. Used when the user does some gesture
  // on the WebContents and it needs to be activated. This MUST be defined above
  // |preview_contents_| so that the delegate can outlive the WebContents.
  scoped_ptr<WebContentsDelegateImpl> preview_delegate_;

  // See comments on the getter above.
  scoped_ptr<TabContents> preview_contents_;

  // See comments on the getter above.
  bool supports_instant_;

  // See comments on the getter above.
  const std::string instant_url_;

  // Used to get notifications about renderers coming and going.
  content::NotificationRegistrar registrar_;

  // See comments on the getter above.
  history::HistoryAddPageArgs last_navigation_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoader);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
