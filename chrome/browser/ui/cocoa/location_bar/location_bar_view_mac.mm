// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/first_run_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/keyword_hint_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/mic_search_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#import "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_event_utils.h"
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
    Profile* profile,
    Browser* browser)
    : OmniboxEditController(command_updater),
      omnibox_view_(new OmniboxViewMac(this, profile, command_updater, field)),
      field_(field),
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(new SelectedKeywordDecoration()),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get())),
      star_decoration_(new StarDecoration(command_updater)),
      zoom_decoration_(new ZoomDecoration(this)),
      keyword_hint_decoration_(new KeywordHintDecoration()),
      mic_search_decoration_(new MicSearchDecoration(command_updater)),
      profile_(profile),
      browser_(browser),
      weak_ptr_factory_(this) {

  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    DCHECK_EQ(i, content_setting_decorations_.size());
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    content_setting_decorations_.push_back(
        new ContentSettingDecoration(type, this, profile_));
  }

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

  browser_->search_model()->AddObserver(this);

  [[field_ cell] setIsPopupMode:
      !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)];
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];

  browser_->search_model()->RemoveObserver(this);
}

void LocationBarViewMac::ShowFirstRunBubble() {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&LocationBarViewMac::ShowFirstRunBubbleInternal,
          weak_ptr_factory_.GetWeakPtr()));
}

GURL LocationBarViewMac::GetDestinationURL() const {
  return destination_url();
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition();
}

content::PageTransition LocationBarViewMac::GetPageTransition() const {
  return transition();
}

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  omnibox_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  omnibox_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  omnibox_view_->SetForcedQuery();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
  if (RefreshContentSettingsDecorations())
    OnDecorationsChanged();
  PopUpContentSettingIfNeeded();
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

void LocationBarViewMac::UpdateOpenPDFInReaderPrompt() {
  // Not implemented on Mac.
}

void LocationBarViewMac::UpdateGeneratedCreditCardView() {
  // TODO(dbeam): encourage groby@ to implement via prodding or chocolate.
  NOTIMPLEMENTED();
}

