// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_
#define CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/bubble_positioner.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class AutocompleteEditViewGtk;
class Browser;

namespace views {
class ImageButton;
class ImageView;
class NativeViewHost;
}

// This class provides a small navigation bar that includes back, forward, and
// a small text entry box.
class CompactNavigationBar : public views::View,
                             public views::ButtonListener,
                             public AutocompleteEditController,
                             public BubblePositioner {
 public:
  explicit CompactNavigationBar(Browser* browser);
  virtual ~CompactNavigationBar();

  // Must be called before anything else, but after adding this view to the
  // widget.
  void Init();

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

 private:
  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // AutocompleteController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual void OnKillFocus();
  virtual void OnSetFocus();
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // BubblePositioner:
  virtual gfx::Rect GetLocationStackBounds() const;

  void AddTabWithURL(const GURL& url, PageTransition::Type transition);

  Browser* browser_;

  bool initialized_;

  views::ImageButton* back_button_;
  views::ImageView* bf_separator_;
  views::ImageButton* forward_button_;

  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
  views::NativeViewHost* location_entry_view_;

  DISALLOW_COPY_AND_ASSIGN(CompactNavigationBar);
};

#endif  // CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_
