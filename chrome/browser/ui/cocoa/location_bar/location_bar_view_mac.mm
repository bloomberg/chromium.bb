// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/tabs/tabs.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/first_run_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/keyword_hint_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/web_intents_button_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#import "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

// Vertical space between the bottom edge of the location_bar and the first run
// bubble arrow point.
const static int kFirstRunBubbleYOffset = 1;

}

// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

LocationBarViewMac::LocationBarViewMac(
    AutocompleteTextField* field,
    CommandUpdater* command_updater,
    ToolbarModel* toolbar_model,
    Profile* profile,
    Browser* browser)
    : omnibox_view_(new OmniboxViewMac(this, toolbar_model, profile,
                                       command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(
          new SelectedKeywordDecoration(OmniboxViewMac::GetFieldFont())),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get(),
                                 OmniboxViewMac::GetFieldFont())),
      plus_decoration_(NULL),
      star_decoration_(new StarDecoration(command_updater)),
      zoom_decoration_(new ZoomDecoration(toolbar_model)),
      keyword_hint_decoration_(
          new KeywordHintDecoration(OmniboxViewMac::GetFieldFont())),
      web_intents_button_decoration_(
          new WebIntentsButtonDecoration(this, OmniboxViewMac::GetFieldFont())),
      profile_(profile),
      browser_(browser),
      toolbar_model_(toolbar_model),
      transition_(content::PageTransitionFromInt(
          content::PAGE_TRANSITION_TYPED |
          content::PAGE_TRANSITION_FROM_ADDRESS_BAR)),
      weak_ptr_factory_(this) {
  if (extensions::FeatureSwitch::action_box()->IsEnabled()) {
    plus_decoration_.reset(new PlusDecoration(this, browser_));
  }

  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    DCHECK_EQ(i, content_setting_decorations_.size());
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    content_setting_decorations_.push_back(
        new ContentSettingDecoration(type, this, profile_));
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  web_intents_button_decoration_->SetButtonImages(
      rb.GetNativeImageNamed(IDR_OMNIBOX_WI_BUBBLE_BACKGROUND_L).ToNSImage(),
      rb.GetNativeImageNamed(IDR_OMNIBOX_WI_BUBBLE_BACKGROUND_C).ToNSImage(),
      rb.GetNativeImageNamed(IDR_OMNIBOX_WI_BUBBLE_BACKGROUND_R).ToNSImage());

  registrar_.Add(this,
      chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this,
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(browser_->profile()));

  edit_bookmarks_enabled_.Init(
      prefs::kEditBookmarksEnabled,
      profile_->GetPrefs(),
      base::Bind(&LocationBarViewMac::OnEditBookmarksEnabledChanged,
                 base::Unretained(this)));
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];
}

void LocationBarViewMac::ShowFirstRunBubble() {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&LocationBarViewMac::ShowFirstRunBubbleInternal,
          weak_ptr_factory_.GetWeakPtr()));
}

void LocationBarViewMac::ShowFirstRunBubbleInternal() {
  if (!field_ || ![field_ window])
    return;

  // The first run bubble's left edge should line up with the left edge of the
  // omnibox. This is different from other bubbles, which line up at a point
  // set by their top arrow. Because the BaseBubbleController adjusts the
  // window origin left to account for the arrow spacing, the first run bubble
  // moves the window origin right by this spacing, so that the
  // BaseBubbleController will move it back to the correct position.
  const NSPoint kOffset = NSMakePoint(
      info_bubble::kBubbleArrowXOffset + info_bubble::kBubbleArrowWidth/2.0,
      kFirstRunBubbleYOffset);
  [FirstRunBubbleController showForView:field_
                                 offset:kOffset
                                browser:browser_
                                profile:profile_];
}

string16 LocationBarViewMac::GetInputString() const {
  return location_input_;
}

void LocationBarViewMac::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  omnibox_view_->model()->SetInstantSuggestion(suggestion);
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition_;
}

content::PageTransition LocationBarViewMac::GetPageTransition() const {
  return transition_;
}

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  omnibox_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  omnibox_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  omnibox_view_->SetForcedQuery();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
  if (RefreshContentSettingsDecorations()) {
    [field_ updateMouseTracking];
    [field_ setNeedsDisplay:YES];
  }
}

