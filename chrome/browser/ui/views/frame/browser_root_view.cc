// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include "base/auto_reset.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "grit/chromium_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"

// static
const char BrowserRootView::kViewClassName[] =
    "browser/ui/views/frame/BrowserRootView";

BrowserRootView::BrowserRootView(BrowserView* browser_view,
                                 views::Widget* widget)
    : views::internal::RootView(widget),
      browser_view_(browser_view),
      scheduling_immersive_reveal_painting_(false),
      forwarding_to_tab_strip_(false) { }

bool BrowserRootView::GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  if (tabstrip() && tabstrip()->visible()) {
    *formats = ui::OSExchangeData::URL | ui::OSExchangeData::STRING;
    return true;
  }
  return false;
}

bool BrowserRootView::AreDropTypesRequired() {
  return true;
}

bool BrowserRootView::CanDrop(const ui::OSExchangeData& data) {
  if (!tabstrip() || !tabstrip()->visible())
    return false;

  // If there is a URL, we'll allow the drop.
  if (data.HasURL())
    return true;

  // If there isn't a URL, see if we can 'paste and go'.
  return GetPasteAndGoURL(data, NULL);
}

void BrowserRootView::OnDragEntered(const ui::DropTargetEvent& event) {
  if (ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<ui::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event, event.data()));
    tabstrip()->OnDragEntered(*mapped_event.get());
  }
}

int BrowserRootView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (ShouldForwardToTabStrip(event)) {
    scoped_ptr<ui::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event, event.data()));
    if (!forwarding_to_tab_strip_) {
      tabstrip()->OnDragEntered(*mapped_event.get());
      forwarding_to_tab_strip_ = true;
    }
    return tabstrip()->OnDragUpdated(*mapped_event.get());
  } else if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip()->OnDragExited();
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void BrowserRootView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip()->OnDragExited();
  }
}

int BrowserRootView::OnPerformDrop(const ui::DropTargetEvent& event) {
  if (!forwarding_to_tab_strip_)
    return ui::DragDropTypes::DRAG_NONE;

  // Extract the URL and create a new ui::OSExchangeData containing the URL. We
  // do this as the TabStrip doesn't know about the autocomplete edit and needs
  // to know about it to handle 'paste and go'.
  GURL url;
  string16 title;
  ui::OSExchangeData mapped_data;
  if (!event.data().GetURLAndTitle(&url, &title) || !url.is_valid()) {
    // The url isn't valid. Use the paste and go url.
    if (GetPasteAndGoURL(event.data(), &url))
      mapped_data.SetURL(url, string16());
    // else case: couldn't extract a url or 'paste and go' url. This ends up
    // passing through an ui::OSExchangeData with nothing in it. We need to do
    // this so that the tab strip cleans up properly.
  } else {
    mapped_data.SetURL(url, string16());
  }
  forwarding_to_tab_strip_ = false;
  scoped_ptr<ui::DropTargetEvent> mapped_event(
      MapEventToTabStrip(event, mapped_data));
  return tabstrip()->OnPerformDrop(*mapped_event);
}

void BrowserRootView::GetAccessibleState(ui::AccessibleViewState* state) {
  views::internal::RootView::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

std::string BrowserRootView::GetClassName() const {
  return kViewClassName;
}

void BrowserRootView::SchedulePaintInRect(const gfx::Rect& rect) {
  views::internal::RootView::SchedulePaintInRect(rect);

  // This function becomes reentrant when redirecting a paint-request to the
  // reveal-view in immersive mode (because paint-requests all bubble up to the
  // root-view). So return early in such cases.
  if (scheduling_immersive_reveal_painting_)
    return;

  if (browser_view_ &&
      browser_view_->immersive_mode_controller() &&
      browser_view_->immersive_mode_controller()->IsRevealed()) {
    views::View* reveal =
        browser_view_->immersive_mode_controller()->reveal_view();
    if (reveal) {
      base::AutoReset<bool> reset(&scheduling_immersive_reveal_painting_, true);
      reveal->SchedulePaintInRect(rect);
    }
  }
}

bool BrowserRootView::ShouldForwardToTabStrip(
    const ui::DropTargetEvent& event) {
  if (!tabstrip()->visible())
    return false;

  // Allow the drop as long as the mouse is over the tabstrip or vertically
  // before it.
  gfx::Point tab_loc_in_host;
  ConvertPointToTarget(tabstrip(), this, &tab_loc_in_host);
  return event.y() < tab_loc_in_host.y() + tabstrip()->height();
}

ui::DropTargetEvent* BrowserRootView::MapEventToTabStrip(
    const ui::DropTargetEvent& event,
    const ui::OSExchangeData& data) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToTarget(this, tabstrip(), &tab_strip_loc);
  return new ui::DropTargetEvent(data, tab_strip_loc, tab_strip_loc,
                                 event.source_operations());
}

TabStrip* BrowserRootView::tabstrip() const {
  return browser_view_->tabstrip();
}

bool BrowserRootView::GetPasteAndGoURL(const ui::OSExchangeData& data,
                                       GURL* url) {
  if (!data.HasString())
    return false;

  string16 text;
  if (!data.GetString(&text) || text.empty())
    return false;
  text = AutocompleteMatch::SanitizeString(text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(
      browser_view_->browser()->profile())->Classify(text, string16(), false,
                                                     false, &match, NULL);
  if (!match.destination_url.is_valid())
    return false;

  if (url)
    *url = match.destination_url;
  return true;
}
