// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/first_run_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/keyword_hint_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/save_credit_card_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/translate_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#import "components/omnibox/browser/omnibox_popup_model.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_state/security_state_model.h"
#include "components/translate/core/browser/language_state.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"

using content::WebContents;

namespace {

// Vertical space between the bottom edge of the location_bar and the first run
// bubble arrow point.
const static int kFirstRunBubbleYOffset = 1;

const int kDefaultIconSize = 16;

// Color of the vector graphic icons when the location bar is dark.
// SkColorSetARGB(0xCC, 0xFF, 0xFF 0xFF);
const SkColor kMaterialDarkVectorIconColor = SK_ColorWHITE;

}  // namespace

// A temporary class that draws hardcoded HTTP graphic icons for Material
// design. This class will be removed once the Material icons are available
// in M53.
@interface LocationBarImageRep : NSCustomImageRep
@property(assign, nonatomic) gfx::VectorIconId iconId;
@property(retain, nonatomic) NSColor* fillColor;

+ (NSImage*)imageForId:(gfx::VectorIconId)vectorIconId
                 color:(SkColor)vectorIconColor;

// NSCustomImageRep delegate method that performs the drawing.
+ (void)drawLocationBarIcon:(LocationBarImageRep*)imageRep;

@end

@implementation LocationBarImageRep

@synthesize iconId = iconId_;
@synthesize fillColor = fillColor_;

- (void)dealloc {
  [fillColor_ release];
  [super dealloc];
}

+ (NSImage*)imageForId:(gfx::VectorIconId)vectorIconId
                 color:(SkColor)vectorIconColor {
  if (vectorIconId != gfx::VectorIconId::LOCATION_BAR_HTTP &&
      vectorIconId != gfx::VectorIconId::LOCATION_BAR_HTTPS_INVALID &&
      vectorIconId != gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID) {
    return NSImageFromImageSkiaWithColorSpace(
        gfx::CreateVectorIcon(vectorIconId, kDefaultIconSize, vectorIconColor),
        base::mac::GetSRGBColorSpace());
  }

  base::scoped_nsobject<LocationBarImageRep> imageRep(
      [[LocationBarImageRep alloc]
          initWithDrawSelector:@selector(drawLocationBarIcon:)
                      delegate:[LocationBarImageRep class]]);
  [imageRep setIconId:vectorIconId];
  [imageRep setFillColor:skia::SkColorToSRGBNSColor(vectorIconColor)];

  // Create the image from the image rep.
  const NSSize kImageSize = NSMakeSize(kDefaultIconSize, kDefaultIconSize);
  NSImage* locationBarImage =
      [[[NSImage alloc] initWithSize:kImageSize] autorelease];
  [locationBarImage setCacheMode:NSImageCacheAlways];
  [locationBarImage addRepresentation:imageRep];

  return locationBarImage;
}

+ (void)drawLocationBarIcon:(LocationBarImageRep*)imageRep {
  [[imageRep fillColor] set];

  // Determine the scale factor.
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGRect unitRect = CGRectMake(0.0, 0.0, 1.0, 1.0);
  CGRect deviceRect = CGContextConvertRectToDeviceSpace(context, unitRect);
  int scaleFactor = deviceRect.size.height;

  switch ([imageRep iconId]) {
    case gfx::VectorIconId::LOCATION_BAR_HTTP:
      [self drawLocationBarIconHTTPForScale:scaleFactor];
      break;
    case gfx::VectorIconId::LOCATION_BAR_HTTPS_INVALID:
      [self drawLocationBarIconHTTPSInvalidForScale:scaleFactor];
      break;
    case gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID:
      [self drawLocationBarIconHTTPSValidForScale:scaleFactor];
      break;
    default:
      // Make it obvious that there's a problem.
      [[NSColor redColor] set];
      NSRectFill(NSMakeRect(0, 0, kDefaultIconSize, kDefaultIconSize));
      break;
  }
}

