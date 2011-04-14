// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_VIEW_MAC_H_
#pragma once

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/content_settings_types.h"

@class AutocompleteTextField;
class CommandUpdater;
class ContentSettingDecoration;
class ContentSettingImageModel;
class EVBubbleDecoration;
@class ExtensionPopupController;
class KeywordHintDecoration;
class LocationIconDecoration;
class PageActionDecoration;
class Profile;
class SelectedKeywordDecoration;
class SkBitmap;
class StarDecoration;
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
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior);
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
  virtual const AutocompleteEditView* location_entry() const;
  virtual AutocompleteEditView* location_entry();
  virtual LocationBarTesting* GetLocationBarForTesting();

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

  // Get the point in the security icon at which the page info bubble aims.
  NSPoint GetPageInfoBubblePoint() const;

  // Get the point in the omnibox at which the first run bubble aims.
  NSPoint GetFirstRunBubblePoint() const;

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
  // This function should always be called with a visible page action.
  // If |page_action| is not a page action or not visible, NOTREACHED()
  // is called and this function returns |NSZeroPoint|.
  NSPoint GetPageActionBubblePoint(ExtensionAction* page_action);

  // Get the blocked-popup content setting's frame in window
  // coordinates.  Used by the blocked-popup animation.  Returns
  // |NSZeroRect| if the relevant content setting decoration is not
  // visible.
  NSRect GetBlockedPopupRect() const;

  // AutocompleteEditController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnSelectionBoundsChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual void OnKillFocus();
  virtual void OnSetFocus();
  virtual SkBitmap GetFavicon() const;
  virtual string16 GetTitle() const;
  virtual InstantController* GetInstant();
  virtual TabContentsWrapper* GetTabContentsWrapper() const;

  NSImage* GetKeywordImage(const string16& keyword);

  AutocompleteTextField* GetAutocompleteTextField() { return field_; }


  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Posts |notification| to the default notification center.
  void PostNotification(NSString* notification);

  // Return the decoration for |page_action|.
  PageActionDecoration* GetPageActionDecoration(ExtensionAction* page_action);

  // Clear the page-action decorations.
  void DeletePageActionDecorations();

  // Re-generate the page-action decorations from the profile's
  // extension service.
  void RefreshPageActionDecorations();

  // Updates visibility of the content settings icons based on the current
  // tab contents state.
  bool RefreshContentSettingsDecorations();

  void ShowFirstRunBubbleInternal(FirstRun::BubbleType bubble_type);

  // Checks if the bookmark star should be enabled or not.
  bool IsStarEnabled();

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
  scoped_ptr<StarDecoration> star_decoration_;

  // Any installed Page Actions.
  ScopedVector<PageActionDecoration> page_action_decorations_;

  // The content blocked decorations.
  ScopedVector<ContentSettingDecoration> content_setting_decorations_;

  // Keyword hint decoration displayed on the right-hand side.
  scoped_ptr<KeywordHintDecoration> keyword_hint_decoration_;

  Profile* profile_;

  Browser* browser_;

  ToolbarModel* toolbar_model_;  // Weak, owned by Browser.

  // The transition type to use for the navigation.
  PageTransition::Type transition_;

  // Used to register for notifications received by NotificationObserver.
  NotificationRegistrar registrar_;

  // Used to schedule a task for the first run info bubble.
  ScopedRunnableMethodFactory<LocationBarViewMac> first_run_bubble_;

  // Used to change the visibility of the star decoration.
  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_VIEW_MAC_H_
