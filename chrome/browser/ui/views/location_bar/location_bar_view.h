// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/drag_controller.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#elif defined(OS_WIN)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/omnibox/omnibox_view_gtk.h"
#endif

class Browser;
class ContentSettingImageView;
class EVBubbleView;
class ExtensionAction;
class GURL;
class InstantController;
class KeywordHintView;
class LocationIconView;
class PageActionWithBadgeView;
class SelectedKeywordView;
class StarView;
class TabContents;
class TabContentsWrapper;
class TemplateURLService;

#if defined(OS_WIN) || defined(USE_AURA)
class SuggestedTextView;
#endif

/////////////////////////////////////////////////////////////////////////////
//
// LocationBarView class
//
//   The LocationBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
/////////////////////////////////////////////////////////////////////////////
class LocationBarView : public LocationBar,
                        public LocationBarTesting,
                        public views::View,
                        public views::DragController,
                        public AutocompleteEditController,
                        public DropdownBarHostDelegate,
                        public TemplateURLServiceObserver,
                        public content::NotificationObserver {
 public:
  // The location bar view's class name.
  static const char kViewClassName[];

  // DropdownBarHostDelegate
  virtual void SetFocusAndSelection(bool select_all) OVERRIDE;
  virtual void SetAnimationOffset(int offset) OVERRIDE;

  // Returns the offset used while animating.
  int animation_offset() const { return animation_offset_; }

  class Delegate {
   public:
    // Should return the current tab contents.
    virtual TabContentsWrapper* GetTabContentsWrapper() const = 0;

    // Returns the InstantController, or NULL if there isn't one.
    virtual InstantController* GetInstant() = 0;

    // Called by the location bar view when the user starts typing in the edit.
    // This forces our security style to be UNKNOWN for the duration of the
    // editing.
    virtual void OnInputInProgress(bool in_progress) = 0;
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    SELECTED_TEXT,
    DEEMPHASIZED_TEXT,
    SECURITY_TEXT,
  };

  // The modes reflect the different scenarios where a location bar can be used.
  // The normal mode is the mode used in a regular browser window.
  // In popup mode, the location bar view is read only and has a slightly
  // different presentation (font size / color).
  // In app launcher mode, the location bar is empty and no security states or
  // page/browser actions are displayed.
  enum Mode {
    NORMAL = 0,
    POPUP,
    APP_LAUNCHER
  };

  LocationBarView(Browser* browser,
                  ToolbarModel* model,
                  Delegate* delegate,
                  Mode mode);
  virtual ~LocationBarView();

  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns the appropriate color for the desired kind, based on the user's
  // system theme.
  static SkColor GetColor(ToolbarModel::SecurityLevel security_level,
                          ColorKind kind);

  // Updates the location bar.  We also reset the bar's permanent text and
  // security style, and, if |tab_for_state_restoring| is non-NULL, also restore
  // saved state that the tab holds.
  void Update(const TabContents* tab_for_state_restoring);

  Browser* browser() const { return browser_; }

  // Sets |preview_enabled| for the PageAction View associated with this
  // |page_action|. If |preview_enabled| is true, the view will display the
  // PageActions icon even though it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction *page_action,
                                   bool preview_enabled);

  // Retrieves the PageAction View which is associated with |page_action|.
  views::View* GetPageActionView(ExtensionAction* page_action);

  // Toggles the star on or off.
  void SetStarToggled(bool on);

  // Shows the bookmark bubble.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  // Returns the screen coordinates of the location entry (where the URL text
  // appears, not where the icons are shown).
  gfx::Point GetLocationEntryOrigin() const;

#if defined(OS_WIN) || defined(USE_AURA)
  // Invoked from OmniboxViewWin to show the instant suggestion.
  void SetInstantSuggestion(const string16& text,
                            bool animate_to_complete);

  // Returns the current instant suggestion text.
  string16 GetInstantSuggestion() const;
#endif

  // Sizing functions
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Layout and Painting functions
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // No focus border for the location bar, the caret is enough.
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE { }

  // Set if we should show a focus rect while the location entry field is
  // focused. Used when the toolbar is in full keyboard accessibility mode.
  // Repaints if necessary.
  virtual void SetShowFocusRect(bool show);

  // Select all of the text. Needed when the user tabs through controls
  // in the toolbar in full keyboard accessibility mode.
  virtual void SelectAll();

  const gfx::Font& font() const { return font_; }