+ (void)drawLocationBarIconHTTPForScale:(int)scaleFactor {
  if (scaleFactor > 1) {
    NSRect ovalRect = NSMakeRect(2.25, 1.75, 12, 12);
    NSBezierPath* circlePath =
        [NSBezierPath bezierPathWithOvalInRect:ovalRect];
    [circlePath setLineWidth:1.5];
    [circlePath stroke];

    NSRectFill(NSMakeRect(7.5, 4.5, 1.5, 4));
    NSRectFill(NSMakeRect(7.5, 9.5, 1.5, 1.5));
  } else {
    NSRect ovalRect = NSMakeRect(2, 2, 12, 12);
    NSBezierPath* circlePath =
        [NSBezierPath bezierPathWithOvalInRect:ovalRect];
    [circlePath setLineWidth:1.5];
    [circlePath stroke];

    NSRectFill(NSMakeRect(7, 4, 2, 5));
    NSRectFill(NSMakeRect(7, 10, 2, 2));
  }
}

+ (void)drawLocationBarIconHTTPSInvalidForScale:(int)scaleFactor {
  // The vector icon is upside down relative to the default OS X coordinate
  // system so rotate by 180 degrees.
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  const int kHalfDefaultIconSize = kDefaultIconSize / 2;
  CGContextTranslateCTM(context, kHalfDefaultIconSize, kHalfDefaultIconSize);
  CGContextRotateCTM(context, M_PI);
  CGContextTranslateCTM(context, -kHalfDefaultIconSize, -kHalfDefaultIconSize);

  // If Retina, nudge the icon up 1/2pt.
  if (scaleFactor == 2) {
    CGContextTranslateCTM(context, 0, -0.5);
  }

  NSBezierPath* trianglePath = [NSBezierPath bezierPath];
  [trianglePath moveToPoint:NSMakePoint(0.5f, 14)];
  [trianglePath relativeLineToPoint:NSMakePoint(15, 0)];
  [trianglePath lineToPoint:NSMakePoint(8, 1)];
  [trianglePath closePath];

  NSBezierPath* cutOutPath = [NSBezierPath bezierPath];
  [cutOutPath moveToPoint:NSMakePoint(9, 12)];
  [cutOutPath relativeLineToPoint:NSMakePoint(-2, 0)];
  [cutOutPath relativeLineToPoint:NSMakePoint(0, -2)];
  [cutOutPath relativeLineToPoint:NSMakePoint(2, 0)];
  [cutOutPath relativeLineToPoint:NSMakePoint(0, 2)];
  [cutOutPath closePath];
  [cutOutPath relativeMoveToPoint:NSMakePoint(0, -3)];
  [cutOutPath relativeLineToPoint:NSMakePoint(-2, 0)];
  [cutOutPath relativeLineToPoint:NSMakePoint(0, -3)];
  [cutOutPath relativeLineToPoint:NSMakePoint(2, 0)];
  [cutOutPath relativeLineToPoint:NSMakePoint(0, 3)];
  [cutOutPath closePath];

  [trianglePath appendBezierPath:cutOutPath];
  [trianglePath fill];
}

+ (void)drawLocationBarIconHTTPSValidForScale:(int)scaleFactor {
  NSAffineTransform* transform = [NSAffineTransform transform];
  // Adjust down 1px in Retina, so that the lock sits on the text baseline.
  if (scaleFactor > 1) {
    [transform translateXBy:0 yBy:-0.5];
  }

  NSBezierPath* rectPath =
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(4, 3, 8, 7)
                                      xRadius:1
                                      yRadius:1];
  [rectPath transformUsingAffineTransform:transform];
  [rectPath fill];

  NSBezierPath* curvePath = [NSBezierPath bezierPath];
  [curvePath moveToPoint:NSMakePoint(5.5, 9.75)];
  [curvePath lineToPoint:NSMakePoint(5.5, 10)];
  [curvePath curveToPoint:NSMakePoint(8, 13)
            controlPoint1:NSMakePoint(5.5, 13)
            controlPoint2:NSMakePoint(7.5, 13)];
  [curvePath curveToPoint:NSMakePoint(10.5, 10)
            controlPoint1:NSMakePoint(8.5, 13)
            controlPoint2:NSMakePoint(10.5, 13)];
  [curvePath lineToPoint:NSMakePoint(10.5, 9.75)];
  [curvePath setLineWidth:1.25];
  [curvePath transformUsingAffineTransform:transform];
  [curvePath stroke];
}

