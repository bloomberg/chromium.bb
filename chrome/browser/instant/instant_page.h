// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_PAGE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_PAGE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_transition_types.h"

class GURL;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// InstantPage is used to exchange messages with a page that implements the
// Instant/Embedded Search API (http://dev.chromium.org/embeddedsearch).
// InstantPage is not used directly but via one of its derived classes:
// InstantOverlay, InstantNTP and InstantTab.
class InstantPage : public content::WebContentsObserver {
 public:
  // InstantPage calls its delegate in response to messages received from the
  // page. Each method is called with the |contents| corresponding to the page
  // we are observing.
  class Delegate {
   public:
    // Called when a RenderView is created, so that state can be initialized.
    virtual void InstantPageRenderViewCreated(
        const content::WebContents* contents) = 0;

    // Called upon determination of Instant API support. Either in response to
    // the page loading or because we received some other message.
    virtual void InstantSupportDetermined(const content::WebContents* contents,
                                          bool supports_instant) = 0;

    // Called when the underlying RenderView crashed.
    virtual void InstantPageRenderViewGone(
        const content::WebContents* contents) = 0;

    // Called when the page is about to navigate to |url|.
    virtual void InstantPageAboutToNavigateMainFrame(
        const content::WebContents* contents,
        const GURL& url) = 0;

    // Called when the page has suggestions. Usually in response to Update(),
    // SendAutocompleteResults() or UpOrDownKeyPressed().
    virtual void SetSuggestions(
        const content::WebContents* contents,
        const std::vector<InstantSuggestion>& suggestions) = 0;

    // Called when the page wants to be shown. Usually in response to Update()
    // or SendAutocompleteResults().
    virtual void ShowInstantOverlay(const content::WebContents* contents,
                                    InstantShownReason reason,
                                    int height,
                                    InstantSizeUnits units) = 0;

    // Called when the page wants the omnibox to start capturing user key
    // strokes. If this call is processed successfully, the omnibox will not
    // look focused visibly but any user key strokes will go to the omnibox.
    // Currently, this is implemented by focusing the omnibox invisibly.
    virtual void StartCapturingKeyStrokes(
        const content::WebContents* contents) = 0;

    // Called when the page wants the omnibox to stop capturing user key
    // strokes.
    virtual void StopCapturingKeyStrokes(content::WebContents* contents) = 0;

    // Called when the page wants to navigate to the specified URL. Usually
    // used by the page to navigate to privileged destinations (e.g. chrome://
    // URLs) or to navigate to URLs that are hidden from the page using
    // Restricted IDs (rid in the API).
    virtual void NavigateToURL(const content::WebContents* contents,
                               const GURL& url,
                               content::PageTransition transition) = 0;

    // Called when the SearchBox wants to delete a Most Visited item.
    virtual void DeleteMostVisitedItem(const GURL& url) = 0;

    // Called when the SearchBox wants to undo a Most Visited deletion.
    virtual void UndoMostVisitedDeletion(const GURL& url) = 0;

    // Called when the SearchBox wants to undo all Most Visited deletions.
    virtual void UndoAllMostVisitedDeletions() = 0;

   protected:
    virtual ~Delegate();
  };

  virtual ~InstantPage();

  // The WebContents corresponding to the page we're talking to. May be NULL.
  content::WebContents* contents() const { return web_contents(); }

  // Returns true if the page is known to support the Instant API. This starts
  // out false, and is set to true whenever we get any message from the page.
  // Once true, it never becomes false (the page isn't expected to drop API
  // support suddenly).
  bool supports_instant() const { return supports_instant_; }

  // Tells the page that the user typed |text| into the omnibox. If |verbatim|
  // is false, the page predicts the query the user means to type and fetches
  // results for the prediction. If |verbatim| is true, |text| is taken as the
  // exact query (no prediction is made).
  virtual void Update(const string16& text,
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
  void SetPopupBounds(const gfx::Rect& bounds);

  // Tells the page the start and end margins of the omnibox (in screen
  // coordinates). This is used by the page to align text or assets properly
  // with the omnibox.
  void SetMarginSize(int start, int end);

  // Tells the page about the font information.
  void InitializeFonts();

  // Tells the renderer to determine if the page supports the Instant API, which
  // results in a call to InstantSupportDetermined() when the reply is received.
  void DetermineIfPageSupportsInstant();

  // Tells the page about the available autocomplete results.
  void SendAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);

  // Tells the page that the user pressed Up or Down in the omnibox. |count| is
  // a repeat count, negative for moving up, positive for moving down.
  void UpOrDownKeyPressed(int count);

  // Tells the page that the user pressed Esc in the omnibox after having
  // arrowed down in the suggestions. The page should reset the selection to
  // the first suggestion. |user_text| is what the omnibox has been reset to.
  void CancelSelection(const string16& user_text);

  // Tells the page about the current theme background.
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);

  // Tells the page whether it is allowed to display Instant results.
  void SetDisplayInstantResults(bool display_instant_results);

  // Tells the page whether the browser is capturing user key strokes.
  void KeyCaptureChanged(bool is_key_capture_enabled);

  // Tells the page about new Most Visited data.
  void SendMostVisitedItems(const std::vector<MostVisitedItem>& items);

 protected:
  explicit InstantPage(Delegate* delegate);

  // Sets |contents| as the page to communicate with. |contents| may be NULL,
  // which effectively stops all communication.
  void SetContents(content::WebContents* contents);

  Delegate* delegate() const { return delegate_; }

  // These functions are called before processing messages received from the
  // page. By default, all messages are handled, but any derived classes may
  // choose to ingore some or all of the received messages by overriding these
  // methods.
  virtual bool ShouldProcessRenderViewCreated();
  virtual bool ShouldProcessRenderViewGone();
  virtual bool ShouldProcessAboutToNavigateMainFrame();
  virtual bool ShouldProcessSetSuggestions();
  virtual bool ShouldProcessShowInstantOverlay();
  virtual bool ShouldProcessStartCapturingKeyStrokes();
  virtual bool ShouldProcessStopCapturingKeyStrokes();
  virtual bool ShouldProcessNavigateToURL();

 private:
  // Overridden from content::WebContentsObserver:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
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

  void OnSetSuggestions(int page_id,
                        const std::vector<InstantSuggestion>& suggestions);
  void OnInstantSupportDetermined(int page_id, bool supports_instant);
  void OnShowInstantOverlay(int page_id,
                            InstantShownReason reason,
                            int height,
                            InstantSizeUnits units);
  void OnStartCapturingKeyStrokes(int page_id);
  void OnStopCapturingKeyStrokes(int page_id);
  void OnSearchBoxNavigate(int page_id, const GURL& url,
                           content::PageTransition transition);
  void OnDeleteMostVisitedItem(const GURL& url);
  void OnUndoMostVisitedDeletion(const GURL& url);
  void OnUndoAllMostVisitedDeletions();

  Delegate* const delegate_;
  bool supports_instant_;

  DISALLOW_COPY_AND_ASSIGN(InstantPage);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_PAGE_H_