void LocationBarViewMac::UpdatePageActions() {
  size_t count_before = page_action_decorations_.size();
  RefreshPageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }
}

void LocationBarViewMac::InvalidatePageActions() {
  size_t count_before = page_action_decorations_.size();
  DeletePageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }
}

void LocationBarViewMac::UpdateWebIntentsButton() {
  RefreshWebIntentsButtonDecoration();
}

void LocationBarViewMac::UpdateOpenPDFInReaderPrompt() {
  // Not implemented on Mac.
}

void LocationBarViewMac::SaveStateToContents(WebContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  omnibox_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Update(const WebContents* contents,
                                bool should_restore_state) {
  command_updater_->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, IsStarEnabled());
  UpdateStarDecorationVisibility();
  UpdatePlusDecorationVisibility();
  UpdateZoomDecoration();
  RefreshPageActionDecorations();
  RefreshContentSettingsDecorations();
  RefreshWebIntentsButtonDecoration();
  // OmniboxView restores state if the tab is non-NULL.
  omnibox_view_->Update(should_restore_state ? contents : NULL);
  OnChanged();
}

void LocationBarViewMac::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    content::PageTransition transition,
    const GURL& alternate_nav_url) {
  // WARNING: don't add an early return here. The calls after the if must
  // happen.
  if (url.is_valid()) {
    location_input_ = UTF8ToUTF16(url.spec());
    disposition_ = disposition;
    transition_ = content::PageTransitionFromInt(
        transition | content::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    if (command_updater_) {
      if (!alternate_nav_url.is_valid()) {
        command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
      } else {
        AlternateNavURLFetcher* fetcher =
            new AlternateNavURLFetcher(alternate_nav_url);
        // The AlternateNavURLFetcher will listen for the pending navigation
        // notification that will be issued as a result of the "open URL." It
        // will automatically install itself into that navigation controller.
        command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
        if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
          // I'm not sure this should be reachable, but I'm not also sure enough
          // that it shouldn't to stick in a NOTREACHED().  In any case, this is
          // harmless.
          delete fetcher;
        } else {
          // The navigation controller will delete the fetcher.
        }
      }
    }
  }
}

void LocationBarViewMac::OnChanged() {
  // Update the location-bar icon.
  const int resource_id = omnibox_view_->GetIcon();
  NSImage* image = OmniboxViewMac::ImageForResource(resource_id);
  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();
}

void LocationBarViewMac::OnSelectionBoundsChanged() {
  NOTIMPLEMENTED();
}

void LocationBarViewMac::OnInputInProgress(bool in_progress) {
  toolbar_model_->SetInputInProgress(in_progress);
  Update(NULL, false);
}

void LocationBarViewMac::OnSetFocus() {
  // Update the keyword and search hint states.
  OnChanged();
}

void LocationBarViewMac::OnKillFocus() {
  // Do nothing.
}

gfx::Image LocationBarViewMac::GetFavicon() const {
  return browser_->GetCurrentPageIcon();
}

string16 LocationBarViewMac::GetTitle() const {
  return browser_->GetWindowTitleForCurrentTab();
}

InstantController* LocationBarViewMac::GetInstant() {
  return browser_->instant_controller() ?
      browser_->instant_controller()->instant() : NULL;
}

void LocationBarViewMac::Revert() {
  omnibox_view_->RevertAll();
}

const OmniboxView* LocationBarViewMac::GetLocationEntry() const {
  return omnibox_view_.get();
}

OmniboxView* LocationBarViewMac::GetLocationEntry() {
  return omnibox_view_.get();
}

LocationBarTesting* LocationBarViewMac::GetLocationBarForTesting() {
  return this;
}

// TODO(pamg): Change all these, here and for other platforms, to size_t.
int LocationBarViewMac::PageActionCount() {
  return static_cast<int>(page_action_decorations_.size());
}

int LocationBarViewMac::PageActionVisibleCount() {
  int result = 0;
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->IsVisible())
      ++result;
  }
  return result;
}

WebContents* LocationBarViewMac::GetWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

