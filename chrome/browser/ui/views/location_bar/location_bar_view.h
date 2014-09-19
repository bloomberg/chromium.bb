// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/drag_controller.h"

class ActionBoxButtonView;
class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class ContentSettingImageView;
class EVBubbleView;
class ExtensionAction;
class GURL;
class GeneratedCreditCardView;
class InstantController;
class KeywordHintView;
class LocationIconView;
class OpenPDFInReaderView;
class ManagePasswordsIconView;
class OriginChipView;
class PageActionWithBadgeView;
class PageActionImageView;
class Profile;
class SearchButton;
class SelectedKeywordView;
class StarView;
class TemplateURLService;
class TranslateIconView;
class ZoomView;

namespace content {
struct SSLStatus;
}

namespace gfx {
class SlideAnimation;
}

namespace views {
class BubbleDelegateView;
class ImageButton;
class ImageView;
class Label;
class Widget;
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
                        public views::ButtonListener,
                        public views::DragController,
                        public OmniboxEditController,
                        public DropdownBarHostDelegate,
                        public gfx::AnimationDelegate,
                        public TemplateURLServiceObserver,
                        public SearchModelObserver {
 public:
  // The location bar view's class name.
  static const char kViewClassName[];

  // Returns the offset used during dropdown animation.
  int dropdown_animation_offset() const { return dropdown_animation_offset_; }

  class Delegate {
   public:
    // Should return the current web contents.
    virtual content::WebContents* GetWebContents() = 0;

    // Returns the InstantController, or NULL if there isn't one.
    virtual InstantController* GetInstant() = 0;

    virtual ToolbarModel* GetToolbarModel() = 0;
    virtual const ToolbarModel* GetToolbarModel() const = 0;

    // Creates Widget for the given delegate.
    virtual views::Widget* CreateViewsBubble(
        views::BubbleDelegateView* bubble_delegate) = 0;

    // Creates PageActionImageView. Caller gets an ownership.
    virtual PageActionImageView* CreatePageActionImageView(
        LocationBarView* owner,
        ExtensionAction* action) = 0;

    // Returns ContentSettingBubbleModelDelegate.
    virtual ContentSettingBubbleModelDelegate*
        GetContentSettingBubbleModelDelegate() = 0;

    // Shows permissions and settings for the given web contents.
    virtual void ShowWebsiteSettings(content::WebContents* web_contents,
                                     const GURL& url,
                                     const content::SSLStatus& ssl) = 0;

   protected:
    virtual ~Delegate() {}
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    SELECTED_TEXT,
    DEEMPHASIZED_TEXT,
    SECURITY_TEXT,
  };

  LocationBarView(Browser* browser,
                  Profile* profile,
                  CommandUpdater* command_updater,
                  Delegate* delegate,
                  bool is_popup_mode);

  virtual ~LocationBarView();

  // Initializes the LocationBarView.
  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns the appropriate color for the desired kind, based on the user's
  // system theme.
  SkColor GetColor(ToolbarModel::SecurityLevel security_level,
                   ColorKind kind) const;

  // Returns the delegate.
  Delegate* delegate() const { return delegate_; }

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // The zoom icon. It may not be visible.
  ZoomView* zoom_view() { return zoom_view_; }

  // The passwords icon. It may not be visible.
  ManagePasswordsIconView* manage_passwords_icon_view() {
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

  // Toggles the translate icon on or off.
  void SetTranslateIconToggled(bool on);

  // The translate icon. It may not be visible.
  TranslateIconView* translate_icon_view() { return translate_icon_view_; }

  // Returns the screen coordinates of the omnibox (where the URL text appears,
  // not where the icons are shown).
  gfx::Point GetOmniboxViewOrigin() const;

  // Shows |text| as an inline autocompletion.  This is useful for IMEs, where
  // we can't show the autocompletion inside the actual OmniboxView.  See
  // comments on |ime_inline_autocomplete_view_|.
  void SetImeInlineAutocompletion(const base::string16& text);

  // Invoked from OmniboxViewWin to show gray text autocompletion.
  void SetGrayTextAutocompletion(const base::string16& text);

  // Returns the current gray text autocompletion.
  base::string16 GetGrayTextAutocompletion() const;

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

  views::View* generated_credit_card_view();

  // Returns the height of the control without the top and bottom
  // edges(i.e.  the height of the edit control inside).  If
  // |use_preferred_size| is true this will be the preferred height,
  // otherwise it will be the current height.
  int GetInternalHeight(bool use_preferred_size);

  // Returns the position and width that the popup should be, and also the left
  // edge that the results should align themselves to (which will leave some
  // border on the left of the popup).
  void GetOmniboxPopupPositioningInfo(gfx::Point* top_left_screen_coord,
                                      int* popup_width,
                                      int* left_margin,
                                      int* right_margin);

  // LocationBar:
  virtual void FocusLocation(bool select_all) OVERRIDE;
  virtual void Revert() OVERRIDE;
  virtual OmniboxView* GetOmniboxView() OVERRIDE;

  // views::View:
  virtual bool HasFocus() const OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // OmniboxEditController:
  virtual void Update(const content::WebContents* contents) OVERRIDE;
  virtual void ShowURL() OVERRIDE;
  virtual void EndOriginChipAnimations(bool cancel_fade) OVERRIDE;
  virtual ToolbarModel* GetToolbarModel() OVERRIDE;
  virtual content::WebContents* GetWebContents() OVERRIDE;

  // Thickness of the edges of the omnibox background images, in normal mode.
  static const int kNormalEdgeThickness;
  // The same, but for popup mode.
  static const int kPopupEdgeThickness;
  // Space between items in the location bar, as well as between items and the
  // edges.
  static const int kItemPadding;
  // Amount of padding built into the standard omnibox icons.
  static const int kIconInternalPadding;
  // Amount of padding to place between the origin chip and the leading edge of
  // the location bar.
  static const int kOriginChipEdgeItemPadding;
  // Amount of padding built into the origin chip.
  static const int kOriginChipBuiltinPadding;
  // Space between the edge and a bubble.
  static const int kBubblePadding;

 private:
  typedef std::vector<ContentSettingImageView*> ContentSettingViews;

  friend class PageActionImageView;
  friend class PageActionWithBadgeView;
  typedef std::vector<ExtensionAction*> PageActions;
  typedef std::vector<PageActionWithBadgeView*> PageActionViews;

  // Helper for GetMinimumWidth().  Calculates the incremental minimum width
  // |view| should add to the trailing width after the omnibox.
  static int IncrementalMinimumWidth(views::View* view);

  // Returns the thickness of any visible left and right edge, in pixels.
  int GetHorizontalEdgeThickness() const;

  // The same, but for the top and bottom edges.
  int vertical_edge_thickness() const {
    return is_popup_mode_ ? kPopupEdgeThickness : kNormalEdgeThickness;
  }

  // Updates the visibility state of the Content Blocked icons to reflect what
  // is actually blocked on the current page. Returns true if the visibility
  // of at least one of the views in |content_setting_views_| changed.
  bool RefreshContentSettingViews();

  // Deletes all page action views that we have created.
  void DeletePageActionViews();

  // Updates the views for the Page Actions, to reflect state changes for
  // PageActions. Returns true if the visibility of a PageActionWithBadgeView
  // changed, or PageActionWithBadgeView were created/destroyed.
  bool RefreshPageActionViews();

  // Updates the view for the zoom icon based on the current tab's zoom. Returns
  // true if the visibility of the view changed.
  bool RefreshZoomView();

  // Updates the Translate icon based on the current tab's Translate status.
  void RefreshTranslateIcon();

  // Updates |manage_passwords_icon_view_|. Returns true if visibility changed.
  bool RefreshManagePasswordsIconView();

  // Helper to show the first run info bubble.
  void ShowFirstRunBubbleInternal();

  // Returns true if the suggest text is valid.
  bool HasValidSuggestText() const;

  bool ShouldShowKeywordBubble() const;
  bool ShouldShowEVBubble() const;

  // Used to "reverse" the URL showing/hiding animations, since we use separate
  // animations whose curves are not true inverses of each other.  Based on the
  // current position of the omnibox, calculates what value the desired
  // animation (|hide_url_animation_| if |hide| is true, |show_url_animation_|
  // if it's false) should be set to in order to produce the same omnibox
  // position.  This way we can stop the old animation, set the new animation to
  // this value, and start it running, and the text will appear to reverse
  // directions from its current location.
  double GetValueForAnimation(bool hide) const;

  // Resets |show_url_animation_| and the color changes it causes.
  void ResetShowAnimationAndColors();

  // LocationBar:
  virtual void ShowFirstRunBubble() OVERRIDE;
  virtual GURL GetDestinationURL() const OVERRIDE;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const OVERRIDE;
  virtual ui::PageTransition GetPageTransition() const OVERRIDE;
  virtual void AcceptInput() OVERRIDE;
  virtual void FocusSearch() OVERRIDE;
  virtual void UpdateContentSettingsIcons() OVERRIDE;
  virtual void UpdateManagePasswordsIconAndBubble() OVERRIDE;
  virtual void UpdatePageActions() OVERRIDE;
  virtual void InvalidatePageActions() OVERRIDE;
  virtual void UpdateBookmarkStarVisibility() OVERRIDE;
  virtual bool ShowPageActionPopup(const extensions::Extension* extension,
                                   bool grant_active_tab) OVERRIDE;
  virtual void UpdateOpenPDFInReaderPrompt() OVERRIDE;
  virtual void UpdateGeneratedCreditCardView() OVERRIDE;
  virtual void SaveStateToContents(content::WebContents* contents) OVERRIDE;
  virtual const OmniboxView* GetOmniboxView() const OVERRIDE;
  virtual LocationBarTesting* GetLocationBarForTesting() OVERRIDE;

  // LocationBarTesting:
  virtual int PageActionCount() OVERRIDE;
  virtual int PageActionVisibleCount() OVERRIDE;
  virtual ExtensionAction* GetPageAction(size_t index) OVERRIDE;
  virtual ExtensionAction* GetVisiblePageAction(size_t index) OVERRIDE;
  virtual void TestPageActionPressed(size_t index) OVERRIDE;
  virtual bool GetBookmarkStarVisibility() OVERRIDE;

  // views::View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // OmniboxEditController:
  virtual void OnChanged() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual const ToolbarModel* GetToolbarModel() const OVERRIDE;
  virtual void HideURL() OVERRIDE;

  // DropdownBarHostDelegate:
  virtual void SetFocusAndSelection(bool select_all) OVERRIDE;
  virtual void SetAnimationOffset(int offset) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  // TemplateURLServiceObserver:
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // The Browser this LocationBarView is in.  Note that at least
  // chromeos::SimpleWebViewDialog uses a LocationBarView outside any browser
  // window, so this may be NULL.
  Browser* browser_;

  OmniboxViewViews* omnibox_view_;

  // Our delegate.
  Delegate* delegate_;

  // Object used to paint the border.
  scoped_ptr<views::Painter> border_painter_;

  // The origin chip that may appear in the location bar.
  OriginChipView* origin_chip_view_;

  // An icon to the left of the edit field.
  LocationIconView* location_icon_view_;

  // A bubble displayed for EV HTTPS sites.
  EVBubbleView* ev_bubble_view_;

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

  // View responsible for showing suggested text. This is NULL when there is no
  // suggested text.
  views::Label* suggested_text_view_;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView* keyword_hint_view_;

  // The voice search icon.
  views::ImageButton* mic_search_view_;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The zoom icon.
  ZoomView* zoom_view_;

  // A bubble that shows after successfully generating a new credit card number.
  GeneratedCreditCardView* generated_credit_card_view_;

  // The icon to open a PDF in Reader.
  OpenPDFInReaderView* open_pdf_in_reader_view_;

  // The manage passwords icon.
  ManagePasswordsIconView* manage_passwords_icon_view_;

  // The current page actions.
  PageActions page_actions_;

  // The page action icon views.
  PageActionViews page_action_views_;

  // The icon for Translate.
  TranslateIconView* translate_icon_view_;

  // The star.
  StarView* star_view_;

  // The search/go button.
  SearchButton* search_button_;

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

  // During dropdown animation, the host clips the widget and draws only the
  // bottom part of it. The view needs to know the pixel offset at which we are
  // drawing the widget so that we can draw the curved edges that attach to the
  // toolbar in the right location.
  int dropdown_animation_offset_;

  // Origin chip animations.
  //
  // For the "show URL" animation, we instantly hide the origin chip and show
  // the |omnibox_view_| in its place, containing the complete URL.  However, we
  // clip that view (using the XXX_leading_inset_ and XXX_width_ members) so
  // that only the hostname is visible.  We also offset the omnibox (using the
  // XXX_offset_ members) so the hostname is in the same place as it was in the
  // origin chip.  Finally, we set the selection text and background color of
  // the text to match the pressed origin chip.  Then, as the animation runs,
  // all of these values are animated to their steady-state values (no omnibox
  // offset, no inset, width equal to the full omnibox text [which is reset to
  // "no width clamp" after the animation ends], and standard selection colors).
  //
  // For the hide animation, we run the positioning and clipping parts of the
  // animation in reverse, but instead of changing the selection color, because
  // there usually isn't a selection when hiding, we leave the omnibox colors
  // alone, and when the hide animation has ended, tell the origin chip to
  // fade-in its background.
  scoped_ptr<gfx::SlideAnimation> show_url_animation_;
  scoped_ptr<gfx::SlideAnimation> hide_url_animation_;
  // The omnibox offset may be positive or negative.  The starting offset is the
  // amount necessary to shift the |omnibox_view_| by such that the hostname
  // portion of the URL aligns with the hostname in the origin chip.  As the
  // show animation runs, the current offset gradually moves to 0.
  int starting_omnibox_offset_;
  int current_omnibox_offset_;
  // The leading inset is always positive.  The starting inset is the width of
  // the text between the leading edge of the omnibox and the edge of the
  // hostname, which is clipped off at the start of the show animation.  Note
  // that in RTL mode, this will be the part of the URL that is logically after
  // the hostname.  As the show animation runs, the current inset gradually
  // moves to 0.
  int starting_omnibox_leading_inset_;
  int current_omnibox_leading_inset_;
  // The width is always positive.  The ending width is the width of the entire
  // omnibox URL.  As the show animation runs, the current width gradually moves
  // from the width of the hostname to the ending value.
  int current_omnibox_width_;
  int ending_omnibox_width_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
