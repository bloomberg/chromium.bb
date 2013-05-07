// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_page.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"

InstantPage::Delegate::~Delegate() {
}

InstantPage::~InstantPage() {
}

bool InstantPage::IsLocal() const {
  return contents() &&
      (contents()->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl) ||
       contents()->GetURL() == GURL(chrome::kChromeSearchLocalGoogleNtpUrl));
}

void InstantPage::Update(const string16& text,
                         size_t selection_start,
                         size_t selection_end,
                         bool verbatim) {
  Send(new ChromeViewMsg_SearchBoxChange(routing_id(), text, verbatim,
                                         selection_start, selection_end));
}

void InstantPage::Submit(const string16& text) {
  Send(new ChromeViewMsg_SearchBoxSubmit(routing_id(), text));
}

void InstantPage::Cancel(const string16& text) {
  Send(new ChromeViewMsg_SearchBoxCancel(routing_id(), text));
}

void InstantPage::SetPopupBounds(const gfx::Rect& bounds) {
  Send(new ChromeViewMsg_SearchBoxPopupResize(routing_id(), bounds));
}

void InstantPage::SetOmniboxBounds(const gfx::Rect& bounds) {
  Send(new ChromeViewMsg_SearchBoxMarginChange(
      routing_id(), bounds.x(), bounds.width()));
}

void InstantPage::InitializeFonts() {
#if defined(OS_MACOSX)
  // This value should be kept in sync with OmniboxViewMac::GetFieldFont.
  const gfx::Font omnibox_font("arial", 16);
#else
  const gfx::Font& omnibox_font =
      ui::ResourceBundle::GetSharedInstance().GetFont(
          ui::ResourceBundle::MediumFont);
#endif
  string16 omnibox_font_name = UTF8ToUTF16(omnibox_font.GetFontName());
  size_t omnibox_font_size = omnibox_font.GetFontSize();
  Send(new ChromeViewMsg_SearchBoxFontInformation(
      routing_id(), omnibox_font_name, omnibox_font_size));
}

void InstantPage::DetermineIfPageSupportsInstant() {
  Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
}

void InstantPage::SendAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  Send(new ChromeViewMsg_SearchBoxAutocompleteResults(routing_id(), results));
}

void InstantPage::UpOrDownKeyPressed(int count) {
  Send(new ChromeViewMsg_SearchBoxUpOrDownKeyPressed(routing_id(), count));
}

void InstantPage::EscKeyPressed() {
  Send(new ChromeViewMsg_SearchBoxEscKeyPressed(routing_id()));
}

void InstantPage::CancelSelection(const string16& user_text,
                                  size_t selection_start,
                                  size_t selection_end,
                                  bool verbatim) {
  Send(new ChromeViewMsg_SearchBoxCancelSelection(
      routing_id(), user_text, verbatim, selection_start, selection_end));
}

void InstantPage::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  Send(new ChromeViewMsg_SearchBoxThemeChanged(routing_id(), theme_info));
}

void InstantPage::SetDisplayInstantResults(bool display_instant_results) {
  Send(new ChromeViewMsg_SearchBoxSetDisplayInstantResults(
      routing_id(), display_instant_results));
}

void InstantPage::KeyCaptureChanged(bool is_key_capture_enabled) {
  Send(new ChromeViewMsg_SearchBoxKeyCaptureChanged(
      routing_id(), is_key_capture_enabled));
}

void InstantPage::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItemIDPair>& items) {
  Send(new ChromeViewMsg_SearchBoxMostVisitedItemsChanged(routing_id(), items));
}

InstantPage::InstantPage(Delegate* delegate, const std::string& instant_url)
    : delegate_(delegate),
      instant_url_(instant_url),
      supports_instant_(false) {
}

void InstantPage::SetContents(content::WebContents* contents) {
  Observe(contents);
}

bool InstantPage::ShouldProcessRenderViewCreated() {
  return false;
}

bool InstantPage::ShouldProcessRenderViewGone() {
  return false;
}

bool InstantPage::ShouldProcessAboutToNavigateMainFrame() {
  return false;
}