PageActionDecoration* LocationBarViewMac::GetPageActionDecoration(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->page_action() == page_action)
      return page_action_decorations_[i];
  }
  // If |page_action| is the browser action of an extension, no element in
  // |page_action_decorations_| will match.
  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::SetPreviewEnabledPageAction(
    ExtensionAction* page_action, bool preview_enabled) {
  DCHECK(page_action);
  WebContents* contents = GetWebContents();
  if (!contents)
    return;
  RefreshPageActionDecorations();
  Layout();

  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  DCHECK(decoration);
  if (!decoration)
    return;

  decoration->set_preview_enabled(preview_enabled);
  decoration->UpdateVisibility(contents, toolbar_model_->GetURL());
}

NSRect LocationBarViewMac::GetPageActionFrame(ExtensionAction* page_action) {
  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  if (!decoration)
    return NSZeroRect;

  AutocompleteTextFieldCell* cell = [field_ cell];
  NSRect frame = [cell frameForDecoration:decoration inFrame:[field_ bounds]];
  return frame;
}

NSPoint LocationBarViewMac::GetPageActionBubblePoint(
    ExtensionAction* page_action) {
  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  if (!decoration)
    return NSZeroPoint;

  NSRect frame = GetPageActionFrame(page_action);
  if (NSIsEmptyRect(frame)) {
    // The bubble point positioning assumes that the page action is visible. If
    // not, something else needs to be done otherwise the bubble will appear
    // near the top left corner (unanchored).
    NOTREACHED();
    return NSZeroPoint;
  }

  NSPoint bubble_point = decoration->GetBubblePointInFrame(frame);
  return [field_ convertPoint:bubble_point toView:nil];
}

NSRect LocationBarViewMac::GetBlockedPopupRect() const {
  const size_t kPopupIndex = CONTENT_SETTINGS_TYPE_POPUPS;
  const LocationBarDecoration* decoration =
      content_setting_decorations_[kPopupIndex];
  if (!decoration || !decoration->IsVisible())
    return NSZeroRect;

  AutocompleteTextFieldCell* cell = [field_ cell];
  const NSRect frame = [cell frameForDecoration:decoration
                                        inFrame:[field_ bounds]];
  return [field_ convertRect:frame toView:nil];
}

