// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_
#define CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/command_updater.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class AutocompleteEditViewGtk;
class BackForwardMenuModel;
class BrowserView;

namespace views {
class ImageButton;
class ImageView;
class NativeViewHost;
}

namespace chromeos {

// This class provides a small navigation bar that includes back, forward, and
// a small text entry box.
class CompactNavigationBar : public views::View,
                             public views::ButtonListener,
                             public AutocompleteEditController,
                             public BubblePositioner,
                             public CommandUpdater::CommandObserver {
 public:
  explicit CompactNavigationBar(BrowserView* browser_view);
  virtual ~CompactNavigationBar();

  // Must be called before anything else, but after adding this view to the
  // widget.
  void Init();

  // Set focus to the location entry in the compact navigation bar.
  void FocusLocation();

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Focus();

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

  // BubblePositioner implementation.
  virtual gfx::Rect GetLocationStackBounds() const;

  // CommandUpdater::CommandObserver implementation.
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // Add new tab for the given url. The location of new tab is
  // controlled by the method |StatusAreaView::GetOpenTabsMode()|.
  void AddTabWithURL(const GURL& url, PageTransition::Type transition);

  BrowserView* browser_view_;

  bool initialized_;

  views::ImageButton* back_;
  views::ImageView* bf_separator_;
  views::ImageButton* forward_;

  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
  views::NativeViewHost* location_entry_view_;

  // History menu for back and forward buttons.
  scoped_ptr<BackForwardMenuModel> back_menu_model_;
  scoped_ptr<BackForwardMenuModel> forward_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(CompactNavigationBar);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_COMPACT_NAVIGATION_BAR_H_