bool InstantPage::ShouldProcessSetSuggestions() {
  return false;
}

bool InstantPage::ShouldProcessShowInstantOverlay() {
  return false;
}

bool InstantPage::ShouldProcessFocusOmnibox() {
  return false;
}

bool InstantPage::ShouldProcessNavigateToURL() {
  return false;
}

void InstantPage::RenderViewCreated(content::RenderViewHost* render_view_host) {
  if (ShouldProcessRenderViewCreated())
    delegate_->InstantPageRenderViewCreated(contents());
}

void InstantPage::DidFinishLoad(
    int64 /* frame_id */,
    const GURL& /* validated_url */,
    bool is_main_frame,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame && !supports_instant_)
    DetermineIfPageSupportsInstant();
}

bool InstantPage::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InstantPage, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetSuggestions, OnSetSuggestions)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ShowInstantOverlay,
                        OnShowInstantOverlay)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FocusOmnibox, OnFocusOmnibox)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxNavigate,
                        OnSearchBoxNavigate);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem,
                        OnDeleteMostVisitedItem);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion,
                        OnUndoMostVisitedDeletion);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions,
                        OnUndoAllMostVisitedDeletions);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void InstantPage::RenderViewGone(base::TerminationStatus /* status */) {
  if (ShouldProcessRenderViewGone())
    delegate_->InstantPageRenderViewGone(contents());
}

void InstantPage::DidCommitProvisionalLoadForFrame(
    int64 /* frame_id */,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition /* transition_type */,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame && ShouldProcessAboutToNavigateMainFrame())
    delegate_->InstantPageAboutToNavigateMainFrame(contents(), url);
}

void InstantPage::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& /* params */) {
  // A 204 can be sent by the search provider as a lightweight signal
  // to fall back to the local page, and we obviously want to fall back
  // if we get any response code that indicates an error.
  if (details.http_status_code == 204 || details.http_status_code >= 400)
    delegate_->InstantPageLoadFailed(contents());
}

void InstantPage::DidFailProvisionalLoad(
    int64 /* frame_id */,
    bool is_main_frame,
    const GURL& /* validated_url */,
    int /* error_code */,
    const string16& /* error_description */,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame)
    delegate_->InstantPageLoadFailed(contents());
}

void InstantPage::OnSetSuggestions(
    int page_id,
    const std::vector<InstantSuggestion>& suggestions) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessSetSuggestions())
      delegate_->SetSuggestions(contents(), suggestions);
  }
}

void InstantPage::OnInstantSupportDetermined(int page_id,
                                             bool supports_instant) {
  if (!contents()->IsActiveEntry(page_id) || supports_instant_) {
    // Nothing to do if the page already supports Instant.
    return;
  }

  supports_instant_ = supports_instant;
  delegate_->InstantSupportDetermined(contents(), supports_instant);

  // If the page doesn't support Instant, stop listening to it.
  if (!supports_instant)
    Observe(NULL);
}

void InstantPage::OnShowInstantOverlay(int page_id,
                                       int height,
                                       InstantSizeUnits units) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessShowInstantOverlay())
      delegate_->ShowInstantOverlay(contents(), height, units);
  }
}

void InstantPage::OnFocusOmnibox(int page_id, OmniboxFocusState state) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessFocusOmnibox())
      delegate_->FocusOmnibox(contents(), state);
  }
}

void InstantPage::OnSearchBoxNavigate(int page_id,
                                      const GURL& url,
                                      content::PageTransition transition,
                                      WindowOpenDisposition disposition) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessNavigateToURL())
      delegate_->NavigateToURL(contents(), url, transition, disposition);
  }
}

void InstantPage::OnDeleteMostVisitedItem(InstantRestrictedID restricted_id) {
  delegate_->DeleteMostVisitedItem(restricted_id);
}

void InstantPage::OnUndoMostVisitedDeletion(InstantRestrictedID restricted_id) {
  delegate_->UndoMostVisitedDeletion(restricted_id);
}

void InstantPage::OnUndoAllMostVisitedDeletions() {
  delegate_->UndoAllMostVisitedDeletions();
}
