// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/content_settings_types.h"

@class AutocompleteTextField;
class CommandUpdater;
class ContentSettingDecoration;
class EVBubbleDecoration;
class KeywordHintDecoration;
class LocationBarDecoration;
class LocationBarViewMacBrowserTest;
class LocationIconDecoration;
class PageActionDecoration;
class PlusDecoration;
class Profile;
class SearchTokenDecoration;
class SelectedKeywordDecoration;
class SeparatorDecoration;
class StarDecoration;
class ToolbarModel;
class ZoomDecoration;
class ZoomDecorationTest;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an OmniboxViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public LocationBar,
                           public LocationBarTesting,
                           public OmniboxEditController,
                           public content::NotificationObserver {
 public:
  LocationBarViewMac(AutocompleteTextField* field,
                     CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     Profile* profile,
                     Browser* browser);
  virtual ~LocationBarViewMac();

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble() OVERRIDE;
  virtual void SetInstantSuggestion(
      const InstantSuggestion& suggestion) OVERRIDE;
  virtual string16 GetInputString() const OVERRIDE;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const OVERRIDE;
  virtual content::PageTransition GetPageTransition() const OVERRIDE;
  virtual void AcceptInput() OVERRIDE;
  virtual void FocusLocation(bool select_all) OVERRIDE;
  virtual void FocusSearch() OVERRIDE;
  virtual void UpdateContentSettingsIcons() OVERRIDE;
  virtual void UpdatePageActions() OVERRIDE;
  virtual void InvalidatePageActions() OVERRIDE;
  virtual void UpdateOpenPDFInReaderPrompt() OVERRIDE;
  virtual void SaveStateToContents(content::WebContents* contents) OVERRIDE;
  virtual void Revert() OVERRIDE;
  virtual const OmniboxView* GetLocationEntry() const OVERRIDE;
  virtual OmniboxView* GetLocationEntry() OVERRIDE;
  virtual LocationBarTesting* GetLocationBarForTesting() OVERRIDE;

  // Overridden from LocationBarTesting:
  virtual int PageActionCount() OVERRIDE;
  virtual int PageActionVisibleCount() OVERRIDE;
  virtual ExtensionAction* GetPageAction(size_t index) OVERRIDE;
  virtual ExtensionAction* GetVisiblePageAction(size_t index) OVERRIDE;
  virtual void TestPageActionPressed(size_t index) OVERRIDE;
  virtual void TestActionBoxMenuItemSelected(int command_id) OVERRIDE;
  virtual bool GetBookmarkStarVisibility() OVERRIDE;

  // Set/Get the editable state of the field.
  void SetEditable(bool editable);
  bool IsEditable();

  // Set the starred state of the bookmark star.
  void SetStarred(bool starred);

  // Set (or resets) the icon image resource for the action box plus decoration.
  void ResetActionBoxIcon();
  void SetActionBoxIcon(int image_id);

  // Happens when the zoom changes for the active tab. |can_show_bubble| is
  // false when the change in zoom for the active tab wasn't an explicit user
  // action (e.g. switching tabs, creating a new tab, creating a new browser).
  // Additionally, |can_show_bubble| will only be true when the bubble wouldn't
  // be obscured by other UI (wrench menu) or redundant (+/- from wrench).
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // Get the point in window coordinates on the star for the bookmark bubble to
  // aim at.
  NSPoint GetBookmarkBubblePoint() const;

  // Get the point in window coordinates on the Action Box icon for
  // anchoring its bubbles.
  NSPoint GetActionBoxAnchorPoint() const;

  // Get the point in window coordinates in the security icon at which the page
  // info bubble aims.
  NSPoint GetPageInfoBubblePoint() const;

  // When any image decorations change, call this to ensure everything is
  // redrawn and laid out if necessary.
  void OnDecorationsChanged();

  // Updates the location bar.  Resets the bar's permanent text and
  // security style, and if |should_restore_state| is true, restores
  // saved state from the tab (for tab switching).
  void Update(const content::WebContents* tab, bool should_restore_state);

  // Layout the various decorations which live in the field.
  void Layout();

  // Re-draws |decoration| if it's already being displayed.
  void RedrawDecoration(LocationBarDecoration* decoration);

  // Sets preview_enabled_ for the PageActionImageView associated with this
  // |page_action|. If |preview_enabled|, the location bar will display the
  // PageAction icon even if it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                   bool preview_enabled);

  // Retrieve the frame for the given |page_action|.
  NSRect GetPageActionFrame(ExtensionAction* page_action);

  // Return |page_action|'s info-bubble point in window coordinates.
  // This function should always be called with a visible page action.
  // If |page_action| is not a page action or not visible, NOTREACHED()
  // is called and this function returns |NSZeroPoint|.
  NSPoint GetPageActionBubblePoint(ExtensionAction* page_action);

  // Get the blocked-popup content setting's frame in window
  // coordinates.  Used by the blocked-popup animation.  Returns
  // |NSZeroRect| if the relevant content setting decoration is not
  // visible.
  NSRect GetBlockedPopupRect() const;

  // OmniboxEditController:
  virtual void OnAutocompleteAccept(
      const GURL& url,
      WindowOpenDisposition disposition,
      content::PageTransition transition,
      const GURL& alternate_nav_url) OVERRIDE;
  virtual void OnChanged() OVERRIDE;
  virtual void OnSelectionBoundsChanged() OVERRIDE;
  virtual void OnInputInProgress(bool in_progress) OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual gfx::Image GetFavicon() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

  NSImage* GetKeywordImage(const string16& keyword);

  AutocompleteTextField* GetAutocompleteTextField() { return field_; }


  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* browser() const { return browser_; }
  ToolbarModel* toolbar_model() const { return toolbar_model_; }

 private:
  friend LocationBarViewMacBrowserTest;
  friend ZoomDecorationTest;

  // Posts |notification| to the default notification center.
  void PostNotification(NSString* notification);

  // Return the decoration for |page_action|.
  PageActionDecoration* GetPageActionDecoration(ExtensionAction* page_action);

  // Clear the page-action decorations.
  void DeletePageActionDecorations();

  void OnEditBookmarksEnabledChanged();

  // Re-generate the page-action decorations from the profile's
  // extension service.
  void RefreshPageActionDecorations();

  // Updates visibility of the content settings icons based on the current
  // tab contents state.
  bool RefreshContentSettingsDecorations();

  void ShowFirstRunBubbleInternal();

  // Checks if the bookmark star should be enabled or not.
  bool IsStarEnabled();

  // Updates the zoom decoration in the omnibox with the current zoom level.
  void UpdateZoomDecoration();

  // Ensures the star decoration is visible or hidden, as required.
  void UpdateStarDecorationVisibility();

  // Ensures the plus decoration is visible or hidden, as required.
  void UpdatePlusDecorationVisibility();

  // Gets the current search provider name.
  string16 GetSearchProviderName() const;

  scoped_ptr<OmniboxViewMac> omnibox_view_;

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  AutocompleteTextField* field_;  // owned by tab controller

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  string16 location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // A decoration that shows an icon to the left of the address.
  scoped_ptr<LocationIconDecoration> location_icon_decoration_;

  // A decoration that shows the search provider being used.
  scoped_ptr<SearchTokenDecoration> search_token_decoration_;

  // A decoration that shows the keyword-search bubble on the left.
  scoped_ptr<SelectedKeywordDecoration> selected_keyword_decoration_;

  // A decoration used to draw a separator between other decorations.
  scoped_ptr<SeparatorDecoration> separator_decoration_;

  // A decoration that shows a lock icon and ev-cert label in a bubble
  // on the left.
  scoped_ptr<EVBubbleDecoration> ev_bubble_decoration_;

  // Action "plus" button right of bookmark star.
  scoped_ptr<PlusDecoration> plus_decoration_;

  // Bookmark star right of page actions.
  scoped_ptr<StarDecoration> star_decoration_;

  // A zoom icon at the end of the omnibox, which shows at non-standard zoom
  // levels.
  scoped_ptr<ZoomDecoration> zoom_decoration_;

  // The installed page actions.
  std::vector<ExtensionAction*> page_actions_;

  // Decorations for the installed Page Actions.
  ScopedVector<PageActionDecoration> page_action_decorations_;

  // The content blocked decorations.
  ScopedVector<ContentSettingDecoration> content_setting_decorations_;

  // Keyword hint decoration displayed on the right-hand side.
  scoped_ptr<KeywordHintDecoration> keyword_hint_decoration_;

  Profile* profile_;

  Browser* browser_;

  ToolbarModel* toolbar_model_;  // Weak, owned by Browser.

  // The transition type to use for the navigation.
  content::PageTransition transition_;

  // Used to register for notifications received by NotificationObserver.
  content::NotificationRegistrar registrar_;

  // Used to schedule a task for the first run info bubble.
  base::WeakPtrFactory<LocationBarViewMac> weak_ptr_factory_;

  // Used to change the visibility of the star decoration.
  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