#if defined(OS_WIN) && !defined(USE_AURA)
  // Event Handlers
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
#endif

  LocationIconView* location_icon_view() { return location_icon_view_; }
  const LocationIconView* location_icon_view() const {
    return location_icon_view_;
  }

  views::View* location_entry_view() const { return location_entry_view_; }

  // AutocompleteEditController
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    content::PageTransition transition,
                                    const GURL& alternate_nav_url) OVERRIDE;
  virtual void OnChanged() OVERRIDE;
  virtual void OnSelectionBoundsChanged() OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual SkBitmap GetFavicon() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE;

  // Overridden from views::View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& event)
      OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(FirstRun::BubbleType bubble_type) OVERRIDE;
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) OVERRIDE;
  virtual string16 GetInputString() const OVERRIDE;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const OVERRIDE;
  virtual content::PageTransition GetPageTransition() const OVERRIDE;
  virtual void AcceptInput() OVERRIDE;
  virtual void FocusLocation(bool select_all) OVERRIDE;
  virtual void FocusSearch() OVERRIDE;
  virtual void UpdateContentSettingsIcons() OVERRIDE;
  virtual void UpdatePageActions() OVERRIDE;
  virtual void InvalidatePageActions() OVERRIDE;
  virtual void SaveStateToContents(TabContents* contents) OVERRIDE;
  virtual void Revert() OVERRIDE;
  virtual const OmniboxView* location_entry() const OVERRIDE;
  virtual OmniboxView* location_entry() OVERRIDE;
  virtual LocationBarTesting* GetLocationBarForTesting() OVERRIDE;

  // Overridden from LocationBarTesting:
  virtual int PageActionCount() OVERRIDE;
  virtual int PageActionVisibleCount() OVERRIDE;
  virtual ExtensionAction* GetPageAction(size_t index) OVERRIDE;
  virtual ExtensionAction* GetVisiblePageAction(size_t index) OVERRIDE;
  virtual void TestPageActionPressed(size_t index) OVERRIDE;

  // Overridden from TemplateURLServiceObserver
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // Overridden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Thickness of the left and right edges of the omnibox, in normal mode.
  static const int kNormalHorizontalEdgeThickness;
  // Thickness of the top and bottom edges of the omnibox.
  static const int kVerticalEdgeThickness;
  // Space between items in the location bar.
  static const int kItemPadding;
  // Amount of padding built into the standard omnibox icons.
  static const int kIconInternalPadding;
  // Space between the edges and the items next to them.
  static const int kEdgeItemPadding;
  // Space between the edge and a bubble.
  static const int kBubbleHorizontalPadding;

 protected:
  virtual void OnFocus() OVERRIDE;

 private:
  typedef std::vector<ContentSettingImageView*> ContentSettingViews;

  friend class PageActionImageView;
  friend class PageActionWithBadgeView;
  typedef std::vector<PageActionWithBadgeView*> PageActionViews;

  // Returns the amount of horizontal space (in pixels) out of
  // |location_bar_width| that is not taken up by the actual text in
  // location_entry_.
  int AvailableWidth(int location_bar_width);

  // If |view| fits in |available_width|, it is made visible and positioned at
  // the leading or trailing end of |bounds|, which are then shrunk
  // appropriately.  Otherwise |view| is made invisible.
  // Note: |view| is expected to have already been positioned and sized
  // vertically.
  void LayoutView(views::View* view,
                  int padding,
                  int available_width,
                  bool leading,
                  gfx::Rect* bounds);

  // Update the visibility state of the Content Blocked icons to reflect what is
  // actually blocked on the current page.
  void RefreshContentSettingViews();

  // Delete all page action views that we have created.
  void DeletePageActionViews();

  // Update the views for the Page Actions, to reflect state changes for
  // PageActions.
  void RefreshPageActionViews();

  // Sets the visibility of view to new_vis.
  void ToggleVisibility(bool new_vis, views::View* view);

#if defined(OS_WIN) || defined(USE_AURA)
#if !defined(USE_AURA)
  // Helper for the Mouse event handlers that does all the real work.
  void OnMouseEvent(const views::MouseEvent& event, UINT msg);
#endif

  // Returns true if the suggest text is valid.
  bool HasValidSuggestText() const;

#if !defined(USE_AURA)
  // Returns |location_entry_| cast to OmniboxViewWin, or NULL if
  // |location_entry_| is of a different type.
  OmniboxViewWin* GetOmniboxViewWin();
#endif
#endif

  // Helper to show the first run info bubble.
  void ShowFirstRunBubbleInternal(FirstRun::BubbleType bubble_type);

  // The Autocomplete Edit field.
  scoped_ptr<OmniboxView> location_entry_;

  // The Browser object that corresponds to this View.
  Browser* browser_;

  // The model.
  ToolbarModel* model_;

  // Our delegate.
  Delegate* delegate_;

  // This is the string of text from the autocompletion session that the user
  // entered or selected.
  string16 location_input_;

  // The user's desired disposition for how their input should be opened
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation
  content::PageTransition transition_;

  // Font used by edit and some of the hints.
  gfx::Font font_;

  // An object used to paint the normal-mode background.
  scoped_ptr<views::Painter> painter_;

  // An icon to the left of the edit field.
  LocationIconView* location_icon_view_;

  // A bubble displayed for EV HTTPS sites.
  EVBubbleView* ev_bubble_view_;

  // Location_entry view
  views::View* location_entry_view_;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_.
  // These autocollapse when the edit needs the room.

  // Shown if the user has selected a keyword.
  SelectedKeywordView* selected_keyword_view_;

#if defined(OS_WIN) || defined(USE_AURA)
  // View responsible for showing suggested text. This is NULL when there is no
  // suggested text.
  SuggestedTextView* suggested_text_view_;
#endif

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView* keyword_hint_view_;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The page action icon views.
  PageActionViews page_action_views_;

  // The star.
  StarView* star_view_;

  // The mode that dictates how the bar shows.
  Mode mode_;

  // True if we should show a focus rect while the location entry field is
  // focused. Used when the toolbar is in full keyboard accessibility mode.
  bool show_focus_rect_;

  // Whether bubble text is short or long.
  FirstRun::BubbleType bubble_type_;

  // This is in case we're destroyed before the model loads. We need to make
  // Add/RemoveObserver calls.
  TemplateURLService* template_url_service_;

  // Tracks this preference to determine whether bookmark editing is allowed.
  BooleanPrefMember edit_bookmarks_enabled_;

  // While animating, the host clips the widget and draws only the bottom
  // part of it. The view needs to know the pixel offset at which we are drawing
  // the widget so that we can draw the curved edges that attach to the toolbar
  // in the right location.
  int animation_offset_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