@end

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
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(new SelectedKeywordDecoration()),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get())),
      save_credit_card_decoration_(
          new SaveCreditCardDecoration(command_updater)),
      star_decoration_(new StarDecoration(command_updater)),
      translate_decoration_(new TranslateDecoration(command_updater)),
      zoom_decoration_(new ZoomDecoration(this)),
      keyword_hint_decoration_(new KeywordHintDecoration()),
      manage_passwords_decoration_(
          new ManagePasswordsDecoration(command_updater, this)),
      browser_(browser),
      location_bar_visible_(true),
      weak_ptr_factory_(this) {
  ScopedVector<ContentSettingImageModel> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (ContentSettingImageModel* model : models.get()) {
    // ContentSettingDecoration takes ownership of its model.
    content_setting_decorations_.push_back(
        new ContentSettingDecoration(model, this, profile));
  }
  models.weak_clear();

  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, profile->GetPrefs(),
      base::Bind(&LocationBarViewMac::OnEditBookmarksEnabledChanged,
                 base::Unretained(this)));

  zoom::ZoomEventManager::GetForBrowserContext(profile)
      ->AddZoomEventManagerObserver(this);

  [[field_ cell] setIsPopupMode:
      !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)];

  // Sets images for the decorations, and performs a layout. This call ensures
  // that this class is in a consistent state after initialization.
  OnChanged();
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];

  zoom::ZoomEventManager::GetForBrowserContext(profile())
      ->RemoveZoomEventManagerObserver(this);
}

void LocationBarViewMac::ShowFirstRunBubble() {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&LocationBarViewMac::ShowFirstRunBubbleInternal,
                            weak_ptr_factory_.GetWeakPtr()));
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

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  omnibox_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  omnibox_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
  if (RefreshContentSettingsDecorations())
    OnDecorationsChanged();
}

void LocationBarViewMac::UpdateManagePasswordsIconAndBubble() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  ManagePasswordsUIController::FromWebContents(web_contents)
      ->UpdateIconAndBubbleState(manage_passwords_decoration_->icon());
  OnDecorationsChanged();
}

void LocationBarViewMac::UpdateSaveCreditCardIcon() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  // |controller| may be nullptr due to lazy initialization.
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  bool enabled = controller && controller->IsIconVisible();
  command_updater()->UpdateCommandEnabled(IDC_SAVE_CREDIT_CARD_FOR_PAGE,
                                          enabled);
  save_credit_card_decoration_->SetIcon(IsLocationBarDark());
  save_credit_card_decoration_->SetVisible(enabled);
  OnDecorationsChanged();
}

void LocationBarViewMac::UpdatePageActions() {
  RefreshPageActionDecorations();
  Layout();

  [field_ updateMouseTracking];
  [field_ setNeedsDisplay:YES];
}

