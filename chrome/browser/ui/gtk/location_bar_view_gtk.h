// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_LOCATION_BAR_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_LOCATION_BAR_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "webkit/glue/window_open_disposition.h"

class OmniboxViewGtk;
class Browser;
class CommandUpdater;
class ContentSettingImageModel;
class ContentSettingBubbleGtk;
class ExtensionAction;
class GtkThemeService;
class SkBitmap;
class TabContents;
class ToolbarModel;

class LocationBarViewGtk : public AutocompleteEditController,
                           public LocationBar,
                           public LocationBarTesting,
                           public content::NotificationObserver {
 public:
  explicit LocationBarViewGtk(Browser* browser);
  virtual ~LocationBarViewGtk();

  void Init(bool popup_window_mode);

  // Returns the widget the caller should host.  You must call Init() first.
  GtkWidget* widget() { return hbox_.get(); }

  // Returns the widget the page info bubble should point to.
  GtkWidget* location_icon_widget() const { return location_icon_image_; }

  // Returns the widget the extension installed bubble should point to.
  GtkWidget* location_entry_widget() const { return entry_box_; }

  Browser* browser() const { return browser_; }

  // Returns the current TabContents.
  TabContents* GetTabContents() const;

  // Sets |preview_enabled| for the PageActionViewGtk associated with this
  // |page_action|. If |preview_enabled| is true, the view will display the
  // page action's icon even though it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubbleGtk to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction *page_action,
                                   bool preview_enabled);

  // Retrieves the GtkWidget which is associated with PageActionView
  // corresponding to |page_action|.
  GtkWidget* GetPageActionWidget(ExtensionAction* page_action);

  // Updates the location bar.  We also reset the bar's permanent text and
  // security style, and, if |tab_for_state_restoring| is non-NULL, also
  // restore saved state that the tab holds.
  void Update(const TabContents* tab_for_state_restoring);

  // Show the bookmark bubble.
  void ShowStarBubble(const GURL& url, bool newly_boomkarked);

  // Set the starred state of the bookmark star.
  void SetStarred(bool starred);

  // Implement the AutocompleteEditController interface.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    content::PageTransition transition,
                                    const GURL& alternate_nav_url) OVERRIDE;
  virtual void OnChanged() OVERRIDE;
  virtual void OnSelectionBoundsChanged() OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;
  virtual SkBitmap GetFavicon() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE;

  // Implement the LocationBar interface.
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

  // Implement the LocationBarTesting interface.
  virtual int PageActionCount() OVERRIDE;
  virtual int PageActionVisibleCount() OVERRIDE;
  virtual ExtensionAction* GetPageAction(size_t index) OVERRIDE;
  virtual ExtensionAction* GetVisiblePageAction(size_t index) OVERRIDE;
  virtual void TestPageActionPressed(size_t index) OVERRIDE;

  // Implement the content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Edit background color.
  static const GdkColor kBackgroundColor;

 private:
  class ContentSettingImageViewGtk : public BubbleDelegateGtk,
                                     public ui::AnimationDelegate {
   public:
    ContentSettingImageViewGtk(ContentSettingsType content_type,
                               const LocationBarViewGtk* parent);
    virtual ~ContentSettingImageViewGtk();

    GtkWidget* widget() { return alignment_.get(); }

    bool IsVisible();
    void UpdateFromTabContents(TabContents* tab_contents);

    // Overridden from ui::AnimationDelegate:
    virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
    virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
    virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

   private:
    // Start the process of showing the label.
    void StartAnimating();

    // Slide the label shut.
    void CloseAnimation();

    CHROMEGTK_CALLBACK_1(ContentSettingImageViewGtk, gboolean, OnButtonPressed,
                         GdkEvent*);
    CHROMEGTK_CALLBACK_1(ContentSettingImageViewGtk, gboolean, OnExpose,
                         GdkEventExpose*);

    // BubbleDelegateGtk overrides:
    virtual void BubbleClosing(BubbleGtk* bubble,
                               bool closed_by_escape) OVERRIDE;

    scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

    // The widgets for this content settings view.
    ui::OwnedWidgetGtk alignment_;
    ui::OwnedWidgetGtk event_box_;
    GtkWidget* hbox_;
    ui::OwnedWidgetGtk image_;

    // Explanatory text ("popup blocked").
    ui::OwnedWidgetGtk label_;

    // The owning LocationBarViewGtk.
    const LocationBarViewGtk* parent_;

    // The currently shown bubble if any.
    ContentSettingBubbleGtk* content_setting_bubble_;

    // When we show explanatory text, we slide it in/out.
    ui::SlideAnimation animation_;

    // The label's default requisition (cached so we can animate accordingly).
    GtkRequisition label_req_;

    base::WeakPtrFactory<ContentSettingImageViewGtk> weak_factory_;

    DISALLOW_COPY_AND_ASSIGN(ContentSettingImageViewGtk);
  };

  class PageActionViewGtk : public ImageLoadingTracker::Observer,
                            public ExtensionContextMenuModel::PopupDelegate {
   public:
    PageActionViewGtk(LocationBarViewGtk* owner, ExtensionAction* page_action);
    virtual ~PageActionViewGtk();

    GtkWidget* widget() { return event_box_.get(); }

    ExtensionAction* page_action() { return page_action_; }

    void set_preview_enabled(bool preview_enabled) {
      preview_enabled_ = preview_enabled;
    }

    bool IsVisible();

    // Called to notify the PageAction that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, const GURL& url);

    // A callback from ImageLoadingTracker for when the image has loaded.
    virtual void OnImageLoaded(
        SkBitmap* image, const ExtensionResource& resource, int index) OVERRIDE;

    // Simulate left mouse click on the page action button.
    void TestActivatePageAction();

    // Overridden from ExtensionContextMenuModel::PopupDelegate:
    virtual void InspectPopup(ExtensionAction* action) OVERRIDE;

   private:
    // Show the popup for this page action. If |devtools| is true, show it
    // with a debugger window attached. Returns true if a popup was shown.
    bool ShowPopup(bool devtools);

    CHROMEGTK_CALLBACK_1(PageActionViewGtk, gboolean, OnButtonPressed,
                         GdkEventButton*);
    CHROMEGTK_CALLBACK_1(PageActionViewGtk, gboolean, OnExposeEvent,
                         GdkEventExpose*);

    // The location bar view that owns us.
    LocationBarViewGtk* owner_;

    // The PageAction that this view represents. The PageAction is not owned by
    // us, it resides in the extension of this particular profile.
    ExtensionAction* page_action_;

    // A cache of all the different icon paths associated with this page action.
    typedef std::map<std::string, GdkPixbuf*> PixbufMap;
    PixbufMap pixbufs_;

    // A cache of the last dynamically generated bitmap and the pixbuf that
    // corresponds to it. We keep track of both so we can free old pixbufs as
    // their icons are replaced.
    SkBitmap last_icon_skbitmap_;
    GdkPixbuf* last_icon_pixbuf_;

    // The object that is waiting for the image loading to complete
    // asynchronously.
    ImageLoadingTracker tracker_;

    // The widgets for this page action.
    ui::OwnedWidgetGtk event_box_;
    ui::OwnedWidgetGtk image_;

    // The tab id we are currently showing the icon for.
    int current_tab_id_;

    // The URL we are currently showing the icon for.
    GURL current_url_;

    // This is used for post-install visual feedback. The page_action icon
    // is briefly shown even if it hasn't been enabled by its extension.
    bool preview_enabled_;

    // The context menu view and model for this extension action.
    scoped_ptr<MenuGtk> context_menu_;
    scoped_refptr<ExtensionContextMenuModel> context_menu_model_;

    DISALLOW_COPY_AND_ASSIGN(PageActionViewGtk);
  };
  friend class PageActionViewGtk;

  // Creates, initializes, and packs the location icon, EV certificate name,
  // and optional border.
  void BuildSiteTypeArea();

  // Enable or disable the location icon/EV certificate as a drag source for
  // the URL.
  void SetSiteTypeDragSource();

  GtkWidget* site_type_area() { return site_type_alignment_; }

  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, gboolean, HandleExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, gboolean, OnIconReleased,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_4(LocationBarViewGtk, void, OnIconDragData,
                       GdkDragContext*, GtkSelectionData*, guint, guint);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, void, OnIconDragBegin,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, void, OnIconDragEnd,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, void, OnHboxSizeAllocate,
                       GtkAllocation*);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, void, OnEntryBoxSizeAllocate,
                       GtkAllocation*);
  CHROMEGTK_CALLBACK_1(LocationBarViewGtk, gboolean, OnStarButtonPress,
                       GdkEventButton*);

  // Updates the site type area: changes the icon and shows/hides the EV
  // certificate information.
  void UpdateSiteTypeArea();

  // Updates the maximum size of the EV certificate label.
  void UpdateEVCertificateLabelSize();

  // Sets the text that should be displayed in the info label and its associated
  // tooltip text.  Call with an empty string if the info label should be
  // hidden.
  void SetInfoText();

  // Set the keyword text for the Search BLAH: keyword box.
  void SetKeywordLabel(const string16& keyword);

  // Set the keyword text for the "Press tab to search BLAH" hint box.
  void SetKeywordHintLabel(const string16& keyword);

  void ShowFirstRunBubbleInternal(FirstRun::BubbleType bubble_type);

  // Show or hide |tab_to_search_box_| and |tab_to_search_hint_| according to
  // the value of |show_selected_keyword_|, |show_keyword_hint_|, and the
  // available horizontal space in the location bar.
  void AdjustChildrenVisibility();

  // Build the star icon.
  void CreateStarButton();

  // Update the star icon after it is toggled or the theme changes.
  void UpdateStarIcon();

  // Returns true if we should only show the URL and none of the extras like
  // the star button or page actions.
  bool ShouldOnlyShowLocation();

  // The outermost widget we want to be hosted.
  ui::OwnedWidgetGtk hbox_;

  // Star button.
  ui::OwnedWidgetGtk star_;
  GtkWidget* star_image_;
  bool starred_;

  // An icon to the left of the address bar.
  GtkWidget* site_type_alignment_;
  GtkWidget* site_type_event_box_;
  GtkWidget* location_icon_image_;
  GtkWidget* drag_icon_;
  bool enable_location_drag_;
  // TODO(pkasting): Split this label off and move the rest of the items to the
  // left of the address bar.
  GtkWidget* security_info_label_;

  // Content setting icons.
  ui::OwnedWidgetGtk content_setting_hbox_;
  ScopedVector<ContentSettingImageViewGtk> content_setting_views_;

  // Extension page action icons.
  ui::OwnedWidgetGtk page_action_hbox_;
  ScopedVector<PageActionViewGtk> page_action_views_;

  // The widget that contains our tab hints and the location bar.
  GtkWidget* entry_box_;

  // Area on the left shown when in tab to search mode.
  GtkWidget* tab_to_search_alignment_;
  GtkWidget* tab_to_search_box_;
  GtkWidget* tab_to_search_magnifier_;
  GtkWidget* tab_to_search_full_label_;
  GtkWidget* tab_to_search_partial_label_;

  // Hint to user that they can tab-to-search by hitting tab.
  GtkWidget* tab_to_search_hint_;
  GtkWidget* tab_to_search_hint_leading_label_;
  GtkWidget* tab_to_search_hint_icon_;
  GtkWidget* tab_to_search_hint_trailing_label_;

  scoped_ptr<OmniboxViewGtk> location_entry_;

  // Alignment used to wrap |location_entry_|.
  GtkWidget* location_entry_alignment_;

  CommandUpdater* command_updater_;
  ToolbarModel* toolbar_model_;
  Browser* browser_;

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  string16 location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation.
  content::PageTransition transition_;

  // Used to schedule a task for the first run bubble.
  base::WeakPtrFactory<LocationBarViewGtk> first_run_bubble_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // Provides colors and rendering mode.
  GtkThemeService* theme_service_;

  content::NotificationRegistrar registrar_;

  // Width of the main |hbox_|. Used to properly elide the EV certificate.
  int hbox_width_;

  // Width of the hbox that holds |tab_to_search_box_|, |location_entry_| and
  // |tab_to_search_hint_|.
  int entry_box_width_;

  // Indicate if |tab_to_search_box_| should be shown.
  bool show_selected_keyword_;

  // Indicate if |tab_to_search_hint_| should be shown.
  bool show_keyword_hint_;

  // The last search keyword that was shown via the |tab_to_search_box_|.
  string16 last_keyword_;

  // Used to change the visibility of the star decoration.
  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_LOCATION_BAR_VIEW_GTK_H_
