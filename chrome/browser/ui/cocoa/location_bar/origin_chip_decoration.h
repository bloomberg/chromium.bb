// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ORIGIN_CHIP_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ORIGIN_CHIP_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/cocoa/location_bar/button_decoration.h"
#include "chrome/browser/ui/location_bar/origin_chip_info.h"
#include "extensions/browser/extension_icon_image.h"

class LocationBarViewMac;
class LocationIconDecoration;

namespace content {
class WebContents;
}

// Origin chip button, which is placed leading the omnibox and contains the
// current site's host. Clicking the chip reveals the page's URL, and clicking
// the icon on the chip reveals the permissions bubble.
class OriginChipDecoration : public ButtonDecoration,
                             public extensions::IconImage::Observer,
                             public SafeBrowsingUIManager::Observer {
 public:
  OriginChipDecoration(LocationBarViewMac* owner,
                       LocationIconDecoration* location_icon);
  virtual ~OriginChipDecoration();

  // Updates the origin chip's content, and display state.
  void Update();

  // Implement |ButtonDecoration|.
  virtual bool PreventFocus(NSPoint location) const OVERRIDE;

  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

  // Implement |IconImage::Observer|.
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

  // Implement |SafeBrowsingUIManager::Observer|.
  virtual void OnSafeBrowsingHit(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;
  virtual void OnSafeBrowsingMatch(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;

 private:
  // Returns the width required to display the chip's contents.
  CGFloat GetChipWidth() const;

  // Contains attributes for drawing the origin string.
  base::scoped_nsobject<NSMutableDictionary> attributes_;

  // The extension's current icon, if the page being displayed belongs to an
  // extension.
  base::scoped_nsobject<NSImage> extension_icon_;

  // The rectangle where the icon was last drawn. Used for hit testing to
  // display the permissions bubble.
  NSRect icon_rect_;

  // Manages information to be displayed on the origin chip.
  OriginChipInfo info_;

  // The label currently displayed in the chip.
  base::scoped_nsobject<NSString> label_;

  // The location icon decoration. Weak.
  LocationIconDecoration* location_icon_;

  // The control view that owns this. Weak.
  LocationBarViewMac* owner_;

  DISALLOW_COPY_AND_ASSIGN(OriginChipDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ORIGIN_CHIP_DECORATION_H_
