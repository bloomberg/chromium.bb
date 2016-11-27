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
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/security_state/core/security_state.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/drag_controller.h"

class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class ContentSettingImageView;
class ExtensionAction;
class GURL;
class KeywordHintView;
class LocationIconView;
class OpenPDFInReaderView;
class ManagePasswordsIconViews;
class PageActionWithBadgeView;
class PageActionImageView;
class Profile;
class SelectedKeywordView;
class StarView;
class TemplateURLService;
class TranslateIconView;
class ZoomView;

namespace autofill {
class SaveCardIconView;
}

namespace views {
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
                        public TemplateURLServiceObserver,
                        public zoom::ZoomEventManagerObserver {
 public:
  class Delegate {
   public:
    // Should return the current web contents.
    virtual content::WebContents* GetWebContents() = 0;

    virtual ToolbarModel* GetToolbarModel() = 0;
    virtual const ToolbarModel* GetToolbarModel() const = 0;

    // Creates PageActionImageView. Caller gets an ownership.
    virtual PageActionImageView* CreatePageActionImageView(
        LocationBarView* owner,
        ExtensionAction* action) = 0;

    // Returns ContentSettingBubbleModelDelegate.
    virtual ContentSettingBubbleModelDelegate*
        GetContentSettingBubbleModelDelegate() = 0;

    // Shows permissions and settings for the given web contents.
    virtual void ShowWebsiteSettings(
        content::WebContents* web_contents,
        const GURL& virtual_url,
        const security_state::SecurityInfo& security_info) = 0;

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

  // Width (and height) of icons in location bar.
  static constexpr int kIconWidth = 16;

  // The location bar view's class name.
  static const char kViewClassName[];

  LocationBarView(Browser* browser,
                  Profile* profile,
                  CommandUpdater* command_updater,
                  Delegate* delegate,
                  bool is_popup_mode);

  ~LocationBarView() override;

  // Returns the color for the location bar border given the window's
  // |incognito| state.
  static SkColor GetBorderColor(bool incognito);

  // Initializes the LocationBarView.
  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns the appropriate color for the desired kind, based on the user's
  // system theme.
  SkColor GetColor(ColorKind kind) const;

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

  // Sets |preview_enabled| for the PageAction View associated with this
  // |page_action|. If |preview_enabled| is true, the view will display the
  // PageActions icon even though it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                   bool preview_enabled);

  // Retrieves the PageAction View which is associated with |page_action|.
  PageActionWithBadgeView* GetPageActionView(ExtensionAction* page_action);

  // Toggles the star on or off.
  void SetStarToggled(bool on);

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

  // Return the point suitable for anchoring location-bar-anchored bubbles at.
  // The point will be returned in the coordinates of the LocationBarView.
  gfx::Point GetLocationBarAnchorPoint() const;

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
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override;
  ToolbarModel* GetToolbarModel() override;
  content::WebContents* GetWebContents() override;

  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

 private:
  using ContentSettingViews = std::vector<ContentSettingImageView*>;

  friend class PageActionImageView;
  friend class PageActionWithBadgeView;
  using PageActions = std::vector<ExtensionAction*>;
  using PageActionViews = std::vector<std::unique_ptr<PageActionWithBadgeView>>;

  // Helper for GetMinimumWidth().  Calculates the incremental minimum width
  // |view| should add to the trailing width after the omnibox.
  int IncrementalMinimumWidth(views::View* view) const;

  // Returns the thickness of any visible edge, in pixels.
  int GetHorizontalEdgeThickness() const;
  int GetVerticalEdgeThickness() const;

  // Returns the total amount of space reserved above or below the content,
  // which is the vertical edge thickness plus the padding next to it.
  int GetTotalVerticalPadding() const;

  // Updates |location_icon_view_| based on the current state and theme.
  void RefreshLocationIcon();

  // Updates the visibility state of the Content Blocked icons to reflect what
  // is actually blocked on the current page. Returns true if the visibility
  // of at least one of the views in |content_setting_views_| changed.
  bool RefreshContentSettingViews();

  // Clears |page_action_views_| and removes the elements from the view
  // hierarchy.
  void DeletePageActionViews();

  // Updates the views for the Page Actions, to reflect state changes for
  // PageActions. Returns true if the visibility of a PageActionWithBadgeView
  // changed, or PageActionWithBadgeView were created/destroyed.
  bool RefreshPageActionViews();

  // Whether the page actions represented by |page_action_views_| differ
  // in ordering or value from |page_actions|.
  bool PageActionsDiffer(const PageActions& page_actions) const;

  // Updates the view for the zoom icon based on the current tab's zoom. Returns
  // true if the visibility of the view changed.
  bool RefreshZoomView();

  // Updates |save_credit_card_icon_view_|. Returns true if visibility changed.
  bool RefreshSaveCreditCardIconView();

  // Updates the Translate icon based on the current tab's Translate status.
  void RefreshTranslateIcon();

  // Updates |manage_passwords_icon_view_|. Returns true if visibility changed.
  bool RefreshManagePasswordsIconView();

  // Helper to show the first run info bubble.
  void ShowFirstRunBubbleInternal();

  // Returns text describing the URL's security level, to be placed in the
  // security chip.
  base::string16 GetSecurityText() const;

  bool ShouldShowKeywordBubble() const;

  // Returns true when the current page is explicitly secure or insecure.
  // In these cases, we should show the state of the security chip.
  bool ShouldShowSecurityChip() const;

  // Returns true if the chip should be animated
  bool ShouldAnimateSecurityChip() const;

  // Used to "reverse" the URL showing/hiding animations, since we use separate
  // animations whose curves are not true inverses of each other.  Based on the
  // current position of the omnibox, calculates what value the desired
  // animation (|hide_url_animation_| if |hide| is true, |show_url_animation_|
  // if it's false) should be set to in order to produce the same omnibox
  // position.  This way we can stop the old animation, set the new animation to
  // this value, and start it running, and the text will appear to reverse
  // directions from its current location.
  double GetValueForAnimation(bool hide) const;

  // LocationBar:
  void ShowFirstRunBubble() override;
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  void AcceptInput() override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override;
  void UpdatePageActions() override;
  void UpdateBookmarkStarVisibility() override;
  void UpdateLocationBarVisibility(bool visible, bool animation) override;
  bool ShowPageActionPopup(const extensions::Extension* extension,
                           bool grant_active_tab) override;
  void UpdateOpenPDFInReaderPrompt() override;
  void SaveStateToContents(content::WebContents* contents) override;
  const OmniboxView* GetOmniboxView() const override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // LocationBarTesting:
  int PageActionCount() override;
  int PageActionVisibleCount() override;
  ExtensionAction* GetPageAction(size_t index) override;
  ExtensionAction* GetVisiblePageAction(size_t index) override;
  void TestPageActionPressed(size_t index) override;
  bool GetBookmarkStarVisibility() override;

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;
  void OnPaint(gfx::Canvas* canvas) override;

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

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // The Browser this LocationBarView is in.  Note that at least
  // chromeos::SimpleWebViewDialog uses a LocationBarView outside any browser
  // window, so this may be NULL.
  Browser* browser_;

  OmniboxViewViews* omnibox_view_;

  // Our delegate.
  Delegate* delegate_;

  // An icon to the left of the edit field: the HTTPS lock, blank page icon,
  // search icon, EV HTTPS bubble, etc.
  LocationIconView* location_icon_view_;

  // A view to show inline autocompletion when an IME is active.  In this case,
  // we shouldn't change the text or selection inside the OmniboxView itself,
  // since this will conflict with the IME's control over the text.  So instead
  // we show any autocompletion in a separate field after the OmniboxView.
  views::Label* ime_inline_autocomplete_view_;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_.
  // These autocollapse when the edit needs the room.

  // Shown if the user has selected a keyword.
  SelectedKeywordView* selected_keyword_view_;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView* keyword_hint_view_;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The zoom icon.
  ZoomView* zoom_view_;

  // The icon to open a PDF in Reader.
  OpenPDFInReaderView* open_pdf_in_reader_view_;

  // The manage passwords icon.
  ManagePasswordsIconViews* manage_passwords_icon_view_;

  // The page action icon views.
  PageActionViews page_action_views_;

  // The save credit card icon.
  autofill::SaveCardIconView* save_credit_card_icon_view_;

  // The icon for Translate.
  TranslateIconView* translate_icon_view_;

  // The star.
  StarView* star_view_;

  // Animation to control showing / hiding the location bar.
  gfx::SlideAnimation size_animation_;

  // Whether we're in popup mode. This value also controls whether the location
  // bar is read-only.
  const bool is_popup_mode_;

  // True if we should show a focus rect while the location entry field is
  // focused. Used when the toolbar is in full keyboard accessibility mode.
  bool show_focus_rect_;

  // This is in case we're destroyed before the model loads. We need to make
  // Add/RemoveObserver calls.
  TemplateURLService* template_url_service_;

  // Tracks this preference to determine whether bookmark editing is allowed.
  BooleanPrefMember edit_bookmarks_enabled_;

  // This is a debug state variable that stores if the WebContents was null
  // during the last RefreshPageAction.
  bool web_contents_null_at_last_refresh_;

  // These allow toggling the verbose security state behavior via flags.
  bool should_show_secure_state_;
  bool should_show_nonsecure_state_;
  bool should_animate_secure_state_;
  bool should_animate_nonsecure_state_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
