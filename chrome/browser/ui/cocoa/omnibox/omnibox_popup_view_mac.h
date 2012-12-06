// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "ui/gfx/font.h"

@class NSImage;
class OmniboxEditModel;
class OmniboxPopupModel;
class OmniboxView;

// Implements OmniboxPopupView using a raw NSWindow containing an
// NSTableView.
//
// TODO(rohitrao): This class is set up in a way that makes testing hard.
// Refactor and write unittests.  http://crbug.com/9977

class OmniboxPopupViewMac : public OmniboxPopupView {
 public:
  static OmniboxPopupView* Create(OmniboxView* omnibox_view,
                                  OmniboxEditModel* edit_model,
                                  NSTextField* field);

  OmniboxPopupViewMac(OmniboxView* omnibox_view,
                      OmniboxEditModel* edit_model,
                      NSTextField* field);
  virtual ~OmniboxPopupViewMac();

  // Overridden from OmniboxPopupView:
  virtual bool IsOpen() const OVERRIDE;
  virtual void InvalidateLine(size_t line) OVERRIDE {
    // TODO(shess): Verify that there is no need to implement this.
    // This is currently used in two places in the model:
    //
    // When setting the selected line, the selected line is
    // invalidated, then the selected line is changed, then the new
    // selected line is invalidated, then PaintUpdatesNow() is called.
    // For us PaintUpdatesNow() should be sufficient.
    //
    // Same thing happens when changing the hovered line, except with
    // no call to PaintUpdatesNow().  Since this code does not
    // currently support special display of the hovered line, there's
    // nothing to do here.
    //
    // deanm indicates that this is an anti-flicker optimization,
    // which we probably cannot utilize (and may not need) so long as
    // we're using NSTableView to implement the popup contents.  We
    // may need to move away from NSTableView to implement hover,
    // though.
  }
  virtual void UpdatePopupAppearance() OVERRIDE;

  virtual gfx::Rect GetTargetBounds() OVERRIDE;

  // Set |line| to be selected.
  void SetSelectedLine(size_t line);

  // This is only called by model in SetSelectedLine() after updating
  // everything.  Popup should already be visible.
  virtual void PaintUpdatesNow() OVERRIDE;

  virtual void OnDragCanceled() OVERRIDE {}

  // Opens the URL corresponding to the given |row|.  If |force_background| is
  // true, forces the URL to open in a background tab.  Otherwise, determines
  // the proper window open disposition from the modifier flags on |[NSApp
  // currentEvent]|.
  void OpenURLForRow(int row, bool force_background);

  // Return the text to show for the match, based on the match's
  // contents and description.  Result will be in |font|, with the
  // boldfaced version used for matches.
  static NSAttributedString* MatchText(const AutocompleteMatch& match,
                                       gfx::Font& font,
                                       float cellWidth);

  // Helper for MatchText() to allow sharing code between the contents
  // and description cases.  Returns NSMutableAttributedString as a
  // convenience for MatchText().
  static NSMutableAttributedString* DecorateMatchedString(
      const string16 &matchString,
      const AutocompleteMatch::ACMatchClassifications &classifications,
      NSColor* textColor, NSColor* dimTextColor, gfx::Font& font);

  // Helper for MatchText() to elide a marked-up string using
  // gfx::ElideText() as a model.  Modifies |aString| in place.
  // TODO(shess): Consider breaking AutocompleteButtonCell out of this
  // code, and modifying it to have something like -setMatch:, so that
  // these convolutions to expose internals for testing can be
  // cleaner.
  static NSMutableAttributedString* ElideString(
      NSMutableAttributedString* aString,
      const string16 originalString,
      const gfx::Font& font,
      const float cellWidth);

 private:
  // Create the popup_ instance if needed.
  void CreatePopupIfNeeded();

  // Calculate the appropriate position for the popup based on the
  // field's screen position and the given target for the matrix
  // height, and makes the popup visible.  Animates to the new frame
  // if the popup shrinks, snaps to the new frame if the popup grows,
  // allows existing animations to continue if the size doesn't
  // change.
  void PositionPopup(const CGFloat matrixHeight);

  // Returns the NSImage that should be used as an icon for the given match.
  NSImage* ImageForMatch(const AutocompleteMatch& match);

  // TODO(shess): |omnibox_view_| should already be accessible via
  // |field_|, or perhaps via |model_|.  Consider refactoring.
  OmniboxView* omnibox_view_;
  scoped_ptr<OmniboxPopupModel> model_;
  NSTextField* field_;  // owned by tab controller

  // Child window containing a matrix which implements the popup.
  scoped_nsobject<NSWindow> popup_;
  NSRect targetPopupFrame_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_VIEW_MAC_H_
