// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_

#include <string>
#include <map>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/content_settings_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class AutocompleteTextField;
class CommandUpdater;
class ContentSettingImageModel;
class EVBubbleDecoration;
@class ExtensionPopupController;
class LocationIconDecoration;
class Profile;
class SelectedKeywordDecoration;
class ToolbarModel;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an AutocompleteEditViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public AutocompleteEditController,
                           public LocationBar,
                           public LocationBarTesting,
                           public NotificationObserver {
 public:
  LocationBarViewMac(AutocompleteTextField* field,
                     CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     Profile* profile,
                     Browser* browser);
  virtual ~LocationBarViewMac();

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(FirstRun::BubbleType bubble_type);
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void FocusLocation(bool select_all);
  virtual void FocusSearch();
  virtual void UpdateContentSettingsIcons();
  virtual void UpdatePageActions();
  virtual void InvalidatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual const AutocompleteEditView* location_entry() const {
    return edit_view_.get();
  }
  virtual AutocompleteEditView* location_entry() {
    return edit_view_.get();
  }
  virtual void PushForceHidden() {}
  virtual void PopForceHidden() {}
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Overridden from LocationBarTesting:
  virtual int PageActionCount();
  virtual int PageActionVisibleCount();
  virtual ExtensionAction* GetPageAction(size_t index);
  virtual ExtensionAction* GetVisiblePageAction(size_t index);
  virtual void TestPageActionPressed(size_t index);

  // Set/Get the editable state of the field.
  void SetEditable(bool editable);
  bool IsEditable();

  // Set the starred state of the bookmark star.
  void SetStarred(bool starred);

  // Get the point on the star for the bookmark bubble to aim at.
  NSPoint GetBookmarkBubblePoint() const;

  // Updates the location bar.  Resets the bar's permanent text and
  // security style, and if |should_restore_state| is true, restores
  // saved state from the tab (for tab switching).
  void Update(const TabContents* tab, bool should_restore_state);

  // Layout the various decorations which live in the field.
  void Layout();

  // Returns the current TabContents.
  TabContents* GetTabContents() const;

  // Sets preview_enabled_ for the PageActionImageView associated with this
  // |page_action|. If |preview_enabled|, the location bar will display the
  // PageAction icon even if it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                   bool preview_enabled);

  // Return |page_action|'s info-bubble point in window coordinates.
  // Returns |NSZeroPoint| if |page_action| is not present.
  NSPoint GetPageActionBubblePoint(ExtensionAction* page_action);

  // PageActionImageView is nested in LocationBarViewMac, and only needed
  // here so that we can access the icon of a page action when preview_enabled_
  // has been set.
  class PageActionImageView;

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

  NSImage* GetTabButtonImage();
  AutocompleteTextField* GetAutocompleteTextField() { return field_; }

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Used to display a clickable icon in the location bar.
  class LocationBarImageView {
   public:
    explicit LocationBarImageView() : image_(nil),
                                      label_(nil),
                                      visible_(false) {}
    virtual ~LocationBarImageView() {}

    // Sets the image.
    void SetImage(NSImage* image);

    // Get the |resource_id| image resource and set the image.
    void SetIcon(int resource_id);

    // Sets the label text, font, and color. |text| may be nil; |color| and
    // |font| are ignored if |text| is nil.
    void SetLabel(NSString* text, NSFont* baseFont, NSColor* color);

    // Sets the visibility. SetImage() should be called with a valid image
    // before the visibility is set to |true|.
    void SetVisible(bool visible);

    NSImage* GetImage() const { return image_; }
    NSAttributedString* GetLabel() const { return label_; }
    bool IsVisible() const { return visible_; }

    // Default size when no image is present.
    virtual NSSize GetDefaultImageSize() const;

    // Returns the size of the image, else the default size.
    NSSize GetImageSize() const;

    // Returns the tooltip for this image view or |nil| if there is none.
    virtual NSString* GetToolTip() { return nil; }

    // Used to determinate if the item can act as a drag source.
    virtual bool IsDraggable() { return false; }

    // The drag pasteboard to use if a drag is initiated.
    virtual NSPasteboard* GetDragPasteboard() { return nil; }

    // Called on mouse down.
    virtual void OnMousePressed(NSRect bounds) {}

    // Called to get the icon's context menu. Return |nil| for no menu.
    virtual NSMenu* GetMenu() { return nil; }

   private:
    scoped_nsobject<NSImage> image_;

    // The label shown next to the icon, or nil if none.
    scoped_nsobject<NSAttributedString> label_;

    bool visible_;

    DISALLOW_COPY_AND_ASSIGN(LocationBarImageView);
  };

  // Used to display the bookmark star in the RHS.
  class StarIconView : public LocationBarImageView {
   public:
    explicit StarIconView(CommandUpdater* command_updater);
    virtual ~StarIconView() {}

    // Shows the bookmark bubble.
    virtual void OnMousePressed(NSRect bounds);

    // Set the image and tooltip based on |starred|.
    void SetStarred(bool starred);

    virtual NSString* GetToolTip();

   private:
    // For bringing up bookmark bar.
    CommandUpdater* command_updater_;  // Weak, owned by Browser.

    // The string to show for a tooltip.
    scoped_nsobject<NSString> tooltip_;

    DISALLOW_COPY_AND_ASSIGN(StarIconView);
  };

  // PageActionImageView is used to display the icon for a given Page Action
  // and notify the extension when the icon is clicked.
  class PageActionImageView : public LocationBarImageView,
                              public ImageLoadingTracker::Observer,
                              public NotificationObserver {
   public:
    PageActionImageView(LocationBarViewMac* owner,
                        Profile* profile,
                        ExtensionAction* page_action);
    virtual ~PageActionImageView();

    ExtensionAction* page_action() { return page_action_; }

    int current_tab_id() { return current_tab_id_; }

    void set_preview_enabled(bool enabled) { preview_enabled_ = enabled; }

    bool preview_enabled() const { return preview_enabled_; }

    // When a new page action is created, all the icons are destroyed and
    // recreated; at this point we need to calculate sizes to lay out the
    // icons even though no images are available yet.  For this case, we return
    // the default image size for a page icon.
    virtual NSSize GetDefaultImageSize() const;

    // Either notify listeners or show a popup depending on the Page Action.
    virtual void OnMousePressed(NSRect bounds);

    // Overridden from ImageLoadingTracker.
    virtual void OnImageLoaded(
        SkBitmap* image, ExtensionResource resource, int index);

    // Called to notify the Page Action that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, const GURL& url);

    // Sets the tooltip for this Page Action image.
    void SetToolTip(NSString* tooltip);
    void SetToolTip(std::string tooltip);

    // Returns the tooltip for this Page Action image or |nil| if there is none.
    virtual NSString* GetToolTip();

    // Overridden to return a menu.
    virtual NSMenu* GetMenu();

   protected:
    // For unit testing only.
    PageActionImageView() : owner_(NULL),
                            profile_(NULL),
                            page_action_(NULL),
                            tracker_(this),
                            current_tab_id_(-1),
                            preview_enabled_(false) {}

   private:
    // Overridden from NotificationObserver:
    virtual void Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details);

    // The location bar view that owns us.
    LocationBarViewMac* owner_;

    // The current profile (not owned by us).
    Profile* profile_;

    // The Page Action that this view represents. The Page Action is not owned
    // by us, it resides in the extension of this particular profile.
    ExtensionAction* page_action_;

    // A cache of images the Page Actions might need to show, mapped by path.
    typedef std::map<std::string, SkBitmap> PageActionMap;
    PageActionMap page_action_icons_;

    // The object that is waiting for the image loading to complete
    // asynchronously.
    ImageLoadingTracker tracker_;

    // The tab id we are currently showing the icon for.
    int current_tab_id_;

    // The URL we are currently showing the icon for.
    GURL current_url_;

    // The string to show for a tooltip.
    scoped_nsobject<NSString> tooltip_;

    // This is used for post-install visual feedback. The page_action icon
    // is briefly shown even if it hasn't been enabled by its extension.
    bool preview_enabled_;

    // Used to register for notifications received by NotificationObserver.
    NotificationRegistrar registrar_;

    DISALLOW_COPY_AND_ASSIGN(PageActionImageView);
  };

  // ContentSettingImageView is used to display the content settings images
  // on the current page.
  class ContentSettingImageView : public LocationBarImageView {
   public:
    ContentSettingImageView(ContentSettingsType settings_type,
                            LocationBarViewMac* owner,
                            Profile* profile);
    virtual ~ContentSettingImageView();

    // Shows a content settings bubble.
    void OnMousePressed(NSRect bounds);

    // Updates the image and visibility state based on the supplied TabContents.
    void UpdateFromTabContents(const TabContents* tab_contents);

    // Returns the tooltip for this Page Action image or |nil| if there is none.
    virtual NSString* GetToolTip();

   private:
    void SetToolTip(NSString* tooltip);

    scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

    LocationBarViewMac* owner_;
    Profile* profile_;
    scoped_nsobject<NSString> tooltip_;

    DISALLOW_COPY_AND_ASSIGN(ContentSettingImageView);
  };
  typedef ScopedVector<ContentSettingImageView> ContentSettingViews;

  class PageActionViewList {
   public:
    PageActionViewList(LocationBarViewMac* location_bar,
                       Profile* profile,
                       ToolbarModel* toolbar_model)
        : owner_(location_bar),
          profile_(profile),
          toolbar_model_(toolbar_model) {}
    ~PageActionViewList() {
      DeleteAll();
    }

    void DeleteAll();
    void RefreshViews();

    PageActionImageView* ViewAt(size_t index);

    size_t Count();
    size_t VisibleCount();

    // Called when the action at |index| is clicked. The |iconFrame| should
    // describe the bounds of the affected action's icon.
    void OnMousePressed(NSRect iconFrame, size_t index);

   protected:
    // Any installed Page Actions.  Exposed for unit testing only.
    std::vector<PageActionImageView*> views_;

   private:
    // The location bar view that owns us.
    LocationBarViewMac* owner_;

    Profile* profile_;
    ToolbarModel* toolbar_model_;

    DISALLOW_COPY_AND_ASSIGN(PageActionViewList);
  };

 private:
  // Posts |notification| to the default notification center.
  void PostNotification(NSString* notification);

  // Updates visibility of the content settings icons based on the current
  // tab contents state.
  void RefreshContentSettingsViews();

  void ShowFirstRunBubbleInternal(FirstRun::BubbleType bubble_type);

  scoped_ptr<AutocompleteEditViewMac> edit_view_;

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  AutocompleteTextField* field_;  // owned by tab controller

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // A decoration that shows an icon to the left of the address.
  scoped_ptr<LocationIconDecoration> location_icon_decoration_;

  // A decoration that shows the keyword-search bubble on the left.
  scoped_ptr<SelectedKeywordDecoration> selected_keyword_decoration_;

  // A decoration that shows a lock icon and ev-cert label in a bubble
  // on the left.
  scoped_ptr<EVBubbleDecoration> ev_bubble_decoration_;

  // Bookmark star right of page actions.
  StarIconView star_icon_view_;

  // Any installed Page Actions.
  PageActionViewList page_action_views_;

  // The content blocked views.
  ContentSettingViews content_setting_views_;

  Profile* profile_;

  Browser* browser_;

  ToolbarModel* toolbar_model_;  // Weak, owned by Browser.

  // Image used in drawing keyword hint.
  scoped_nsobject<NSImage> tab_button_image_;

  // The transition type to use for the navigation.
  PageTransition::Type transition_;

  // Used to register for notifications received by NotificationObserver.
  NotificationRegistrar registrar_;

  // Used to schedule a task for the first run info bubble.
  ScopedRunnableMethodFactory<LocationBarViewMac> first_run_bubble_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
