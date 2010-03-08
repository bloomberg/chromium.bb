// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_VIEW_H_

#include "base/basictypes.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/views/dropdown_bar_view.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class AutocompleteEditViewGtk;
class Browser;
class BrowserActionsContainer;
class BrowserView;
class ToolbarStarToggleGtk;
class Tab;
class TabContents;
class TabStrip;
class ToolbarStarToggle;

namespace views {
class ImageButton;
class NativeViewHost;
}  // namespace views

namespace chromeos {

// CompactLocationBarView is a version of location bar that is shown under
// a tab for short priod of used when Chrome is in the compact
// navigation bar mode.
class CompactLocationBarView : public DropdownBarView,
                               public views::ButtonListener,
                               public AutocompleteEditController,
                               public BubblePositioner,
                               public views::DragController {
 public:
  explicit CompactLocationBarView(CompactLocationBarHost* host);
  ~CompactLocationBarView();

  // Claims focus for the text field and selects its contents.
  virtual void SetFocusAndSelection();

  void Update(const TabContents* contents);

  ToolbarStarToggle* star_button() const { return star_; }

 private:
  Browser* browser() const;

  // Called when the view is added to the tree to initialize the
  // CompactLocationBarView.
  void Init();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);
  virtual void Focus();
  // No focus border for the location bar, the caret is enough.
  virtual void PaintFocusBorder(gfx::Canvas* canvas) { }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // AutocompleteEditController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnKillFocus();
  virtual void OnSetFocus();
  virtual void OnInputInProgress(bool in_progress);
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // BubblePositioner implementation.
  virtual gfx::Rect GetLocationStackBounds() const;

  // views::DragController implementation.
  virtual void WriteDragData(View* sender,
                             const gfx::Point& press_pt,
                             OSExchangeData* data);
  virtual int GetDragOperations(View* sender, const gfx::Point& p);
  virtual bool CanStartDrag(View* sender,
                            const gfx::Point& press_pt,
                            const gfx::Point& p) {
    return true;
  }

  CompactLocationBarHost* clb_host() {
    return static_cast<CompactLocationBarHost*>(host());
  }

  views::ImageButton* reload_;
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
  views::NativeViewHost* location_entry_view_;
  BrowserActionsContainer* browser_actions_;
  ToolbarStarToggle* star_;

  DISALLOW_COPY_AND_ASSIGN(CompactLocationBarView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_COMPACT_LOCATION_BAR_VIEW_H_