void LocationBarViewMac::UpdateBookmarkStarVisibility() {
  star_decoration_->SetVisible(IsStarEnabled());
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

bool LocationBarViewMac::ShowPageActionPopup(
    const extensions::Extension* extension, bool grant_active_tab) {
  for (ScopedVector<PageActionDecoration>::iterator iter =
           page_action_decorations_.begin();
       iter != page_action_decorations_.end(); ++iter) {
    if ((*iter)->GetExtension() == extension)
      return (*iter)->ActivatePageAction(grant_active_tab);
  }
  return false;
}

void LocationBarViewMac::UpdateOpenPDFInReaderPrompt() {
  // Not implemented on Mac.
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
    return page_action_decorations_[index]->GetPageAction();
  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarViewMac::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->IsVisible()) {
      if (current == index)
        return page_action_decorations_[i]->GetPageAction();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::TestPageActionPressed(size_t index) {
  DCHECK_LT(index, static_cast<size_t>(PageActionVisibleCount()));
  size_t current = 0;
  for (PageActionDecoration* decoration : page_action_decorations_) {
    if (decoration->IsVisible()) {
      if (current == index) {
        decoration->OnMousePressed(NSZeroRect, NSZeroPoint);
        return;
      }
      ++current;
    }
  }
}

bool LocationBarViewMac::GetBookmarkStarVisibility() {
  DCHECK(star_decoration_.get());
  return star_decoration_->IsVisible();
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  UpdateBookmarkStarVisibility();
  UpdateZoomDecoration(/*default_zoom_changed=*/false);
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::SetStarred(bool starred) {
  if (star_decoration_->starred() == starred)
    return;

  star_decoration_->SetStarred(starred, IsLocationBarDark());
  UpdateBookmarkStarVisibility();
  OnDecorationsChanged();
}

void LocationBarViewMac::SetTranslateIconLit(bool on) {
  translate_decoration_->SetLit(on, IsLocationBarDark());
  OnDecorationsChanged();
}

void LocationBarViewMac::ZoomChangedForActiveTab(bool can_show_bubble) {
  bool changed = UpdateZoomDecoration(/*default_zoom_changed=*/false);
  if (changed)
    OnDecorationsChanged();

  if (can_show_bubble && zoom_decoration_->IsVisible())
    zoom_decoration_->ShowBubble(YES);
}

bool LocationBarViewMac::IsStarEnabled() const {
  return browser_defaults::bookmarks_enabled &&
         [field_ isEditable] &&
         !GetToolbarModel()->input_in_progress() &&
         edit_bookmarks_enabled_.GetValue() &&
         !IsBookmarkStarHiddenByExtension();
}

NSPoint LocationBarViewMac::GetBookmarkBubblePoint() const {
  DCHECK(IsStarEnabled());
  return [field_ bubblePointForDecoration:star_decoration_.get()];
}

NSPoint LocationBarViewMac::GetSaveCreditCardBubblePoint() const {
  return [field_ bubblePointForDecoration:save_credit_card_decoration_.get()];
}

NSPoint LocationBarViewMac::GetTranslateBubblePoint() const {
  return [field_ bubblePointForDecoration:translate_decoration_.get()];
}

NSPoint LocationBarViewMac::GetManagePasswordsBubblePoint() const {
  return [field_ bubblePointForDecoration:manage_passwords_decoration_.get()];
}

NSPoint LocationBarViewMac::GetPageInfoBubblePoint() const {
  if (ev_bubble_decoration_->IsVisible()) {
    return [field_ bubblePointForDecoration:ev_bubble_decoration_.get()];
  } else {
    return [field_ bubblePointForDecoration:location_icon_decoration_.get()];
  }
}

void LocationBarViewMac::OnDecorationsChanged() {
  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  std::vector<LocationBarDecoration*> decorations = GetDecorations();
  for (const auto& decoration : decorations)
    UpdateAccessibilityViewPosition(decoration);
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
  [cell addRightDecoration:translate_decoration_.get()];
  [cell addRightDecoration:zoom_decoration_.get()];
  [cell addRightDecoration:save_credit_card_decoration_.get()];
  [cell addRightDecoration:manage_passwords_decoration_.get()];

  // Note that display order is right to left.
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    [cell addRightDecoration:page_action_decorations_[i]];
  }

  for (ScopedVector<ContentSettingDecoration>::iterator i =
       content_setting_decorations_.begin();
       i != content_setting_decorations_.end(); ++i) {
    [cell addRightDecoration:*i];
  }

  [cell addRightDecoration:keyword_hint_decoration_.get()];

  // By default only the location icon is visible.
  location_icon_decoration_->SetVisible(true);
  selected_keyword_decoration_->SetVisible(false);
  ev_bubble_decoration_->SetVisible(false);
  keyword_hint_decoration_->SetVisible(false);

  // Get the keyword to use for keyword-search and hinting.
  const base::string16 keyword = omnibox_view_->model()->keyword();
  base::string16 short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = TemplateURLServiceFactory::GetForProfile(profile())->
        GetKeywordShortName(keyword, &is_extension_keyword);
  }

  const bool is_keyword_hint = omnibox_view_->model()->is_keyword_hint();
  if (!keyword.empty() && !is_keyword_hint) {
    // Switch from location icon to keyword mode.
    location_icon_decoration_->SetVisible(false);
    selected_keyword_decoration_->SetVisible(true);
    selected_keyword_decoration_->SetKeyword(short_name, is_extension_keyword);
    // Note: the first time through this code path the
    // |selected_keyword_decoration_| has no image set because under Material
    // Design we need to set its color, which we cannot do until we know the
    // theme (by being installed in a browser window).
    selected_keyword_decoration_->SetImage(GetKeywordImage(keyword));
  } else if (ShouldShowEVBubble()) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    base::string16 label(GetToolbarModel()->GetEVCertName());
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
  decoration->UpdateVisibility(contents);
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

void LocationBarViewMac::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

void LocationBarViewMac::Update(const WebContents* contents) {
  UpdateManagePasswordsIconAndBubble();
  UpdateBookmarkStarVisibility();
  UpdateSaveCreditCardIcon();
  UpdateTranslateDecoration();
  UpdateZoomDecoration(/*default_zoom_changed=*/false);
  RefreshPageActionDecorations();
  RefreshContentSettingsDecorations();
  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();
  OnChanged();
}

void LocationBarViewMac::UpdateWithoutTabRestore() {
  Update(nullptr);
}

void LocationBarViewMac::UpdateLocationIcon() {
  bool in_dark_mode = IsLocationBarDark();

  SkColor vector_icon_color = gfx::kPlaceholderColor;
  gfx::VectorIconId vector_icon_id = gfx::VectorIconId::VECTOR_ICON_NONE;
  if (ShouldShowEVBubble()) {
    vector_icon_id = gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID;
    vector_icon_color = gfx::kGoogleGreen700;
  } else {
    vector_icon_id = omnibox_view_->GetVectorIcon();
    security_state::SecurityStateModel::SecurityLevel security_level =
        GetToolbarModel()->GetSecurityLevel(false);
    if (security_level == security_state::SecurityStateModel::NONE) {
      vector_icon_color = gfx::kChromeIconGrey;
    } else {
      NSColor* sRGBColor =
          OmniboxViewMac::GetSecureTextColor(security_level, in_dark_mode);
      NSColor* deviceColor =
          [sRGBColor colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
      vector_icon_color = skia::NSDeviceColorToSkColor(deviceColor);
    }
  }

  // If the theme is dark, then the color should always be
  // kMaterialDarkVectorIconColor.
  if (in_dark_mode)
    vector_icon_color = kMaterialDarkVectorIconColor;

  DCHECK(vector_icon_id != gfx::VectorIconId::VECTOR_ICON_NONE);
  NSImage* image =
      [LocationBarImageRep imageForId:vector_icon_id color:vector_icon_color];

  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();
}

void LocationBarViewMac::UpdateColorsToMatchTheme() {
  if (!ui::MaterialDesignController::IsModeMaterial() ||
      ![[field_ window] inIncognitoMode]) {
    return;
  }

  // Update the location-bar icon.
  UpdateLocationIcon();

  // Make sure we're displaying the correct star color for Incognito mode. If
  // the window is in Incognito mode, switching between a theme and no theme
  // can move the window in and out of dark mode.
  star_decoration_->SetStarred(star_decoration_->starred(),
                               IsLocationBarDark());

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
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    const int resource_id = omnibox_view_->GetIcon();
    NSImage* image = OmniboxViewMac::ImageForResource(resource_id);
    location_icon_decoration_->SetImage(image);
    ev_bubble_decoration_->SetImage(image);
    Layout();
    return;
  }
  UpdateLocationIcon();
}

void LocationBarViewMac::ShowURL() {
  omnibox_view_->ShowURL();
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

bool LocationBarViewMac::ShouldShowEVBubble() const {
  return (GetToolbarModel()->GetSecurityLevel(false) ==
          security_state::SecurityStateModel::EV_SECURE);
}

bool LocationBarViewMac::IsLocationBarDark() const {
  return [[field_ window] inIncognitoModeWithSystemTheme];
}

NSImage* LocationBarViewMac::GetKeywordImage(const base::string16& keyword) {
  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      profile())->GetTemplateURLForKeyword(keyword);
  if (template_url &&
      (template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION)) {
    return extensions::OmniboxAPI::Get(profile())->
        GetOmniboxIcon(template_url->GetExtensionId()).AsNSImage();
  }

  if (ui::MaterialDesignController::IsModeMaterial()) {
    SkColor icon_color = IsLocationBarDark() ? kMaterialDarkVectorIconColor
                                             : gfx::kGoogleBlue700;
    return NSImageFromImageSkiaWithColorSpace(
        gfx::CreateVectorIcon(gfx::VectorIconId::OMNIBOX_SEARCH,
                              kDefaultIconSize, icon_color),
        base::mac::GetSRGBColorSpace());
  }

  return OmniboxViewMac::ImageForResource(IDR_OMNIBOX_SEARCH);
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
}

PageActionDecoration* LocationBarViewMac::GetPageActionDecoration(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    if (page_action_decorations_[i]->GetPageAction() == page_action)
      return page_action_decorations_[i];
  }
  // If |page_action| is the browser action of an extension, no element in
  // |page_action_decorations_| will match.
  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::DeletePageActionDecorations() {
  // TODO(shess): Deleting these decorations could result in the cell
  // refering to them before things are laid out again.  Meanwhile, at
  // least fail safe.
  [[field_ cell] clearDecorations];

  page_action_decorations_.clear();
}

void LocationBarViewMac::OnEditBookmarksEnabledChanged() {
  UpdateBookmarkStarVisibility();
  OnChanged();
}

void LocationBarViewMac::RefreshPageActionDecorations() {
  if (!IsEditable()) {
    DeletePageActionDecorations();
    return;
  }

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    DeletePageActionDecorations();
    return;
  }

  std::vector<ExtensionAction*> new_page_actions =
      extensions::TabHelper::FromWebContents(web_contents)->
          location_bar_controller()->GetCurrentActions();

  if (PageActionsDiffer(new_page_actions)) {
    DeletePageActionDecorations();
    for (size_t i = 0; i < new_page_actions.size(); ++i) {
      page_action_decorations_.push_back(
          new PageActionDecoration(this, browser_, new_page_actions[i]));
    }
  }

  GURL url = GetToolbarModel()->GetURL();
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    page_action_decorations_[i]->UpdateVisibility(
        GetToolbarModel()->input_in_progress() ? NULL : web_contents);
  }
}