ExtensionAction* LocationBarViewMac::GetPageAction(size_t index) {
  if (index < page_action_decorations_.size())
    return page_action_decorations_[index]->page_action();
  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarViewMac::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->IsVisible()) {
      if (current == index)
        return page_action_decorations_[i]->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::TestPageActionPressed(size_t index) {
  DCHECK_LT(index, page_action_decorations_.size());
  if (index < page_action_decorations_.size())
    page_action_decorations_[index]->OnMousePressed(NSZeroRect);
}

void LocationBarViewMac::TestActionBoxMenuItemSelected(int command_id) {
  plus_decoration_->action_box_button_controller()->ExecuteCommand(command_id);
}

bool LocationBarViewMac::GetBookmarkStarVisibility() {
  DCHECK(star_decoration_.get());
  return star_decoration_->IsVisible();
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  UpdateStarDecorationVisibility();
  UpdatePlusDecorationVisibility();
  UpdateZoomDecoration();
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::OnDecorationsChanged() {
  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  [field_ updateMouseTracking];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
}

void LocationBarViewMac::SetStarred(bool starred) {
  star_decoration_->SetStarred(starred);
  UpdateStarDecorationVisibility();
  OnDecorationsChanged();
}

void LocationBarViewMac::ResetActionBoxIcon() {
  plus_decoration_->ResetIcon();
  OnDecorationsChanged();
}

void LocationBarViewMac::SetActionBoxIcon(int image_id) {
  plus_decoration_->SetTemporaryIcon(image_id);
  OnDecorationsChanged();
}

void LocationBarViewMac::ZoomChangedForActiveTab(bool can_show_bubble) {
  UpdateZoomDecoration();
  OnDecorationsChanged();

  // TODO(dbeam): show a zoom bubble when |can_show_bubble| is true, the zoom
  // decoration is showing, and the wrench menu isn't showing.
}

NSPoint LocationBarViewMac::GetActionBoxAnchorPoint() const {
  return plus_decoration_->GetActionBoxAnchorPoint();
}

NSPoint LocationBarViewMac::GetBookmarkBubblePoint() const {
  AutocompleteTextFieldCell* cell = [field_ cell];
  const NSRect frame = [cell frameForDecoration:star_decoration_.get()
                                        inFrame:[field_ bounds]];
  const NSPoint point = star_decoration_->GetBubblePointInFrame(frame);
  return [field_ convertPoint:point toView:nil];
}

NSPoint LocationBarViewMac::GetPageInfoBubblePoint() const {
  AutocompleteTextFieldCell* cell = [field_ cell];
  if (ev_bubble_decoration_->IsVisible()) {
    const NSRect frame = [cell frameForDecoration:ev_bubble_decoration_.get()
                                          inFrame:[field_ bounds]];
    const NSPoint point = ev_bubble_decoration_->GetBubblePointInFrame(frame);
    return [field_ convertPoint:point toView:nil];
  } else {
    const NSRect frame =
        [cell frameForDecoration:location_icon_decoration_.get()
                         inFrame:[field_ bounds]];
    const NSPoint point =
        location_icon_decoration_->GetBubblePointInFrame(frame);
    return [field_ convertPoint:point toView:nil];
  }
}

NSImage* LocationBarViewMac::GetKeywordImage(const string16& keyword) {
  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      profile_)->GetTemplateURLForKeyword(keyword);
  if (template_url && template_url->IsExtensionKeyword()) {
    return extensions::OmniboxAPI::Get(profile_)->
        GetOmniboxIcon(template_url->GetExtensionId()).AsNSImage();
  }

  return OmniboxViewMac::ImageForResource(IDR_OMNIBOX_SEARCH);
}

void LocationBarViewMac::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
      WebContents* contents = GetWebContents();
      if (content::Details<WebContents>(contents) != details)
        return;

      [field_ updateMouseTracking];
      [field_ setNeedsDisplay:YES];
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED: {
      // Only update if the updated action box was for the active tab contents.
      WebContents* target_tab = content::Details<WebContents>(details).ptr();
      if (target_tab == GetWebContents())
        UpdatePageActions();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

void LocationBarViewMac::OnEditBookmarksEnabledChanged() {
  UpdateStarDecorationVisibility();
  OnChanged();
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
}

bool LocationBarViewMac::RefreshContentSettingsDecorations() {
  const bool input_in_progress = toolbar_model_->GetInputInProgress();
  WebContents* web_contents = input_in_progress ?
      NULL : browser_->tab_strip_model()->GetActiveWebContents();
  bool icons_updated = false;
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    icons_updated |=
        content_setting_decorations_[i]->UpdateFromWebContents(web_contents);
  }
  return icons_updated;
}

void LocationBarViewMac::DeletePageActionDecorations() {
  // TODO(shess): Deleting these decorations could result in the cell
  // refering to them before things are laid out again.  Meanwhile, at
  // least fail safe.
  [[field_ cell] clearDecorations];

  page_action_decorations_.clear();
}

void LocationBarViewMac::RefreshPageActionDecorations() {
  if (!IsEditable()) {
    DeletePageActionDecorations();
    return;
  }

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    DeletePageActionDecorations();  // Necessary?
    return;
  }

  std::vector<ExtensionAction*> new_page_actions =
      extensions::TabHelper::FromWebContents(web_contents)->
          location_bar_controller()->GetCurrentActions();

  if (new_page_actions != page_actions_) {
    page_actions_.swap(new_page_actions);
    DeletePageActionDecorations();
    for (size_t i = 0; i < page_actions_.size(); ++i) {
      page_action_decorations_.push_back(
          new PageActionDecoration(this, browser_, page_actions_[i]));
    }
  }

  GURL url = toolbar_model_->GetURL();
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    page_action_decorations_[i]->UpdateVisibility(
        toolbar_model_->GetInputInProgress() ? NULL : web_contents,
        url);
  }
}

void LocationBarViewMac::RefreshWebIntentsButtonDecoration() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    web_intents_button_decoration_->SetVisible(false);
    return;
  }

  web_intents_button_decoration_->Update(web_contents);
}

