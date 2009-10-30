// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_H_
#define CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_H_

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class AutocompleteEditViewGtk;
class Browser;
class BrowserView;
class ToolbarStarToggleGtk;
class Tab;
class TabContents;
class TabStrip;

namespace views {
class ImageButton;
class NativeViewHost;
}  // namespace views

namespace chromeos {

// CompactLocationBar is a version of location bar that is shown under
// a tab for short priod of used when Chrome is in the compact
// navigation bar mode.
// TODO(oshima): re-implement w/o using a popup, like a FindBar.
class CompactLocationBar : public views::View,
                           public views::ButtonListener,
                           public AutocompleteEditController,
                           public BubblePositioner {
 public:
  explicit CompactLocationBar(BrowserView* browser_view);
  ~CompactLocationBar();

  // Returns the bounds to locale the compact location bar under the tab.
  gfx::Rect GetBoundsUnderTab(const Tab* tab) const;

  // (Re)Starts the popup timer that hides the popup after X seconds.
  void StartPopupTimer();

  // Updates the content and the location of the compact location bar.
  void Update(const Tab* tab, const TabContents* contents);

  // Updates the location of the location bar popup under the given tab.
  void UpdateBounds(const Tab* tab);

 private:
  Browser* browser() const;

  // Cancels the popup timer.
  void CancelPopupTimer();

  // Hides the popup window.
  void HidePopup();

  // Called when the view is added to the tree to initialize the
  // CompactLocationBar.
  void Init();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // AutocompleteEditController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnKillFocus() {}
  virtual void OnSetFocus() {}
  virtual void OnInputInProgress(bool in_progress);
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // BubblePositioner implementation.
  virtual gfx::Rect GetLocationStackBounds() const;

  BrowserView* browser_view_;
  const TabContents* current_contents_;

  views::ImageButton* reload_;
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
  views::NativeViewHost* location_entry_view_;

  // scoped_ptr<ToolbarStarToggleGtk> star_;
  views::NativeViewHost* star_view_;

  scoped_ptr<base::OneShotTimer<CompactLocationBar> > popup_timer_;

  // A popup window to show the compact location bar.
  views::Widget* popup_;

  DISALLOW_COPY_AND_ASSIGN(CompactLocationBar);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_H_

