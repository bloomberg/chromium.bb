// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_H_

#include "base/basictypes.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/ui/views/compact_nav/compact_location_bar_view_host.h"
#include "chrome/browser/ui/views/dropdown_bar_view.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class Browser;
class BrowserView;
class CompactLocationBarViewHost;
class LocationBarView;
class ReloadButton;
class TabContents;

namespace views {
class ImageButton;
class NativeViewHost;
}  // namespace views

// CompactLocationBarView is a version of location bar that is shown under a tab
// for short period of time when Chrome is in the compact navigation bar mode.
class CompactLocationBarView : public DropdownBarView,
                               public views::ButtonListener,
                               public AutocompleteEditController {
 public:
  explicit CompactLocationBarView(CompactLocationBarViewHost* host);
  virtual ~CompactLocationBarView();

  // Claims focus for the text field and optionally selects its contents.
  virtual void SetFocusAndSelection(bool select_all);

  // Update the contained location bar to |contents|.
  void Update(const TabContents* contents);

  // Overridden from AccessiblePaneView
  virtual bool SetPaneFocus(int view_storage_id, View* initial_focus) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  LocationBarView* location_bar_view() { return location_bar_view_; }
  ReloadButton* reload_button() { return reload_button_; }

 private:
  Browser* browser() const;

  // Called when the view is added to the tree to initialize the
  // CompactLocationBarView.
  void Init();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout() OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child) OVERRIDE;

  // No focus border for the location bar, the caret is enough.
  virtual void Focus();
  virtual void PaintFocusBorder(gfx::Canvas* canvas) { }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // AutocompleteEditController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url) OVERRIDE;
  virtual void OnChanged() OVERRIDE;
  virtual void OnSelectionBoundsChanged() OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual SkBitmap GetFavicon() const OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE;

  CompactLocationBarViewHost* clb_host() {
    return static_cast<CompactLocationBarViewHost*>(host());
  }

  ReloadButton* reload_button_;
  LocationBarView* location_bar_view_;
  // TODO(stevet): Add the BrowserActionsContainer back.

  // Font used by edit and some of the hints.
  gfx::Font font_;

  // True if we have already been initialized.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(CompactLocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_LOCATION_BAR_VIEW_H_
