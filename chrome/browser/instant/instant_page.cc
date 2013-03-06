// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_page.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"

InstantPage::Delegate::~Delegate() {
}

InstantPage::~InstantPage() {
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
  // TODO(sail) Remove this once the Mac omnibox font size is updated.
#if defined(OS_MACOSX)
  ui::ResourceBundle::FontStyle font_style = ui::ResourceBundle::BaseFont;
#else
  ui::ResourceBundle::FontStyle font_style = ui::ResourceBundle::MediumFont;
#endif
  const gfx::Font& omnibox_font =
      ui::ResourceBundle::GetSharedInstance().GetFont(font_style);
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

void InstantPage::CancelSelection(const string16& user_text) {
  Send(new ChromeViewMsg_SearchBoxCancelSelection(routing_id(), user_text));
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
    const std::vector<MostVisitedItem>& items) {
  Send(new ChromeViewMsg_InstantMostVisitedItemsChanged(routing_id(), items));
}

InstantPage::InstantPage(Delegate* delegate)
    : delegate_(delegate),
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

bool InstantPage::ShouldProcessStartCapturingKeyStrokes() {
  return false;
}

bool InstantPage::ShouldProcessStopCapturingKeyStrokes() {
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
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_StartCapturingKeyStrokes,
                        OnStartCapturingKeyStrokes);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_StopCapturingKeyStrokes,
                        OnStopCapturingKeyStrokes);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxNavigate,
                        OnSearchBoxNavigate);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantDeleteMostVisitedItem,
                        OnDeleteMostVisitedItem);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantUndoMostVisitedDeletion,
                        OnUndoMostVisitedDeletion);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantUndoAllMostVisitedDeletions,
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
                                       InstantShownReason reason,
                                       int height,
                                       InstantSizeUnits units) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessShowInstantOverlay())
      delegate_->ShowInstantOverlay(contents(), reason, height, units);
  }
}

void InstantPage::OnStartCapturingKeyStrokes(int page_id) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessStartCapturingKeyStrokes())
      delegate_->StartCapturingKeyStrokes(contents());
  }
}

void InstantPage::OnStopCapturingKeyStrokes(int page_id) {
  if (contents()->IsActiveEntry(page_id)) {
    OnInstantSupportDetermined(page_id, true);
    if (ShouldProcessStopCapturingKeyStrokes())
      delegate_->StopCapturingKeyStrokes(contents());
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

void InstantPage::OnDeleteMostVisitedItem(const GURL& url) {
  delegate_->DeleteMostVisitedItem(url);
}

void InstantPage::OnUndoMostVisitedDeletion(const GURL& url) {
  delegate_->UndoMostVisitedDeletion(url);
}

void InstantPage::OnUndoAllMostVisitedDeletions() {
  delegate_->UndoAllMostVisitedDeletions();
}
