// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_tab.h"

#include "chrome/browser/instant/instant_controller.h"

InstantTab::InstantTab(InstantController* controller,
                       content::WebContents* contents)
    : client_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      controller_(controller),
      contents_(contents),
      supports_instant_(false) {
}

InstantTab::~InstantTab() {
}

void InstantTab::Init() {
  client_.SetContents(contents_);
  client_.DetermineIfPageSupportsInstant();
}

void InstantTab::Update(const string16& text,
                        size_t selection_start,
                        size_t selection_end,
                        bool verbatim) {
  client_.Update(text, selection_start, selection_end, verbatim);
}

void InstantTab::Submit(const string16& text) {
  client_.Submit(text);
}

void InstantTab::SendAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  client_.SendAutocompleteResults(results);
}

void InstantTab::SetDisplayInstantResults(bool display_instant_results) {
  client_.SetDisplayInstantResults(display_instant_results);
}

void InstantTab::UpOrDownKeyPressed(int count) {
  client_.UpOrDownKeyPressed(count);
}

void InstantTab::SetSuggestions(
    const std::vector<InstantSuggestion>& suggestions) {
  InstantSupportDetermined(true);
  controller_->SetSuggestions(contents_, suggestions);
}

void InstantTab::InstantSupportDetermined(bool supports_instant) {
  // If we had already determined that the page supports Instant, nothing to do.
  if (supports_instant_)
    return;

  supports_instant_ = supports_instant;

  // If the page doesn't support Instant, stop communicating with it.
  if (!supports_instant)
    client_.SetContents(NULL);

  controller_->InstantSupportDetermined(contents_, supports_instant);
}

void InstantTab::ShowInstantPreview(InstantShownReason /* reason */,
                                    int /* height */,
                                    InstantSizeUnits /* units */) {
  // The page is a committed tab (i.e., always showing), so nothing to do.
}

void InstantTab::StartCapturingKeyStrokes() {
  // We don't honor this call from committed tabs.
}

void InstantTab::StopCapturingKeyStrokes() {
  // We don't honor this call from committed tabs.
}

void InstantTab::RenderViewGone() {
  // For a commit page, a crash should not be handled differently.
}

void InstantTab::AboutToNavigateMainFrame(const GURL& url) {
  // The client is a committed tab, navigations will happen as expected.
}

void InstantTab::NavigateToURL(const GURL& url,
                               content::PageTransition transition) {
  controller_->NavigateToURL(url, transition);
}
