// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_

#include <gtk/gtk.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "chrome/common/page_transition_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditViewGtk;
class BubblePositioner;
class Browser;
class CommandUpdater;
class ExtensionAction;
class ExtensionActionContextMenuModel;
class GtkThemeProvider;
class Profile;
class SkBitmap;
class TabContents;
class ToolbarModel;

class LocationBarViewGtk : public AutocompleteEditController,
                           public LocationBar,
                           public LocationBarTesting,
                           public NotificationObserver {
 public:
  LocationBarViewGtk(CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     const BubblePositioner* bubble_positioner,
                     Browser* browser_);
  virtual ~LocationBarViewGtk();

  void Init(bool popup_window_mode);

  void SetProfile(Profile* profile);

  // Returns the widget the caller should host.  You must call Init() first.
  GtkWidget* widget() { return hbox_.get(); }

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

  // Implement the AutocompleteEditController interface.
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

  // Implement the LocationBar interface.
  virtual void ShowFirstRunBubble(bool use_OEM_bubble);
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdateContentBlockedIcons();
  virtual void UpdatePageActions();
  virtual void InvalidatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual AutocompleteEditView* location_entry() {
    return location_entry_.get();
  }
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Implement the LocationBarTesting interface.
  virtual int PageActionCount() { return page_action_views_.size(); }
  virtual int PageActionVisibleCount();
  virtual ExtensionAction* GetPageAction(size_t index);
  virtual ExtensionAction* GetVisiblePageAction(size_t index);
  virtual void TestPageActionPressed(size_t index);

  // Implement the NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Translation between a security level and the background color.  Both the
  // location bar and edit have to manage and match the background color.
  static const GdkColor kBackgroundColorByLevel[3];

 private:
  class PageActionViewGtk : public ImageLoadingTracker::Observer {
   public:
    PageActionViewGtk(
        LocationBarViewGtk* owner, Profile* profile,
        ExtensionAction* page_action);
    virtual ~PageActionViewGtk();

    GtkWidget* widget() { return event_box_.get(); }

    ExtensionAction* page_action() { return page_action_; }

    void set_preview_enabled(bool preview_enabled) {
      preview_enabled_ = preview_enabled;
    }

    bool IsVisible() { return GTK_WIDGET_VISIBLE(widget()); }

    // Called to notify the PageAction that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, GURL url);

    // A callback from ImageLoadingTracker for when the image has loaded.
    virtual void OnImageLoaded(SkBitmap* image, size_t index);

    // Simulate left mouse click on the page action button.
    void TestActivatePageAction();

   private:
    static gboolean OnButtonPressedThunk(GtkWidget* sender,
                                         GdkEvent* event,
                                         PageActionViewGtk* page_action_view) {
      return page_action_view->OnButtonPressed(sender, event);
    }
    gboolean OnButtonPressed(GtkWidget* sender, GdkEvent* event);

    static gboolean OnExposeEventThunk(GtkWidget* widget,
                                       GdkEventExpose* event,
                                       PageActionViewGtk* page_action_view) {
      return page_action_view->OnExposeEvent(widget, event);
    }
    gboolean OnExposeEvent(GtkWidget* widget, GdkEventExpose* event);

    // The location bar view that owns us.
    LocationBarViewGtk* owner_;

    // The current profile (not owned by us).
    Profile* profile_;

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
    // asynchronously.  It will delete itself once it is done.
    ImageLoadingTracker* tracker_;

    // The widgets for this page action.
    OwnedWidgetGtk event_box_;
    OwnedWidgetGtk image_;

    // The tab id we are currently showing the icon for.
    int current_tab_id_;

    // The URL we are currently showing the icon for.
    GURL current_url_;

    // This is used for post-install visual feedback. The page_action icon
    // is briefly shown even if it hasn't been enabled by its extension.
    bool preview_enabled_;

    // The context menu view and model for this extension action.
    scoped_ptr<MenuGtk> context_menu_;
    scoped_ptr<ExtensionActionContextMenuModel> context_menu_model_;

    DISALLOW_COPY_AND_ASSIGN(PageActionViewGtk);
  };
  friend class PageActionViewGtk;

  static gboolean HandleExposeThunk(GtkWidget* widget, GdkEventExpose* event,
                                    gpointer userdata) {
    return reinterpret_cast<LocationBarViewGtk*>(userdata)->
        HandleExpose(widget, event);
  }

  gboolean HandleExpose(GtkWidget* widget, GdkEventExpose* event);

  static gboolean OnSecurityIconPressed(GtkWidget* sender,
                                        GdkEventButton* event,
                                        LocationBarViewGtk* location_bar);

  // Set the SSL icon we should be showing.
  void SetSecurityIcon(ToolbarModel::Icon icon);

  // Sets the text that should be displayed in the info label and its associated
  // tooltip text.  Call with an empty string if the info label should be
  // hidden.
  void SetInfoText();

  // Set the keyword text for the Search BLAH: keyword box.
  void SetKeywordLabel(const std::wstring& keyword);

  // Set the keyword text for the "Press tab to search BLAH" hint box.
  void SetKeywordHintLabel(const std::wstring& keyword);

  void ShowFirstRunBubbleInternal(bool use_OEM_bubble);

  static void OnEntryBoxSizeAllocateThunk(GtkWidget* widget,
                                          GtkAllocation* allocation,
                                          gpointer userdata) {
    reinterpret_cast<LocationBarViewGtk*>(userdata)->
        OnEntryBoxSizeAllocate(allocation);
  }
  void OnEntryBoxSizeAllocate(GtkAllocation* allocation);

  // Show or hide |tab_to_search_box_|, |tab_to_search_hint_| and
  // |type_to_search_hint_| according to the value of |show_selected_keyword_|,
  // |show_keyword_hint_|, |show_search_hint_| and the available horizontal
  // space in the location bar.
  void AdjustChildrenVisibility();

  // The outermost widget we want to be hosted.
  OwnedWidgetGtk hbox_;

  // SSL icons.
  GtkWidget* security_icon_event_box_;
  GtkWidget* security_lock_icon_image_;
  GtkWidget* security_warning_icon_image_;
  // Toolbar info text (EV cert info).
  GtkWidget* info_label_;

  // Extension page action icons.
  OwnedWidgetGtk page_action_hbox_;
  ScopedVector<PageActionViewGtk> page_action_views_;

  // Area on the left shown when in tab to search mode.
  GtkWidget* tab_to_search_box_;
  GtkWidget* tab_to_search_full_label_;
  GtkWidget* tab_to_search_partial_label_;

  // Hint to user that they can tab-to-search by hitting tab.
  GtkWidget* tab_to_search_hint_;
  GtkWidget* tab_to_search_hint_leading_label_;
  GtkWidget* tab_to_search_hint_icon_;
  GtkWidget* tab_to_search_hint_trailing_label_;

  // Hint to user that the inputted text is not a keyword or url.
  GtkWidget* type_to_search_hint_;

  scoped_ptr<AutocompleteEditViewGtk> location_entry_;

  Profile* profile_;
  CommandUpdater* command_updater_;
  ToolbarModel* toolbar_model_;
  Browser* browser_;

  // We need to hold on to this just to it pass to the edit.
  const BubblePositioner* bubble_positioner_;

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation.
  PageTransition::Type transition_;

  // Used schedule a task for the first run info bubble.
  ScopedRunnableMethodFactory<LocationBarViewGtk> first_run_bubble_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // Provides colors and rendering mode.
  GtkThemeProvider* theme_provider_;

  NotificationRegistrar registrar_;

  // Width of the hbox that holds |tab_to_search_box_|, |location_entry_| and
  // |tab_to_search_hint_|.
  int entry_box_width_;

  // Indicate if |tab_to_search_box_| should be shown.
  bool show_selected_keyword_;

  // Indicate if |tab_to_search_hint_| should be shown.
  bool show_keyword_hint_;

  // Indicate if |type_to_search_hint_| should be shown.
  bool show_search_hint_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewGtk);
};

#endif  // CHROME_BROWSER_GTK_LOCATION_BAR_VIEW_GTK_H_