bool LocationBarViewMac::PageActionsDiffer(
    const std::vector<ExtensionAction*>& page_actions) const {
  if (page_action_decorations_.size() != page_actions.size())
    return true;

  for (size_t index = 0; index < page_actions.size(); ++index) {
    PageActionDecoration* decoration = page_action_decorations_[index];
    if (decoration->GetPageAction() != page_actions[index])
      return true;
  }

  return false;
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
                                profile:profile()];
}

void LocationBarViewMac::UpdateTranslateDecoration() {
  if (!TranslateService::IsTranslateBubbleEnabled())
    return;

  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  translate::LanguageState& language_state =
      ChromeTranslateClient::FromWebContents(web_contents)->GetLanguageState();
  bool enabled = language_state.translate_enabled();
  command_updater()->UpdateCommandEnabled(IDC_TRANSLATE_PAGE, enabled);
  translate_decoration_->SetVisible(enabled);
  translate_decoration_->SetLit(language_state.IsPageTranslated(),
                                IsLocationBarDark());
}

bool LocationBarViewMac::UpdateZoomDecoration(bool default_zoom_changed) {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  return zoom_decoration_->UpdateIfNecessary(
      zoom::ZoomController::FromWebContents(web_contents), default_zoom_changed,
      IsLocationBarDark());
}

