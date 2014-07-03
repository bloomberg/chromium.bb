// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/common/content_settings_types.h"

@class AutocompleteTextField;
class CommandUpdater;
class ContentSettingDecoration;
class EVBubbleDecoration;
class GeneratedCreditCardDecoration;
class KeywordHintDecoration;
class LocationBarDecoration;
class LocationIconDecoration;
class MicSearchDecoration;
class OriginChipDecoration;
class PageActionDecoration;
class Profile;
class SearchButtonDecoration;
class SelectedKeywordDecoration;
class StarDecoration;
class TranslateDecoration;
class ZoomDecoration;
class ZoomDecorationTest;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an OmniboxViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public LocationBar,
                           public LocationBarTesting,
                           public OmniboxEditController,
                           public content::NotificationObserver,
                           public SearchModelObserver {
 public:
  LocationBarViewMac(AutocompleteTextField* field,
                     CommandUpdater* command_updater,
                     Profile* profile,
                     Browser* browser);
  virtual ~LocationBarViewMac();

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble() OVERRIDE;
  virtual GURL GetDestinationURL() const OVERRIDE;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const OVERRIDE;
  virtual content::PageTransition GetPageTransition() const OVERRIDE;
  virtual void AcceptInput() OVERRIDE;
  virtual void FocusLocation(bool select_all) OVERRIDE;
  virtual void FocusSearch() OVERRIDE;
  virtual void UpdateContentSettingsIcons() OVERRIDE;
  virtual void UpdateManagePasswordsIconAndBubble() OVERRIDE {};
  virtual void UpdatePageActions() OVERRIDE;
  virtual void InvalidatePageActions() OVERRIDE;
  virtual void UpdateOpenPDFInReaderPrompt() OVERRIDE;
  virtual void UpdateGeneratedCreditCardView() OVERRIDE;
  virtual void SaveStateToContents(content::WebContents* contents) OVERRIDE;
  virtual void Revert() OVERRIDE;
  virtual const OmniboxView* GetOmniboxView() const OVERRIDE;
  virtual OmniboxView* GetOmniboxView() OVERRIDE;
  virtual LocationBarTesting* GetLocationBarForTesting() OVERRIDE;

  // Overridden from LocationBarTesting:
  virtual int PageActionCount() OVERRIDE;
  virtual int PageActionVisibleCount() OVERRIDE;
  virtual ExtensionAction* GetPageAction(size_t index) OVERRIDE;
  virtual ExtensionAction* GetVisiblePageAction(size_t index) OVERRIDE;
  virtual void TestPageActionPressed(size_t index) OVERRIDE;
  virtual bool GetBookmarkStarVisibility() OVERRIDE;

  // Set/Get the editable state of the field.
  void SetEditable(bool editable);
  bool IsEditable();

  // Set the starred state of the bookmark star.
  void SetStarred(bool starred);

  // Set whether or not the translate icon is lit.
  void SetTranslateIconLit(bool on);

  // Happens when the zoom changes for the active tab. |can_show_bubble| is
  // false when the change in zoom for the active tab wasn't an explicit user
  // action (e.g. switching tabs, creating a new tab, creating a new browser).
  // Additionally, |can_show_bubble| will only be true when the bubble wouldn't
  // be obscured by other UI (wrench menu) or redundant (+/- from wrench).
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // Checks if the bookmark star should be enabled or not.
  bool IsStarEnabled() const;

  // Get the point in window coordinates on the star for the bookmark bubble to
  // aim at. Only works if IsStarEnabled returns YES.
  NSPoint GetBookmarkBubblePoint() const;

  // Get the point in window coordinates on the star for the Translate bubble to
  // aim at.
  NSPoint GetTranslateBubblePoint() const;

  // Get the point in window coordinates in the security icon at which the page
  // info bubble aims.
  NSPoint GetPageInfoBubblePoint() const;

  // Get the point in window coordinates in the "generated cc" icon at which the
  // corresponding info bubble aims.
  NSPoint GetGeneratedCreditCardBubblePoint() const;

  // When any image decorations change, call this to ensure everything is
  // redrawn and laid out if necessary.
  void OnDecorationsChanged();

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

  // OmniboxEditController:
  virtual void Update(const content::WebContents* contents) OVERRIDE;
  virtual void OnChanged() OVERRIDE;
  virtual void OnSetFocus() OVERRIDE;
  virtual void ShowURL() OVERRIDE;
  virtual void EndOriginChipAnimations(bool cancel_fade) OVERRIDE;
  virtual InstantController* GetInstant() OVERRIDE;
  virtual content::WebContents* GetWebContents() OVERRIDE;
  virtual ToolbarModel* GetToolbarModel() OVERRIDE;
  virtual const ToolbarModel* GetToolbarModel() const OVERRIDE;

  NSImage* GetKeywordImage(const base::string16& keyword);

  AutocompleteTextField* GetAutocompleteTextField() { return field_; }


  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  Browser* browser() const { return browser_; }

  // Activates the page action for the extension that has the given id.
  void ActivatePageAction(const std::string& extension_id);

 protected:
  // OmniboxEditController:
  virtual void HideURL() OVERRIDE;

 private:
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

  // Updates the translate decoration in the omnibox with the current translate
  // state.
  void UpdateTranslateDecoration();

  // Updates the zoom decoration in the omnibox with the current zoom level.
  void UpdateZoomDecoration();

  // Ensures the star decoration is visible or hidden, as required.
  void UpdateStarDecorationVisibility();

  // Updates the voice search decoration. Returns true if the visible state was
  // changed.
  bool UpdateMicSearchDecorationVisibility();

  scoped_ptr<OmniboxViewMac> omnibox_view_;

  AutocompleteTextField* field_;  // owned by tab controller

  // A decoration that shows an icon to the left of the address.
  scoped_ptr<LocationIconDecoration> location_icon_decoration_;

  // A decoration that shows the keyword-search bubble on the left.
  scoped_ptr<SelectedKeywordDecoration> selected_keyword_decoration_;

  // A decoration that shows a lock icon and ev-cert label in a bubble
  // on the left.
  scoped_ptr<EVBubbleDecoration> ev_bubble_decoration_;

  // Bookmark star right of page actions.
  scoped_ptr<StarDecoration> star_decoration_;

  // Translate icon at the end of the ominibox.
  scoped_ptr<TranslateDecoration> translate_decoration_;

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

  // The voice search icon.
  scoped_ptr<MicSearchDecoration> mic_search_decoration_;

  // Generated CC hint decoration.
  scoped_ptr<GeneratedCreditCardDecoration> generated_credit_card_decoration_;

  // The right-hand-side search button that is shown on search result pages.
  scoped_ptr<SearchButtonDecoration> search_button_decoration_;

  // The left-hand-side origin chip.
  scoped_ptr<OriginChipDecoration> origin_chip_decoration_;

  Browser* browser_;

  // Used to register for notifications received by NotificationObserver.
  content::NotificationRegistrar registrar_;

  // Used to schedule a task for the first run info bubble.
  base::WeakPtrFactory<LocationBarViewMac> weak_ptr_factory_;

  // Used to change the visibility of the star decoration.
  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
