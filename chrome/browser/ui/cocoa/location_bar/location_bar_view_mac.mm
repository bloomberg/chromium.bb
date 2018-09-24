// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#import "base/mac/mac_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#import "components/omnibox/browser/omnibox_popup_model.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/toolbar/vector_icons.h"
#include "components/translate/core/browser/language_state.h"
#include "components/variations/variations_associated_data.h"
#include "components/vector_icons/vector_icons.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

using content::WebContents;

namespace {

const int kDefaultIconSize = 16;

// Color of the vector graphic icons when the location bar is dark.
// SkColorSetARGB(0xCC, 0xFF, 0xFF 0xFF);
const SkColor kMaterialDarkVectorIconColor = SK_ColorWHITE;

}  // namespace

// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

LocationBarViewMac::LocationBarViewMac(AutocompleteTextField* field,
                                       CommandUpdater* command_updater,
                                       Profile* profile,
                                       Browser* browser)
    : LocationBar(profile),
      ChromeOmniboxEditController(command_updater),
      omnibox_view_(new OmniboxViewMac(this, profile, command_updater, field)),
      field_(field),
      zoom_decoration_(new ZoomDecoration(this)),
      browser_(browser),
      location_bar_visible_(true) {
  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, profile->GetPrefs(),
      base::Bind(&LocationBarViewMac::OnEditBookmarksEnabledChanged,
                 base::Unretained(this)));

  zoom::ZoomEventManager::GetForBrowserContext(profile)->AddObserver(this);

  [[field_ cell] setIsPopupMode:
      !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)];

  // Sets images for the decorations, and performs a layout. This call ensures
  // that this class is in a consistent state after initialization.
  OnChanged();
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];

  zoom::ZoomEventManager::GetForBrowserContext(profile())->RemoveObserver(this);
}

GURL LocationBarViewMac::GetDestinationURL() const {
  return destination_url();
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition();
}

ui::PageTransition LocationBarViewMac::GetPageTransition() const {
  return transition();
}

base::TimeTicks LocationBarViewMac::GetMatchSelectionTimestamp() const {
  return match_selection_timestamp();
}

void LocationBarViewMac::AcceptInput() {
  AcceptInput(base::TimeTicks());
}

void LocationBarViewMac::AcceptInput(
    base::TimeTicks match_selection_timestamp) {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  omnibox_view_->model()->AcceptInput(disposition, false,
                                      match_selection_timestamp);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  omnibox_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
}

void LocationBarViewMac::UpdateManagePasswordsIconAndBubble() {
}

void LocationBarViewMac::UpdateSaveCreditCardIcon() {
}

void LocationBarViewMac::UpdateLocalCardMigrationIcon() {
  // TODO(crbug.com/859652): Implement for mac.
  NOTIMPLEMENTED();
}

void LocationBarViewMac::UpdateBookmarkStarVisibility() {
}

void LocationBarViewMac::UpdateLocationBarVisibility(bool visible,
                                                     bool animate) {
  // Track the target location bar visibility to avoid redundant transitions
  // being initiated when one is already in progress.
  if (visible != location_bar_visible_) {
    [[[BrowserWindowController browserWindowControllerForView:field_]
        toolbarController] updateVisibility:visible
                              withAnimation:animate];
    location_bar_visible_ = visible;
  }
}