void LocationBarViewMac::SaveStateToContents(WebContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  omnibox_view_->SaveStateToTab(contents);
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

bool LocationBarViewMac::GetBookmarkStarVisibility() {
  DCHECK(star_decoration_.get());
  return star_decoration_->IsVisible();
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  UpdateStarDecorationVisibility();
  UpdateZoomDecoration();
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::SetStarred(bool starred) {
  star_decoration_->SetStarred(starred);
  UpdateStarDecorationVisibility();
  OnDecorationsChanged();
}

void LocationBarViewMac::ZoomChangedForActiveTab(bool can_show_bubble) {
  UpdateZoomDecoration();
  OnDecorationsChanged();

  if (can_show_bubble && zoom_decoration_->IsVisible())
    zoom_decoration_->ShowBubble(YES);
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

void LocationBarViewMac::OnDecorationsChanged() {
  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  [field_ updateMouseTracking];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
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
  [cell addRightDecoration:star_decoration_.get()];
  [cell addRightDecoration:zoom_decoration_.get()];

  // Note that display order is right to left.
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    [cell addRightDecoration:page_action_decorations_[i]];
  }

  // Iterate through |content_setting_decorations_| in reverse order so that
  // the order in which the decorations are drawn matches the Views code.
  for (ScopedVector<ContentSettingDecoration>::reverse_iterator i =
       content_setting_decorations_.rbegin();
       i != content_setting_decorations_.rend(); ++i) {
    [cell addRightDecoration:*i];
  }

  [cell addRightDecoration:keyword_hint_decoration_.get()];
  [cell addRightDecoration:mic_search_decoration_.get()];

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
  } else if (GetToolbarModel()->GetSecurityLevel(false) ==
             ToolbarModel::EV_SECURE) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    string16 label(GetToolbarModel()->GetEVCertName());
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
  decoration->UpdateVisibility(contents, GetToolbarModel()->GetURL());
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

void LocationBarViewMac::Update(const WebContents* contents) {
  command_updater()->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, IsStarEnabled());
  command_updater()->UpdateCommandEnabled(IDC_BOOKMARK_PAGE_FROM_STAR,
                                          IsStarEnabled());
  UpdateStarDecorationVisibility();
  UpdateZoomDecoration();
  RefreshPageActionDecorations();
  RefreshContentSettingsDecorations();
  UpdateMicSearchDecorationVisibility();
  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();
  OnChanged();
}

void LocationBarViewMac::OnChanged() {
  // Update the location-bar icon.
  const int resource_id = omnibox_view_->GetIcon();
  NSImage* image = OmniboxViewMac::ImageForResource(resource_id);
  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();

  if (browser_->instant_controller()) {
    browser_->instant_controller()->SetOmniboxBounds(
        gfx::Rect(NSRectToCGRect([field_ frame])));
  }
}

void LocationBarViewMac::OnSetFocus() {
  // Update the keyword and search hint states.
  OnChanged();
}

InstantController* LocationBarViewMac::GetInstant() {
  return browser_->instant_controller() ?
      browser_->instant_controller()->instant() : NULL;
}

WebContents* LocationBarViewMac::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

ToolbarModel* LocationBarViewMac::GetToolbarModel() {
  return browser_->toolbar_model();
}

const ToolbarModel* LocationBarViewMac::GetToolbarModel() const {
  return browser_->toolbar_model();
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

void LocationBarViewMac::ModelChanged(const SearchModel::State& old_state,
                                      const SearchModel::State& new_state) {
  if (UpdateMicSearchDecorationVisibility())
    Layout();
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
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

void LocationBarViewMac::PopUpContentSettingIfNeeded() {
  AutocompleteTextFieldCell* cell = [field_ cell];
  const NSRect bounds = [field_ bounds];
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    const NSRect frame =
        [cell frameForDecoration:content_setting_decorations_[i]
                         inFrame:bounds];
    content_setting_decorations_[i]->PopUpIfNeeded(frame);
  }
}

void LocationBarViewMac::DeletePageActionDecorations() {
  // TODO(shess): Deleting these decorations could result in the cell
  // refering to them before things are laid out again.  Meanwhile, at
  // least fail safe.
  [[field_ cell] clearDecorations];

  page_action_decorations_.clear();
}

void LocationBarViewMac::OnEditBookmarksEnabledChanged() {
  UpdateStarDecorationVisibility();
  OnChanged();
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

  GURL url = GetToolbarModel()->GetURL();
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    page_action_decorations_[i]->UpdateVisibility(
        GetToolbarModel()->input_in_progress() ? NULL : web_contents, url);
  }
}

bool LocationBarViewMac::RefreshContentSettingsDecorations() {
  const bool input_in_progress = GetToolbarModel()->input_in_progress();
  WebContents* web_contents = input_in_progress ?
      NULL : browser_->tab_strip_model()->GetActiveWebContents();
  bool icons_updated = false;
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    icons_updated |=
        content_setting_decorations_[i]->UpdateFromWebContents(web_contents);
  }
  return icons_updated;
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

bool LocationBarViewMac::IsStarEnabled() {
  return [field_ isEditable] &&
         browser_defaults::bookmarks_enabled &&
         !GetToolbarModel()->input_in_progress() &&
         edit_bookmarks_enabled_.GetValue();
}

void LocationBarViewMac::UpdateZoomDecoration() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  zoom_decoration_->Update(ZoomController::FromWebContents(web_contents));
}

void LocationBarViewMac::UpdateStarDecorationVisibility() {
  star_decoration_->SetVisible(IsStarEnabled());
}

bool LocationBarViewMac::UpdateMicSearchDecorationVisibility() {
  bool is_visible = !GetToolbarModel()->input_in_progress() &&
                    browser_->search_model()->voice_search_supported();
  if (mic_search_decoration_->IsVisible() == is_visible)
    return false;
  mic_search_decoration_->SetVisible(is_visible);
  return true;
}
