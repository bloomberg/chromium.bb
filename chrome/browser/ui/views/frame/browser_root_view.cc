// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include <cmath>

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"

// static
const char BrowserRootView::kViewClassName[] =
    "browser/ui/views/frame/BrowserRootView";

BrowserRootView::BrowserRootView(BrowserView* browser_view,
                                 views::Widget* widget)
    : views::internal::RootView(widget),
      browser_view_(browser_view),
      forwarding_to_tab_strip_(false),
      scroll_remainder_x_(0),
      scroll_remainder_y_(0) {}

bool BrowserRootView::GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) {
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
  if (data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES))
    return true;

  // If there isn't a URL, see if we can 'paste and go'.
  return GetPasteAndGoURL(data, nullptr);
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
  base::string16 title;
  ui::OSExchangeData mapped_data;
  if (!event.data().GetURLAndTitle(
           ui::OSExchangeData::CONVERT_FILENAMES, &url, &title) ||
      !url.is_valid()) {
    // The url isn't valid. Use the paste and go url.
    if (GetPasteAndGoURL(event.data(), &url))
      mapped_data.SetURL(url, base::string16());
    // else case: couldn't extract a url or 'paste and go' url. This ends up
    // passing through an ui::OSExchangeData with nothing in it. We need to do
    // this so that the tab strip cleans up properly.
  } else {
    mapped_data.SetURL(url, base::string16());
  }
  forwarding_to_tab_strip_ = false;
  scoped_ptr<ui::DropTargetEvent> mapped_event(
      MapEventToTabStrip(event, mapped_data));
  return tabstrip()->OnPerformDrop(*mapped_event);
}

const char* BrowserRootView::GetClassName() const {
  return kViewClassName;
}

bool BrowserRootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (browser_defaults::kScrollEventChangesTab) {
    // Switch to the left/right tab if the wheel-scroll happens over the
    // tabstrip, or the empty space beside the tabstrip.
    views::View* hit_view = GetEventHandlerForPoint(event.location());
    int hittest =
        GetWidget()->non_client_view()->NonClientHitTest(event.location());
    if (tabstrip()->Contains(hit_view) ||
        hittest == HTCAPTION ||
        hittest == HTTOP) {
      scroll_remainder_x_ += event.x_offset();
      scroll_remainder_y_ += event.y_offset();

      // Number of integer scroll events that have passed in each direction.
      int whole_scroll_amount_x =
          std::lround(static_cast<double>(scroll_remainder_x_) /
                      ui::MouseWheelEvent::kWheelDelta);
      int whole_scroll_amount_y =
          std::lround(static_cast<double>(scroll_remainder_y_) /
                      ui::MouseWheelEvent::kWheelDelta);

      // Adjust the remainder such that any whole scrolls we have taken action
      // for don't count towards the scroll remainder.
      scroll_remainder_x_ -=
          whole_scroll_amount_x * ui::MouseWheelEvent::kWheelDelta;
      scroll_remainder_y_ -=
          whole_scroll_amount_y * ui::MouseWheelEvent::kWheelDelta;

      // Count a scroll in either axis - summing the axes works for this.
      int whole_scroll_offset = whole_scroll_amount_x + whole_scroll_amount_y;

      Browser* browser = browser_view_->browser();
      TabStripModel* model = browser->tab_strip_model();
      // Switch to the next tab only if not at the end of the tab-strip.
      if (whole_scroll_offset < 0 &&
          model->active_index() + 1 < model->count()) {
        chrome::SelectNextTab(browser);
        return true;
      }

      // Switch to the previous tab only if not at the beginning of the
      // tab-strip.
      if (whole_scroll_offset > 0 && model->active_index() > 0) {
        chrome::SelectPreviousTab(browser);
        return true;
      }
    }
  }
  return RootView::OnMouseWheel(event);
}

void BrowserRootView::OnMouseExited(const ui::MouseEvent& event) {
  // Reset the remainders so tab switches occur halfway through a smooth scroll.
  scroll_remainder_x_ = 0;
  scroll_remainder_y_ = 0;
  RootView::OnMouseExited(event);
}

void BrowserRootView::OnEventProcessingStarted(ui::Event* event) {
  if (event->IsGestureEvent()) {
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    if (gesture_event->type() == ui::ET_GESTURE_TAP &&
        gesture_event->location().y() <= 0 &&
        gesture_event->location().x() <= browser_view_->GetBounds().width()) {
      TouchUMA::RecordGestureAction(TouchUMA::GESTURE_ROOTVIEWTOP_TAP);
    }
  }

  RootView::OnEventProcessingStarted(event);
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

  base::string16 text;
  if (!data.GetString(&text) || text.empty())
    return false;
  text = AutocompleteMatch::SanitizeString(text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(
      browser_view_->browser()->profile())->Classify(
          text, false, false, metrics::OmniboxEventProto::INVALID_SPEC, &match,
          nullptr);
  if (!match.destination_url.is_valid())
    return false;

  if (url)
    *url = match.destination_url;
  return true;
}
