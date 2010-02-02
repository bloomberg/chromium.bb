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
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/toolbar_model.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class AutocompleteTextField;
class BubblePositioner;
class CommandUpdater;
@class ExtensionPopupController;
class Profile;
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
                     const BubblePositioner* bubble_positioner,
                     CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     Profile* profile);
  virtual ~LocationBarViewMac();

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(bool use_OEM_bubble) { NOTIMPLEMENTED(); }
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition disposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdateContentBlockedIcons();
  virtual void UpdatePageActions();
  virtual void InvalidatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual AutocompleteEditView* location_entry() {
    return edit_view_.get();
  }
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Overridden from LocationBarTesting:
  virtual int PageActionCount();
  virtual int PageActionVisibleCount();
  virtual ExtensionAction* GetPageAction(size_t index);
  virtual ExtensionAction* GetVisiblePageAction(size_t index);
  virtual void TestPageActionPressed(size_t index);

  // Updates the location bar.  Resets the bar's permanent text and
  // security style, and if |should_restore_state| is true, restores
  // saved state from the tab (for tab switching).
  void Update(const TabContents* tab, bool should_restore_state);

  // Sets preview_enabled_ for the PageActionImageView associated with this
  // |page_action|. If |preview_enabled|, the location bar will display the
  // PageAction icon even if it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                   bool preview_enabled);

  // Return the index of a given page_action.
  size_t GetPageActionIndex(ExtensionAction* page_action);

  // PageActionImageView is nested in LocationBarViewMac, and only needed
  // here so that we can access the icon of a page action when preview_enabled_
  // has been set.
  class PageActionImageView;

  // Return the PageActionImageView associated with |page_action|.
  PageActionImageView* GetPageActionImageView(ExtensionAction* page_action);

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

  // Internals of OnChanged(), pulled out for purposes of unit
  // testing.  Sets up |field| based on the parameters, which are
  // pulled from edit_view->model().
  static void OnChangedImpl(AutocompleteTextField* field,
                            const std::wstring& keyword,
                            const std::wstring& short_name,
                            const bool is_keyword_hint,
                            const bool show_search_hint,
                            NSImage* image);

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
    void SetImage(SkBitmap* image);

    // Sets the label text, font, and color. |text| may be nil; |color| and
    // |font| are ignored if |text| is nil.
    void SetLabel(NSString* text, NSFont* baseFont, NSColor* color);

    // Sets the visibility. SetImage() should be called with a valid image
    // before the visibility is set to |true|.
    void SetVisible(bool visible);

    const NSImage* GetImage() const { return image_; }
    const NSAttributedString* GetLabel() const { return label_; }
    bool IsVisible() const { return visible_; }

   private:
    scoped_nsobject<NSImage> image_;

    // The label shown next to the icon, or nil if none.
    scoped_nsobject<NSAttributedString> label_;

    bool visible_;

    DISALLOW_COPY_AND_ASSIGN(LocationBarImageView);
  };

  // SecurityImageView is used to display the lock or warning icon when the
  // current URL's scheme is https.
  class SecurityImageView : public LocationBarImageView {
   public:
    enum Image {
      LOCK = 0,
      WARNING
    };

    SecurityImageView(Profile* profile, ToolbarModel* model);
    virtual ~SecurityImageView();

    // Sets the image to the appropriate icon.
    void SetImageShown(Image image);

    // Shows the page info dialog. Virtual so it can be overridden for testing.
    virtual bool OnMousePressed();

   private:
    // The lock icon shown when using HTTPS. Loaded lazily, the first time it's
    // needed.
    scoped_nsobject<NSImage> lock_icon_;

    // The warning icon shown when HTTPS is broken. Loaded lazily, the first
    // time it's needed.
    scoped_nsobject<NSImage> warning_icon_;

    Profile* profile_;
    ToolbarModel* model_;

    DISALLOW_COPY_AND_ASSIGN(SecurityImageView);
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

    bool preview_enabled() { return preview_enabled_; }

    // Return the size of the image, or a default size if no image available
    // and preview is enabled.
    virtual NSSize GetImageSize();

    // Either notify listeners or show a popup depending on the Page Action.
    // Virtual so it can be overridden for testing.
    virtual bool OnMousePressed(NSRect bounds);

    // Overridden from ImageLoadingTracker.
    virtual void OnImageLoaded(SkBitmap* image, size_t index);

    // Called to notify the Page Action that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, const GURL& url);

    // Sets the tooltip for this Page Action image.
    void SetToolTip(NSString* tooltip);
    void SetToolTip(std::string tooltip);

    // Returns the tooltip for this Page Action image or |nil| if there is none.
    const NSString* GetToolTip();

   protected:
    // For unit testing only.
    PageActionImageView() : owner_(NULL),
                            profile_(NULL),
                            page_action_(NULL),
                            tracker_(NULL),
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
    ImageLoadingTracker* tracker_;

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

  class PageActionViewList {
   public:
    PageActionViewList(LocationBarViewMac* location_bar,
                       Profile* profile,
                       ToolbarModel* toolbar_model)
        : owner_(location_bar),
          profile_(profile),
          toolbar_model_(toolbar_model) {}

    void DeleteAll();
    void RefreshViews();

    PageActionImageView* ViewAt(size_t index);

    size_t Count();
    size_t VisibleCount();

    // Called when the action at |index| is clicked. The |iconFrame| should
    // describe the bounds of the affected action's icon.
    void OnMousePressed(NSRect iconFrame, size_t index);

   protected:
    // Any installed Page Actions (weak references: owned by the extensions
    // service). Exposed for unit testing only.
    std::vector<PageActionImageView*> views_;

   private:
    // The location bar view that owns us.
    LocationBarViewMac* owner_;

    Profile* profile_;
    ToolbarModel* toolbar_model_;

    DISALLOW_COPY_AND_ASSIGN(PageActionViewList);
  };

 private:
  // Sets the SSL icon we should be showing.
  void SetSecurityIcon(ToolbarModel::Icon icon);

  // Sets the label for the SSL icon.
  void SetSecurityIconLabel();

  scoped_ptr<AutocompleteEditViewMac> edit_view_;

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  AutocompleteTextField* field_;  // owned by tab controller

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // The view that shows the lock/warning when in HTTPS mode.
  SecurityImageView security_image_view_;

  // Any installed Page Actions.
  PageActionViewList* page_action_views_;

  Profile* profile_;

  ToolbarModel* toolbar_model_;  // Weak, owned by Browser.

  // Image used in drawing keyword hint.
  scoped_nsobject<NSImage> tab_button_image_;

  // The transition type to use for the navigation.
  PageTransition::Type transition_;

  // Used to register for notifications received by NotificationObserver.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