void LocationBarViewMac::SaveStateToContents(WebContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  omnibox_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Revert() {
  omnibox_view_->RevertAll();
}

const OmniboxView* LocationBarViewMac::GetOmniboxView() const {
  return omnibox_view_.get();
}

OmniboxView* LocationBarViewMac::GetOmniboxView() {
  return omnibox_view_.get();
}

LocationBarTesting* LocationBarViewMac::GetLocationBarForTesting() {
  return this;
}

bool LocationBarViewMac::GetBookmarkStarVisibility() {
  return false;
}

bool LocationBarViewMac::TestContentSettingImagePressed(size_t index) {
  return false;
}

bool LocationBarViewMac::IsContentSettingBubbleShowing(size_t index) {
  return false;
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  UpdateBookmarkStarVisibility();
  UpdateZoomDecoration(/*default_zoom_changed=*/false);
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::ZoomChangedForActiveTab(bool can_show_bubble) {
  bool changed = UpdateZoomDecoration(/*default_zoom_changed=*/false);
  if (changed)
    OnDecorationsChanged();

  if (can_show_bubble && zoom_decoration_->IsVisible())
    zoom_decoration_->ShowBubble(YES);
}

NSPoint LocationBarViewMac::GetBubblePointForDecoration(
    LocationBarDecoration* decoration) const {
  return [field_ bubblePointForDecoration:decoration];
}

void LocationBarViewMac::OnDecorationsChanged() {
  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  std::vector<LocationBarDecoration*> decorations = GetDecorations();
  for (auto* decoration : decorations)
    UpdateAccessibilityView(decoration);
  [field_ updateMouseTracking];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
}

// TODO(shess): This function should over time grow to closely match
// the views Layout() function.
void LocationBarViewMac::Layout() {
  AutocompleteTextFieldCell* cell = [field_ cell];

  // Reset the leading decorations.
  // TODO(shess): Shortly, this code will live somewhere else, like in
  // the constructor.  I am still wrestling with how best to deal with
  // right-hand decorations, which are not a static set.
  [cell clearDecorations];
  [cell addTrailingDecoration:zoom_decoration_.get()];

  // Get the keyword to use for keyword-search and hinting.
  const base::string16 keyword = omnibox_view_->model()->keyword();
  base::string16 short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = TemplateURLServiceFactory::GetForProfile(profile())->
        GetKeywordShortName(keyword, &is_extension_keyword);
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

void LocationBarViewMac::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

void LocationBarViewMac::Update(const WebContents* contents) {
  UpdateManagePasswordsIconAndBubble();
  UpdateBookmarkStarVisibility();
  UpdateSaveCreditCardIcon();
  UpdateZoomDecoration(/*default_zoom_changed=*/false);
  RefreshContentSettingsDecorations();
  if (contents) {
    omnibox_view_->OnTabChanged(contents);
  } else {
    omnibox_view_->Update();
  }

  OnChanged();
}

void LocationBarViewMac::UpdateWithoutTabRestore() {
  Update(nullptr);
}

void LocationBarViewMac::UpdateLocationIcon() {
}

void LocationBarViewMac::UpdateColorsToMatchTheme() {
  // Update the location-bar icon.
  UpdateLocationIcon();

  // Update the appearance of the text in the Omnibox.
  omnibox_view_->Update();
}

void LocationBarViewMac::OnAddedToWindow() {
  UpdateColorsToMatchTheme();
}

void LocationBarViewMac::OnThemeChanged() {
  UpdateColorsToMatchTheme();
}

void LocationBarViewMac::OnChanged() {
  UpdateLocationIcon();
}

ToolbarModel* LocationBarViewMac::GetToolbarModel() {
  return browser_->toolbar_model();
}

const ToolbarModel* LocationBarViewMac::GetToolbarModel() const {
  return browser_->toolbar_model();
}

WebContents* LocationBarViewMac::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void LocationBarViewMac::UpdatePageActionIcon(PageActionIconType type) {
  // TODO(https://crbug.com/788051): Return page action icons for updating here
  // as update methods are migrated out of LocationBar to the
  // PageActionIconContainer interface.
  switch (type) {
    case PageActionIconType::kFind:
      // TODO(crbug/651643): Implement for mac.
      NOTIMPLEMENTED();
      break;
    case PageActionIconType::kZoom:
      UpdateZoomDecoration(/*default_zoom_changed=*/false);
      OnChanged();
      break;
  }
}

PageInfoVerboseType LocationBarViewMac::GetPageInfoVerboseType() const {
  if (omnibox_view_->IsEditingOrEmpty() ||
      omnibox_view_->model()->is_keyword_hint()) {
    return PageInfoVerboseType::kNone;
  } else if (GetToolbarModel()->GetSecurityLevel(false) ==
             security_state::EV_SECURE) {
    return PageInfoVerboseType::kEVCert;
  } else if (GetToolbarModel()->GetURL().SchemeIs(
                 extensions::kExtensionScheme)) {
    return PageInfoVerboseType::kExtension;
  } else if (GetToolbarModel()->GetURL().SchemeIs(content::kChromeUIScheme)) {
    return PageInfoVerboseType::kChrome;
  } else {
    return PageInfoVerboseType::kSecurity;
  }
}

bool LocationBarViewMac::HasSecurityVerboseText() const {
  if (GetPageInfoVerboseType() != PageInfoVerboseType::kSecurity)
    return false;

  return !GetToolbarModel()->GetSecureVerboseText().empty();
}

bool LocationBarViewMac::IsLocationBarDark() const {
  return [[field_ window] inIncognitoModeWithSystemTheme];
}

NSImage* LocationBarViewMac::GetKeywordImage(const base::string16& keyword) {
  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      profile())->GetTemplateURLForKeyword(keyword);
  if (template_url &&
      (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
    return extensions::OmniboxAPI::Get(profile())->
        GetOmniboxIcon(template_url->GetExtensionId()).AsNSImage();
  }

  SkColor icon_color =
      IsLocationBarDark() ? kMaterialDarkVectorIconColor : gfx::kGoogleBlue700;
  return NSImageFromImageSkiaWithColorSpace(
      gfx::CreateVectorIcon(vector_icons::kSearchIcon, kDefaultIconSize,
                            icon_color),
      base::mac::GetSRGBColorSpace());
}

SkColor LocationBarViewMac::GetLocationBarIconColor() const {
  bool in_dark_mode = IsLocationBarDark();
  if (in_dark_mode)
    return kMaterialDarkVectorIconColor;

  if (GetPageInfoVerboseType() == PageInfoVerboseType::kEVCert)
    return gfx::kGoogleGreen700;

  security_state::SecurityLevel security_level =
      GetToolbarModel()->GetSecurityLevel(false);

  if (security_level == security_state::NONE ||
      security_level == security_state::HTTP_SHOW_WARNING) {
    return gfx::kChromeIconGrey;
  }

  NSColor* srgb_color =
      OmniboxViewMac::GetSecureTextColor(security_level, in_dark_mode);
  NSColor* device_color =
      [srgb_color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
  return skia::NSDeviceColorToSkColor(device_color);
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
}

void LocationBarViewMac::OnEditBookmarksEnabledChanged() {
  UpdateBookmarkStarVisibility();
  OnChanged();
}

bool LocationBarViewMac::RefreshContentSettingsDecorations() {
  return false;
}

bool LocationBarViewMac::UpdateZoomDecoration(bool default_zoom_changed) {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  return zoom_decoration_->UpdateIfNecessary(
      zoom::ZoomController::FromWebContents(web_contents), default_zoom_changed,
      IsLocationBarDark());
}

void LocationBarViewMac::UpdateAccessibilityView(
    LocationBarDecoration* decoration) {
  if (!decoration->IsVisible())
    return;
  // This uses |frame| instead of |bounds| because the accessibility views are
  // parented to the toolbar.
  NSRect apparent_frame =
      [[field_ cell] frameForDecoration:decoration inFrame:[field_ frame]];

  // This is a bit subtle:
  // The decorations' accessibility views can become key to allow keyboard
  // access to the location bar decorations, but Cocoa's automatic key view loop
  // sorts by top-left coordinate. Since the omnibox's top-left coordinate is
  // before its leading decorations, the omnibox would sort before its own
  // leading decorations, which was logical but visually unintuitive. Therefore,
  // for leading decorations, this method moves their frame to be "just before"
  // the omnibox in automatic key view loop order, and gives them an apparent
  // frame (see DecorationAccessibilityView) so that they still paint their
  // focus rings at the right place.
  //
  // TODO(lgrey): This hack doesn't work in RTL layouts, but the layout of the
  // omnibox is currently screwed up in RTL layouts anyway. See
  // https://crbug.com/715627.
  NSRect real_frame = apparent_frame;
  int left_index = [[field_ cell] leadingDecorationIndex:decoration];

  // If there are ever too many leading views, the fake x-coords might land
  // before the button preceding the omnibox in the key view order. This
  // threshold is just a guess.
  DCHECK_LT(left_index, 10);
  if (left_index != -1) {
    CGFloat delta = left_index + 1;
    real_frame.origin.x =
        cocoa_l10n_util::ShouldDoExperimentalRTLLayout()
            ? NSMaxX([field_ frame]) + delta - NSWidth(real_frame)
            : NSMinX([field_ frame]) - delta;
  }
  decoration->UpdateAccessibilityView(apparent_frame);
  [decoration->GetAccessibilityView() setFrame:real_frame];
  [decoration->GetAccessibilityView() setNeedsDisplayInRect:apparent_frame];
}

std::vector<LocationBarDecoration*> LocationBarViewMac::GetDecorations() {
  std::vector<LocationBarDecoration*> decorations;

  // TODO(ellyjones): page actions and keyword hints are not included right
  // now. Keyword hints have no useful tooltip (issue 752592), and page actions
  // are likewise.
  decorations.push_back(zoom_decoration_.get());
  return decorations;
}

void LocationBarViewMac::OnDefaultZoomLevelChanged() {
  if (UpdateZoomDecoration(/*default_zoom_changed=*/true))
    OnDecorationsChanged();
}

std::vector<NSView*> LocationBarViewMac::GetDecorationAccessibilityViews() {
  std::vector<LocationBarDecoration*> decorations = GetDecorations();
  std::vector<NSView*> views;
  for (auto* decoration : decorations)
    views.push_back(decoration->GetAccessibilityView());
  return views;
}