// TODO(shess): This function should over time grow to closely match
// the views Layout() function.
void LocationBarViewMac::Layout() {
  AutocompleteTextFieldCell* cell = [field_ cell];

  // Reset the left-hand decorations.
  // TODO(shess): Shortly, this code will live somewhere else, like in
  // the constructor.  I am still wrestling with how best to deal with
  // right-hand decorations, which are not a static set.
  [cell clearDecorations];
  [cell addLeftDecoration:location_icon_decoration_.get()];
  [cell addLeftDecoration:selected_keyword_decoration_.get()];
  [cell addLeftDecoration:ev_bubble_decoration_.get()];
  if (plus_decoration_.get())
    [cell addRightDecoration:plus_decoration_.get()];
  [cell addRightDecoration:star_decoration_.get()];
  // TODO(dbeam): uncomment when zoom bubble exists.
  // [cell addRightDecoration:zoom_decoration_.get()];

  // Note that display order is right to left.
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    [cell addRightDecoration:page_action_decorations_[i]];
  }
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    [cell addRightDecoration:content_setting_decorations_[i]];
  }

  [cell addRightDecoration:keyword_hint_decoration_.get()];

  [cell addRightDecoration:web_intents_button_decoration_.get()];

  // By default only the location icon is visible.
  location_icon_decoration_->SetVisible(true);
  selected_keyword_decoration_->SetVisible(false);
  ev_bubble_decoration_->SetVisible(false);
  keyword_hint_decoration_->SetVisible(false);

  // Get the keyword to use for keyword-search and hinting.
  const string16 keyword = omnibox_view_->model()->keyword();
  string16 short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = TemplateURLServiceFactory::GetForProfile(profile_)->
        GetKeywordShortName(keyword, &is_extension_keyword);
  }

  const bool is_keyword_hint = omnibox_view_->model()->is_keyword_hint();

  if (!keyword.empty() && !is_keyword_hint) {
    // Switch from location icon to keyword mode.
    location_icon_decoration_->SetVisible(false);
    selected_keyword_decoration_->SetVisible(true);
    selected_keyword_decoration_->SetKeyword(short_name, is_extension_keyword);
    selected_keyword_decoration_->SetImage(GetKeywordImage(keyword));
  } else if (toolbar_model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    string16 label(toolbar_model_->GetEVCertName());
    ev_bubble_decoration_->SetFullLabel(base::SysUTF16ToNSString(label));
  } else if (!keyword.empty() && is_keyword_hint) {
    keyword_hint_decoration_->SetKeyword(short_name,
                                         is_extension_keyword);
    keyword_hint_decoration_->SetVisible(true);
  }

  // These need to change anytime the layout changes.
  // TODO(shess): Anytime the field editor might have changed, the
  // cursor rects almost certainly should have changed.  The tooltips
  // might change even when the rects don't change.
  OnDecorationsChanged();
}

void LocationBarViewMac::RedrawDecoration(LocationBarDecoration* decoration) {
  AutocompleteTextFieldCell* cell = [field_ cell];
  NSRect frame = [cell frameForDecoration:decoration
                                  inFrame:[field_ bounds]];
  if (!NSIsEmptyRect(frame))
    [field_ setNeedsDisplayInRect:frame];
}

bool LocationBarViewMac::IsStarEnabled() {
  return [field_ isEditable] &&
         browser_defaults::bookmarks_enabled &&
         !toolbar_model_->GetInputInProgress() &&
         edit_bookmarks_enabled_.GetValue();
}

void LocationBarViewMac::UpdateZoomDecoration() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  zoom_decoration_->Update(ZoomController::FromWebContents(web_contents));
}

void LocationBarViewMac::UpdateStarDecorationVisibility() {
  // If the action box is enabled, only show the star if it's lit.
  bool visible = IsStarEnabled();
  if (!star_decoration_->starred() &&
      extensions::FeatureSwitch::action_box()->IsEnabled())
    visible = false;
  star_decoration_->SetVisible(visible);
}

void LocationBarViewMac::UpdatePlusDecorationVisibility() {
  if (extensions::FeatureSwitch::action_box()->IsEnabled()) {
    // If the action box is enabled, hide it when input is in progress.
    plus_decoration_->SetVisible(!toolbar_model_->GetInputInProgress());
  }
}