void LocationBarViewMac::UpdateAccessibilityViewPosition(
    LocationBarDecoration* decoration) {
  if (!decoration->IsVisible())
    return;
  NSRect r =
      [[field_ cell] frameForDecoration:decoration inFrame:[field_ frame]];
  [decoration->GetAccessibilityView() setFrame:r];
  [decoration->GetAccessibilityView() setNeedsDisplayInRect:r];
}

std::vector<LocationBarDecoration*> LocationBarViewMac::GetDecorations() {
  std::vector<LocationBarDecoration*> decorations;
  // TODO(ellyjones): content setting decorations aren't included right now, nor
  // are page actions and the keyword hint.
  decorations.push_back(location_icon_decoration_.get());
  decorations.push_back(selected_keyword_decoration_.get());
  decorations.push_back(ev_bubble_decoration_.get());
  decorations.push_back(save_credit_card_decoration_.get());
  decorations.push_back(star_decoration_.get());
  decorations.push_back(translate_decoration_.get());
  decorations.push_back(zoom_decoration_.get());
  decorations.push_back(manage_passwords_decoration_.get());
  return decorations;
}

void LocationBarViewMac::OnDefaultZoomLevelChanged() {
  if (UpdateZoomDecoration(/*default_zoom_changed=*/true))
    OnDecorationsChanged();
}

std::vector<NSView*> LocationBarViewMac::GetDecorationAccessibilityViews() {
  std::vector<LocationBarDecoration*> decorations = GetDecorations();
  std::vector<NSView*> views;
  for (const auto& decoration : decorations)
    views.push_back(decoration->GetAccessibilityView());
  return views;
}
