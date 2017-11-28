// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/prefs/pref_member.h"
#include "components/security_state/core/security_state.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/drag_controller.h"

class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class FindBarIcon;
class GURL;
class IntentPickerView;
class KeywordHintView;
class LocationIconView;
class ManagePasswordsIconViews;
class Profile;
class SelectedKeywordView;
class StarView;
class TranslateIconView;
class ZoomView;

namespace autofill {
class SaveCardIconView;
}

namespace views {
class ImageButton;
class Label;
}

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
                        public gfx::AnimationDelegate,
                        public ChromeOmniboxEditController,
                        public DropdownBarHostDelegate,
                        public zoom::ZoomEventManagerObserver,
                        public views::ButtonListener,
                        public ContentSettingImageView::Delegate {
 public:
  class Delegate {
   public:
    // Should return the current web contents.
    virtual content::WebContents* GetWebContents() = 0;

    virtual ToolbarModel* GetToolbarModel() = 0;
    virtual const ToolbarModel* GetToolbarModel() const = 0;

    // Returns ContentSettingBubbleModelDelegate.
    virtual ContentSettingBubbleModelDelegate*
        GetContentSettingBubbleModelDelegate() = 0;

   protected:
    virtual ~Delegate() {}
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    SELECTED_TEXT,
    DEEMPHASIZED_TEXT,
    SECURITY_CHIP_TEXT,
  };

  // Visual width (and height) of icons in location bar.
  static constexpr int kIconWidth = 16;

  // The amount of padding between the visual edge of an icon and the edge of
  // its click target, for all all sides of the icon. The total edge length of
  // each icon view should be kIconWidth + 2 * kIconInteriorPadding.
  static constexpr int kIconInteriorPadding = 4;

  // The location bar view's class name.
  static const char kViewClassName[];

  LocationBarView(Browser* browser,
                  Profile* profile,
                  CommandUpdater* command_updater,
                  Delegate* delegate,
                  bool is_popup_mode);

  ~LocationBarView() override;

  // Initializes the LocationBarView.
  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns the appropriate color for the desired kind, based on the user's
  // system theme.
  SkColor GetColor(ColorKind kind) const;

  // Returns the location bar border color blended with the toolbar color.
  // It's guaranteed to be opaque.
  SkColor GetOpaqueBorderColor(bool incognito) const;

  // Returns the color to be used for security text in the context of
  // |security_level|.
  SkColor GetSecureTextColor(
      security_state::SecurityLevel security_level) const;

  // Returns the delegate.
  Delegate* delegate() const { return delegate_; }

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // The zoom icon. It may not be visible.
  ZoomView* zoom_view() { return zoom_view_; }

  // The passwords icon. It may not be visible.
  ManagePasswordsIconViews* manage_passwords_icon_view() {
    return manage_passwords_icon_view_;
  }

  // Toggles the star on or off.
  void SetStarToggled(bool on);

#if defined(OS_CHROMEOS)
  // The intent picker, should not always be visible.
  IntentPickerView* intent_picker_view() { return intent_picker_view_; }
#endif  // defined(OS_CHROMEOS)

  // The star. It may not be visible.
  StarView* star_view() { return star_view_; }

  // The save credit card icon. It may not be visible.
  autofill::SaveCardIconView* save_credit_card_icon_view() {
    return save_credit_card_icon_view_;
  }

  // The translate icon. It may not be visible.
  TranslateIconView* translate_icon_view() { return translate_icon_view_; }

  // Returns the screen coordinates of the omnibox (where the URL text appears,
  // not where the icons are shown).
  gfx::Point GetOmniboxViewOrigin() const;

  // Shows |text| as an inline autocompletion.  This is useful for IMEs, where
  // we can't show the autocompletion inside the actual OmniboxView.  See
  // comments on |ime_inline_autocomplete_view_|.
  void SetImeInlineAutocompletion(const base::string16& text);

  // Set if we should show a focus rect while the location entry field is
  // focused. Used when the toolbar is in full keyboard accessibility mode.
  // Repaints if necessary.
  virtual void SetShowFocusRect(bool show);

  // Select all of the text. Needed when the user tabs through controls
  // in the toolbar in full keyboard accessibility mode.
  virtual void SelectAll();

  LocationIconView* location_icon_view() { return location_icon_view_; }

  SelectedKeywordView* selected_keyword_view() {
    return selected_keyword_view_;
  }

  // Where InfoBar arrows should point. The point will be returned in the
  // coordinates of the LocationBarView.
  gfx::Point GetInfoBarAnchorPoint() const;

  // The anchor view for security-related bubbles. That is, those anchored to
  // the leading edge of the Omnibox, under the padlock.
  views::View* GetSecurityBubbleAnchorView();

  OmniboxViewViews* omnibox_view() { return omnibox_view_; }
  const OmniboxViewViews* omnibox_view() const { return omnibox_view_; }

  // Returns the position and width that the popup should be, and also the left
  // edge that the results should align themselves to (which will leave some
  // border on the left of the popup). |top_edge_overlap| specifies the number
  // of pixels the top edge of the popup should overlap the bottom edge of
  // the toolbar.
  void GetOmniboxPopupPositioningInfo(gfx::Point* top_left_screen_coord,
                                      int* popup_width,
                                      int* left_margin,
                                      int* right_margin,
                                      int top_edge_overlap);

  // Updates the controller, and, if |contents| is non-null, restores saved
  // state that the tab holds.
  void Update(const content::WebContents* contents);

  // Clears the location bar's state for |contents|.
  void ResetTabState(content::WebContents* contents);

  // LocationBar:
  void FocusLocation(bool select_all) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;

  // views::View:
  bool HasFocus() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override;
  ToolbarModel* GetToolbarModel() override;
  content::WebContents* GetWebContents() override;

  // ContentSettingImageView::Delegate:
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  static bool IsVirtualKeyboardVisible();

 private:
  using ContentSettingViews = std::vector<ContentSettingImageView*>;

  // Helper for GetMinimumWidth().  Calculates the incremental minimum width
  // |view| should add to the trailing width after the omnibox.
  int IncrementalMinimumWidth(views::View* view) const;

  // The border color, drawn on top of the toolbar.
  SkColor GetBorderColor() const;

  // Returns the thickness of any visible edge, in pixels.
  int GetHorizontalEdgeThickness() const;

  // Returns the total amount of space reserved above or below the content,
  // which is the vertical edge thickness plus the padding next to it.
  int GetTotalVerticalPadding() const;

  // Updates |location_icon_view_| based on the current state and theme.
  void RefreshLocationIcon();

  // Updates the visibility state of the Content Blocked icons to reflect what
  // is actually blocked on the current page. Returns true if the visibility
  // of at least one of the views in |content_setting_views_| changed.
  bool RefreshContentSettingViews();

  // Updates the view for the zoom icon based on the current tab's zoom. Returns
  // true if the visibility of the view changed.
  bool RefreshZoomView();

  // Updates |save_credit_card_icon_view_|. Returns true if visibility changed.
  bool RefreshSaveCreditCardIconView();

  // Updates |find_bar_icon_|. Returns true if visibility changed.
  bool RefreshFindBarIcon();

  // Updates the Translate icon based on the current tab's Translate status.
  void RefreshTranslateIcon();

  // Updates |manage_passwords_icon_view_|. Returns true if visibility changed.
  bool RefreshManagePasswordsIconView();

  // Updates the color of the icon for the "clear all" button.
  void RefreshClearAllButtonIcon();

  // Returns text to be placed in the location icon view.
  // - For secure/insecure pages, returns text describing the URL's security
  // level.
  // - For extension URLs, returns the extension name.
  // - For chrome:// URLs, returns the short product name (e.g. Chrome).
  base::string16 GetLocationIconText() const;

  bool ShouldShowKeywordBubble() const;

  // Returns true if any of the following is true:
  // - the current page is explicitly secure or insecure.
  // - the current page URL is a chrome-extension:// URL.
  bool ShouldShowLocationIconText() const;

  // Returns true if the location icon text should be animated.
  bool ShouldAnimateLocationIconTextVisibilityChange() const;

  // LocationBar:
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  void AcceptInput() override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override;
  void UpdateFindBarIconVisibility() override;
  void UpdateBookmarkStarVisibility() override;
  void UpdateZoomViewVisibility() override;
  void UpdateLocationBarVisibility(bool visible, bool animation) override;
  void SaveStateToContents(content::WebContents* contents) override;
  const OmniboxView* GetOmniboxView() const override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // LocationBarTesting:
  bool GetBookmarkStarVisibility() override;
  bool TestContentSettingImagePressed(size_t index) override;

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // ChromeOmniboxEditController:
  void OnChanged() override;
  const ToolbarModel* GetToolbarModel() const override;

  // DropdownBarHostDelegate:
  void SetFocusAndSelection(bool select_all) override;

  // The Browser this LocationBarView is in.  Note that at least
  // chromeos::SimpleWebViewDialog uses a LocationBarView outside any browser
  // window, so this may be NULL.
  Browser* browser_;

  OmniboxViewViews* omnibox_view_ = nullptr;

  // Our delegate.
  Delegate* delegate_;

  // An icon to the left of the edit field: the HTTPS lock, blank page icon,
  // search icon, EV HTTPS bubble, etc.
  LocationIconView* location_icon_view_ = nullptr;

  // A view to show inline autocompletion when an IME is active.  In this case,
  // we shouldn't change the text or selection inside the OmniboxView itself,
  // since this will conflict with the IME's control over the text.  So instead
  // we show any autocompletion in a separate field after the OmniboxView.
  views::Label* ime_inline_autocomplete_view_ = nullptr;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_.
  // These autocollapse when the edit needs the room.

  // Shown if the user has selected a keyword.
  SelectedKeywordView* selected_keyword_view_ = nullptr;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView* keyword_hint_view_ = nullptr;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The zoom icon.
  ZoomView* zoom_view_ = nullptr;

  // The manage passwords icon.
  ManagePasswordsIconViews* manage_passwords_icon_view_ = nullptr;

  // The save credit card icon.
  autofill::SaveCardIconView* save_credit_card_icon_view_ = nullptr;

  // The icon for Translate.
  TranslateIconView* translate_icon_view_ = nullptr;

#if defined(OS_CHROMEOS)
  // The intent picker for accessing ARC's apps.
  IntentPickerView* intent_picker_view_ = nullptr;
#endif  // defined(OS_CHROMEOS)

  // The icon displayed when the find bar is visible.
  FindBarIcon* find_bar_icon_ = nullptr;

  // The star for bookmarking.
  StarView* star_view_ = nullptr;

  // An [x] that appears in touch mode (when the OSK is visible) and allows the
  // user to clear all text.
  views::ImageButton* clear_all_button_ = nullptr;

  // Animation to control showing / hiding the location bar.
  gfx::SlideAnimation size_animation_{this};

  // Whether we're in popup mode. This value also controls whether the location
  // bar is read-only.
  const bool is_popup_mode_;

  // True if we should show a focus rect while the location entry field is
  // focused. Used when the toolbar is in full keyboard accessibility mode.
  bool show_focus_rect_ = false;

  // Tracks this preference to determine whether bookmark editing is allowed.
  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
