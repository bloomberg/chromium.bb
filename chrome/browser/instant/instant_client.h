// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CLIENT_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CLIENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/web_contents_observer.h"

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

// InstantClient is used to exchange messages between its delegate and a page
// that supports the Instant API (http://dev.chromium.org/searchbox).
class InstantClient : public content::WebContentsObserver {
 public:
  // When InstantClient receives messages from the page, it calls the following
  // methods on its delegate.
  class Delegate {
   public:
    // Called when the page has suggestions. Usually in response to Change(),
    // SendAutocompleteResults() or UpOrDownKeyPressed().
    virtual void SetSuggestions(
        const std::vector<InstantSuggestion>& suggestions) = 0;

    // Called upon determination of Instant API support. Usually in response to
    // SetContents() or when the page first finishes loading.
    virtual void InstantSupportDetermined(bool supports_instant) = 0;

    // Called when the page wants to be shown. Usually in response to Change(),
    // SendAutocompleteResults() or SearchModeChanged().
    virtual void ShowInstantPreview(InstantShownReason reason,
                                    int height,
                                    InstantSizeUnits units) = 0;

    // Called when the page wants the browser to start capturing user key
    // strokes.
    virtual void StartCapturingKeyStrokes() = 0;

    // Called when the page wants the browser to stop capturing user key
    // strokes.
    virtual void StopCapturingKeyStrokes() = 0;

    // Called when the underlying RenderView crashes.
    virtual void RenderViewGone() = 0;

    // Called when the page is about to navigate.
    virtual void AboutToNavigateMainFrame(const GURL& url) = 0;

   protected:
    virtual ~Delegate();
  };

  // Doesn't take ownership of |delegate|.
  explicit InstantClient(Delegate* delegate);
  virtual ~InstantClient();

  // Sets |contents| as the page to communicate with. |contents| can be NULL,
  // which effectively stops all communication.
  void SetContents(content::WebContents* contents);

  // Tells the page that the user typed |text| into the omnibox. If |verbatim|
  // is false, the page predicts the query the user means to type and fetches
  // results for the prediction. If |verbatim| is true, |text| is taken as the
  // exact query (no prediction is made).
  void Update(const string16& text,
              size_t selection_start,
              size_t selection_end,
              bool verbatim);

  // Tells the page that the user pressed Enter in the omnibox.
  void Submit(const string16& text);

  // Tells the page that the user clicked on it. Nothing is being cancelled; the
  // poor choice of name merely reflects the IPC of the same (poor) name.
  void Cancel(const string16& text);

  // Tells the page the bounds of the omnibox dropdown (in screen coordinates).
  // This is used by the page to offset the results to avoid them being covered
  // by the omnibox dropdown.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Tells the renderer to determine if the page supports the Instant API, which
  // results in a call to InstantSupportDetermined() when the reply is received.
  void DetermineIfPageSupportsInstant();

  // Tells the page about the available autocomplete results.
  void SendAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);

  // Tells the page that the user pressed Up or Down in the omnibox. |count| is
  // a repeat count, negative for moving up, positive for moving down.
  void UpOrDownKeyPressed(int count);

  // Tells the page that the active tab's search mode has changed.
  void SearchModeChanged(const chrome::search::Mode& mode);

  // Tells the page about the current theme background.
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);

  // Tells the page about the current theme area height.
  void SendThemeAreaHeight(int height);

  // Tells the page whether it is allowed to display Instant results.
  void SetDisplayInstantResults(bool display_instant_results);

  // Tells the page whether the browser is capturing user key strokes.
  void KeyCaptureChanged(bool is_key_capture_enabled);

 private:
  // Overridden from content::WebContentsObserver:
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

  void SetSuggestions(int page_id,
                      const std::vector<InstantSuggestion>& suggestions);
  void InstantSupportDetermined(int page_id, bool result);
  void ShowInstantPreview(int page_id,
                          InstantShownReason reason,
                          int height,
                          InstantSizeUnits units);
  void StartCapturingKeyStrokes(int page_id);
  void StopCapturingKeyStrokes(int page_id);

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(InstantClient);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CLIENT_H_
