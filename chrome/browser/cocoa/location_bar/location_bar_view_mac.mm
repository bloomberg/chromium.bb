// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/i18n/rtl.h"
#include "base/nsimage_cache_mac.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/content_blocked_bubble_controller.h"
#include "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/cocoa/first_run_bubble_controller.h"
#import "chrome/browser/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/cocoa/location_bar/selected_keyword_decoration.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/location_bar_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"


// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

LocationBarViewMac::LocationBarViewMac(
    AutocompleteTextField* field,
    CommandUpdater* command_updater,
    ToolbarModel* toolbar_model,
    Profile* profile,
    Browser* browser)
    : edit_view_(new AutocompleteEditViewMac(this, toolbar_model, profile,
                                             command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(
          new SelectedKeywordDecoration([field_ font])),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get(),
                                 [field_ font])),
      star_icon_view_(command_updater),
      page_action_views_(this, profile, toolbar_model),
      profile_(profile),
      browser_(browser),
      toolbar_model_(toolbar_model),
      transition_(PageTransition::TYPED),
      first_run_bubble_(this) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_setting_view =
        new ContentSettingImageView(static_cast<ContentSettingsType>(i), this,
                                    profile_);
    content_setting_views_.push_back(content_setting_view);
    content_setting_view->SetVisible(false);
  }

  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setStarIconView:&star_icon_view_];
  [cell setPageActionViewList:&page_action_views_];
  [cell setContentSettingViewsList:&content_setting_views_];

  registrar_.Add(this,
      NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      NotificationService::AllSources());
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setPageActionViewList:NULL];
  [cell setStarIconView:NULL];
}

void LocationBarViewMac::ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarViewMac::ShowFirstRunBubbleInternal, bubble_type);
  MessageLoop::current()->PostTask(FROM_HERE, task);
}

void LocationBarViewMac::ShowFirstRunBubbleInternal(
    FirstRun::BubbleType bubble_type) {
  if (!field_ || ![field_ window])
    return;

  // The bubble needs to be just below the Omnibox and slightly to the right
  // of the left omnibox icon, so shift x and y co-ordinates.
  const NSPoint kOffset = NSMakePoint(1, 4);
  [FirstRunBubbleController showForView:field_ offset:kOffset profile:profile_];
}

std::wstring LocationBarViewMac::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarViewMac::GetPageTransition() const {
  return transition_;
}

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  edit_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation(bool select_all) {
  edit_view_->FocusLocation(select_all);
}

void LocationBarViewMac::FocusSearch() {
  edit_view_->SetForcedQuery();
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
  RefreshContentSettingsViews();
  [field_ updateCursorAndToolTipRects];
  [field_ setNeedsDisplay:YES];
}

void LocationBarViewMac::UpdatePageActions() {
  size_t count_before = page_action_views_.Count();
  page_action_views_.RefreshViews();
  [field_ resetFieldEditorFrameIfNeeded];
  if (page_action_views_.Count() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
  [field_ setNeedsDisplay:YES];
}

void LocationBarViewMac::InvalidatePageActions() {
  size_t count_before = page_action_views_.Count();
  page_action_views_.DeleteAll();
  if (page_action_views_.Count() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarViewMac::SaveStateToContents(TabContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  edit_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Update(const TabContents* contents,
                                bool should_restore_state) {
  page_action_views_.RefreshViews();
  RefreshContentSettingsViews();
  // AutocompleteEditView restores state if the tab is non-NULL.
  edit_view_->Update(should_restore_state ? contents : NULL);
  OnChanged();
}

void LocationBarViewMac::OnAutocompleteAccept(const GURL& url,
                                              WindowOpenDisposition disposition,
                                              PageTransition::Type transition,
                                              const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (!command_updater_)
    return;

  if (!alternate_nav_url.is_valid()) {
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
    return;
  }

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

void LocationBarViewMac::OnChanged() {
  // Update the location-bar icon.
  const int resource_id = edit_view_->GetIcon();
  NSImage* image = AutocompleteEditViewMac::ImageForResource(resource_id);
  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();
}

void LocationBarViewMac::OnInputInProgress(bool in_progress) {
  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL, false);
}

void LocationBarViewMac::OnSetFocus() {
  // Update the keyword and search hint states.
  OnChanged();
}

void LocationBarViewMac::OnKillFocus() {
  // Do nothing.
}

SkBitmap LocationBarViewMac::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewMac::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewMac::Revert() {
  edit_view_->RevertAll();
}

// TODO(pamg): Change all these, here and for other platforms, to size_t.
int LocationBarViewMac::PageActionCount() {
  return static_cast<int>(page_action_views_.Count());
}

int LocationBarViewMac::PageActionVisibleCount() {
  return static_cast<int>(page_action_views_.VisibleCount());
}

TabContents* LocationBarViewMac::GetTabContents() const {
  return browser_->GetSelectedTabContents();
}

void LocationBarViewMac::SetPreviewEnabledPageAction(
    ExtensionAction* page_action, bool preview_enabled) {
  DCHECK(page_action);
  TabContents* contents = GetTabContents();
  if (!contents)
    return;
  page_action_views_.RefreshViews();
  [field_ setNeedsDisplay:YES];

  LocationBarViewMac::PageActionImageView* page_action_image_view = NULL;
  for (size_t i = 0; i < page_action_views_.Count(); ++i) {
    if (page_action_views_.ViewAt(i)->page_action() == page_action)
      page_action_image_view = page_action_views_.ViewAt(i);
  }
  DCHECK(page_action_image_view);
  if (!page_action_image_view)
    return;

  page_action_image_view->set_preview_enabled(preview_enabled);
  page_action_image_view->UpdateVisibility(contents,
      GURL(WideToUTF8(toolbar_model_->GetText())));
}

NSPoint LocationBarViewMac::GetPageActionBubblePoint(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  size_t index = 0;
  for (; index < page_action_views_.Count(); ++index) {
    if (page_action_views_.ViewAt(index)->page_action() == page_action)
      break;
  }
  DCHECK_LT(index, page_action_views_.Count());
  if (index == page_action_views_.Count())
    return NSZeroPoint;

  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  NSRect frame = [cell pageActionFrameForIndex:index inFrame:[field_ bounds]];
  if (!NSIsEmptyRect(frame)) {
    frame = [field_ convertRect:frame toView:nil];
    return NSMakePoint(NSMidX(frame), NSMinY(frame));
  }

  NOTREACHED();
  return NSZeroPoint;
}

ExtensionAction* LocationBarViewMac::GetPageAction(size_t index) {
  if (index < page_action_views_.Count())
    return page_action_views_.ViewAt(index)->page_action();
  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarViewMac::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.Count(); ++i) {
    if (page_action_views_.ViewAt(i)->IsVisible()) {
      if (current == index)
        return page_action_views_.ViewAt(i)->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::TestPageActionPressed(size_t index) {
  if (index >= page_action_views_.Count()) {
    NOTREACHED();
    return;
  }
  page_action_views_.OnMousePressed(NSZeroRect, index);
}

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  star_icon_view_.SetVisible(editable);
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::SetStarred(bool starred) {
  star_icon_view_.SetStarred(starred);

  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  [field_ updateCursorAndToolTipRects];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
}

NSPoint LocationBarViewMac::GetBookmarkBubblePoint() const {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  NSRect frame = [cell starIconFrameForFrame:[field_ bounds]];
  frame = [field_ convertRect:frame toView:nil];

  // Star is not exactly centered horizontally, and the tip should
  // point to the visual bottom of the star, not the middle.
  return NSMakePoint(NSMidX(frame), NSMinY(frame) + 4.0);
}

NSImage* LocationBarViewMac::GetTabButtonImage() {
  if (!tab_button_image_) {
    SkBitmap* skiaBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
    if (skiaBitmap) {
      tab_button_image_.reset([gfx::SkBitmapToNSImage(*skiaBitmap) retain]);
    }
  }
  return tab_button_image_;
}

void LocationBarViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
      TabContents* contents = GetTabContents();
      if (Details<TabContents>(contents) != details)
        return;

      [field_ updateCursorAndToolTipRects];
      [field_ setNeedsDisplay:YES];
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

void LocationBarViewMac::PostNotification(NSString* notification) {
  [[NSNotificationCenter defaultCenter] postNotificationName:notification
                                        object:[NSValue valueWithPointer:this]];
}

void LocationBarViewMac::RefreshContentSettingsViews() {
  const TabContents* tab_contents = browser_->GetSelectedTabContents();
  for (ContentSettingViews::iterator it(content_setting_views_.begin());
      it != content_setting_views_.end();
      ++it) {
    (*it)->UpdateFromTabContents(
        toolbar_model_->input_in_progress() ? NULL : tab_contents);
  }
}

// LocationBarImageView---------------------------------------------------------

void LocationBarViewMac::LocationBarImageView::SetImage(NSImage* image) {
  image_.reset([image retain]);
}

void LocationBarViewMac::LocationBarImageView::SetIcon(int resource_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetImage(rb.GetNSImageNamed(resource_id));
}

void LocationBarViewMac::LocationBarImageView::SetLabel(NSString* text,
                                                        NSFont* baseFont,
                                                        NSColor* color) {
  // Create an attributed string for the label, if a label was given.
  label_.reset();
  if (text) {
    DCHECK(color);
    DCHECK(baseFont);
    NSFont* font = [NSFont fontWithDescriptor:[baseFont fontDescriptor]
                                         size:[baseFont pointSize] - 2.0];
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
         color, NSForegroundColorAttributeName,
         font, NSFontAttributeName,
         NULL];
    NSAttributedString* attrStr =
        [[NSAttributedString alloc] initWithString:text attributes:attributes];
    label_.reset(attrStr);
  }
}

void LocationBarViewMac::LocationBarImageView::SetVisible(bool visible) {
  visible_ = visible;
}

NSSize LocationBarViewMac::LocationBarImageView::GetDefaultImageSize() const {
  return NSZeroSize;
}

NSSize LocationBarViewMac::LocationBarImageView::GetImageSize() const {
  NSImage* image = GetImage();
  if (image)
    return [image size];
  return GetDefaultImageSize();
}

// StarIconView-----------------------------------------------------------------

LocationBarViewMac::StarIconView::StarIconView(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetVisible(true);
  SetStarred(false);
}

void LocationBarViewMac::StarIconView::SetStarred(bool starred) {
  if (starred) {
    SetImage(AutocompleteEditViewMac::ImageForResource(IDR_OMNIBOX_STAR_LIT));
    tooltip_.reset(
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STARRED) retain]);
  } else {
    SetImage(AutocompleteEditViewMac::ImageForResource(IDR_OMNIBOX_STAR));
    tooltip_.reset(
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STAR) retain]);
  }
}

void LocationBarViewMac::StarIconView::OnMousePressed(NSRect bounds) {
  command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
}

NSString* LocationBarViewMac::StarIconView::GetToolTip() {
  return tooltip_.get();
}

// PageActionImageView----------------------------------------------------------

LocationBarViewMac::PageActionImageView::PageActionImageView(
    LocationBarViewMac* owner,
    Profile* profile,
    ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      tracker_(this),
      current_tab_id_(-1),
      preview_enabled_(false) {
  Extension* extension = profile->GetExtensionsService()->GetExtensionById(
      page_action->extension_id(), false);
  DCHECK(extension);

  // Load all the icons declared in the manifest. This is the contents of the
  // icons array, plus the default_icon property, if any.
  std::vector<std::string> icon_paths(*page_action->icon_paths());
  if (!page_action_->default_icon_path().empty())
    icon_paths.push_back(page_action_->default_icon_path());

  for (std::vector<std::string>::iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    tracker_.LoadImage(extension, extension->GetResource(*iter),
                       gfx::Size(Extension::kPageActionIconMaxSize,
                                 Extension::kPageActionIconMaxSize),
                       ImageLoadingTracker::DONT_CACHE);
  }

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      Source<Profile>(profile_));
}

LocationBarViewMac::PageActionImageView::~PageActionImageView() {
}

NSSize LocationBarViewMac::PageActionImageView::GetDefaultImageSize() const {
  return NSMakeSize(Extension::kPageActionIconMaxSize,
                    Extension::kPageActionIconMaxSize);
}

// Overridden from LocationBarImageView. Either notify listeners or show a
// popup depending on the Page Action.
void LocationBarViewMac::PageActionImageView::OnMousePressed(NSRect bounds) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    // We don't want other code to try and handle this click.  Returning true
    // prevents this by indicating that we handled it.
    return;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    AutocompleteTextField* textField = owner_->GetAutocompleteTextField();
    NSWindow* window = [textField window];
    NSRect relativeBounds = [[window contentView] convertRect:bounds
                                                     fromView:textField];
    NSPoint arrowPoint = NSMakePoint(NSMinX(relativeBounds),
                                     NSMinY(relativeBounds));
    // Adjust the anchor point to be at the center of the page action icon.
    arrowPoint.x += [GetImage() size].width / 2;

    [ExtensionPopupController showURL:page_action_->GetPopupUrl(current_tab_id_)
                            inBrowser:BrowserList::GetLastActive()
                           anchoredAt:arrowPoint
                        arrowLocation:info_bubble::kTopRight
                              devMode:NO];
  } else {
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        profile_, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(),
        1);
  }
}

void LocationBarViewMac::PageActionImageView::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  // We loaded icons()->size() icons, plus one extra if the Page Action had
  // a default icon.
  int total_icons = static_cast<int>(page_action_->icon_paths()->size());
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    if (index < static_cast<int>(page_action_->icon_paths()->size()))
      page_action_icons_[page_action_->icon_paths()->at(index)] = *image;
    else
      page_action_icons_[page_action_->default_icon_path()] = *image;
  }

  owner_->UpdatePageActions();
}

void LocationBarViewMac::PageActionImageView::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = ExtensionTabUtil::GetTabId(contents);
  current_url_ = url;

  bool visible = preview_enabled_ ||
                 page_action_->GetIsVisible(current_tab_id_);
  if (visible) {
    SetToolTip(page_action_->GetTitle(current_tab_id_));

    // Set the image.
    // It can come from three places. In descending order of priority:
    // - The developer can set it dynamically by path or bitmap. It will be in
    //   page_action_->GetIcon().
    // - The developer can set it dynamically by index. It will be in
    //   page_action_->GetIconIndex().
    // - It can be set in the manifest by path. It will be in page_action_->
    //   default_icon_path().

    // First look for a dynamically set bitmap.
    SkBitmap skia_icon = page_action_->GetIcon(current_tab_id_);
    if (skia_icon.isNull()) {
      int icon_index = page_action_->GetIconIndex(current_tab_id_);
      std::string icon_path;
      if (icon_index >= 0)
        icon_path = page_action_->icon_paths()->at(icon_index);
      else
        icon_path = page_action_->default_icon_path();

      if (!icon_path.empty()) {
        PageActionMap::iterator iter = page_action_icons_.find(icon_path);
        if (iter != page_action_icons_.end())
          skia_icon = iter->second;
      }
    }

    if (!skia_icon.isNull())
      SetImage(gfx::SkBitmapToNSImage(skia_icon));
  }
  if (IsVisible() != visible) {
    SetVisible(visible);
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        Source<ExtensionAction>(page_action_),
        Details<TabContents>(contents));
  }
}

void LocationBarViewMac::PageActionImageView::SetToolTip(NSString* tooltip) {
  if (!tooltip) {
    tooltip_.reset();
    return;
  }
  tooltip_.reset([tooltip retain]);
}

void LocationBarViewMac::PageActionImageView::SetToolTip(std::string tooltip) {
  if (tooltip.empty()) {
    SetToolTip(nil);
    return;
  }
  SetToolTip(base::SysUTF8ToNSString(tooltip));
}

NSString* LocationBarViewMac::PageActionImageView::GetToolTip() {
  return tooltip_.get();
}

NSMenu* LocationBarViewMac::PageActionImageView::GetMenu() {
  if (!profile_)
    return nil;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return nil;
  Extension* extension = service->GetExtensionById(
      page_action_->extension_id(), false);
  DCHECK(extension);
  if (!extension)
    return nil;
  return [[[ExtensionActionContextMenu alloc]
      initWithExtension:extension
                profile:profile_
        extensionAction:page_action_] autorelease];
}

void LocationBarViewMac::PageActionImageView::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionPopupController* popup = [ExtensionPopupController popup];
      if (popup && ![popup isClosing])
        [popup close];

      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

// ContentSettingsImageView-----------------------------------------------------
LocationBarViewMac::ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType settings_type,
    LocationBarViewMac* owner,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              settings_type)),
      owner_(owner),
      profile_(profile) {
}

LocationBarViewMac::ContentSettingImageView::~ContentSettingImageView() {}

void LocationBarViewMac::ContentSettingImageView::OnMousePressed(NSRect bounds)
{
  // Get host. This should be shared shared on linux/win/osx medium-term.
  TabContents* tabContents =
      BrowserList::GetLastActive()->GetSelectedTabContents();
  if (!tabContents)
    return;
  GURL url = tabContents->GetURL();
  std::wstring displayHost;
  net::AppendFormattedHost(
      url,
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      &displayHost, NULL, NULL);

  // Transform mouse coordinates to screen space.
  AutocompleteTextField* textField = owner_->GetAutocompleteTextField();
  NSWindow* window = [textField window];
  bounds = [textField convertRect:bounds toView:nil];
  NSPoint anchor = NSMakePoint(NSMidX(bounds) + 1, NSMinY(bounds));
  anchor = [window convertBaseToScreen:anchor];

  // Open bubble.
  ContentSettingBubbleModel* model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          tabContents, profile_,
          content_setting_image_model_->get_content_settings_type());
  [ContentBlockedBubbleController showForModel:model
                                   parentWindow:window
                                     anchoredAt:anchor];
}

NSString* LocationBarViewMac::ContentSettingImageView::GetToolTip() {
  return tooltip_.get();
}

void LocationBarViewMac::ContentSettingImageView::UpdateFromTabContents(
    const TabContents* tab_contents) {
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  if (content_setting_image_model_->is_visible()) {
    // TODO(thakis): We should use pdfs for these icons on OSX.
    // http://crbug.com/35847
    SetIcon(content_setting_image_model_->get_icon());
    SetToolTip(base::SysUTF8ToNSString(
        content_setting_image_model_->get_tooltip()));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

void LocationBarViewMac::ContentSettingImageView::SetToolTip(NSString* tooltip)
{
  tooltip_.reset([tooltip retain]);
}

// PageActionViewList-----------------------------------------------------------

void LocationBarViewMac::PageActionViewList::DeleteAll() {
  if (!views_.empty()) {
    STLDeleteContainerPointers(views_.begin(), views_.end());
    views_.clear();
  }
}

void LocationBarViewMac::PageActionViewList::RefreshViews() {
  if (!owner_->IsEditable()) {
    DeleteAll();
    return;
  }

  std::vector<ExtensionAction*> page_actions;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any Page Actions) load.
  if (page_actions.size() != views_.size()) {
    DeleteAll();  // Delete the old views (if any).

    views_.resize(page_actions.size());

    for (size_t i = 0; i < page_actions.size(); ++i) {
      views_[i] = new PageActionImageView(owner_, profile_, page_actions[i]);
      views_[i]->SetVisible(false);
    }
  }

  if (views_.empty())
    return;

  TabContents* contents = owner_->GetTabContents();
  if (!contents)
    return;

  GURL url = GURL(WideToUTF8(toolbar_model_->GetText()));
  for (size_t i = 0; i < views_.size(); ++i)
    views_[i]->UpdateVisibility(contents, url);
}

LocationBarViewMac::PageActionImageView*
    LocationBarViewMac::PageActionViewList::ViewAt(size_t index) {
  CHECK(index < Count());
  return views_[index];
}

size_t LocationBarViewMac::PageActionViewList::Count() {
  return views_.size();
}

size_t LocationBarViewMac::PageActionViewList::VisibleCount() {
  size_t result = 0;
  for (size_t i = 0; i < views_.size(); ++i) {
    if (views_[i]->IsVisible())
      ++result;
  }
  return result;
}

void LocationBarViewMac::PageActionViewList::OnMousePressed(NSRect iconFrame,
                                                            size_t index) {
  ViewAt(index)->OnMousePressed(iconFrame);
}

// TODO(shess): This function should over time grow to closely match
// the views Layout() function.
void LocationBarViewMac::Layout() {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  // Reset the left-hand decorations.
  // TODO(shess): Shortly, this code will live somewhere else, like in
  // the constructor.  I am still wrestling with how best to deal with
  // right-hand decorations, which are not a static set.
  [cell clearDecorations];
  [cell addLeftDecoration:location_icon_decoration_.get()];
  [cell addLeftDecoration:selected_keyword_decoration_.get()];
  [cell addLeftDecoration:ev_bubble_decoration_.get()];

  // By default only the location icon is visible.
  location_icon_decoration_->SetVisible(true);
  selected_keyword_decoration_->SetVisible(false);
  ev_bubble_decoration_->SetVisible(false);

  // Get the keyword to use for keyword-search and hinting.
  const std::wstring keyword(edit_view_->model()->keyword());
  std::wstring short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = profile_->GetTemplateURLModel()->
        GetKeywordShortName(keyword, &is_extension_keyword);
  }

  const bool is_keyword_hint = edit_view_->model()->is_keyword_hint();

  if (!keyword.empty() && !is_keyword_hint) {
    // Switch from location icon to keyword mode.
    location_icon_decoration_->SetVisible(false);
    selected_keyword_decoration_->SetVisible(true);

    selected_keyword_decoration_->SetKeyword(short_name, is_extension_keyword);

    // TODO(shess): This goes away once the hints are decorations.
    [cell clearHint];
  } else if (toolbar_model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    std::wstring label(toolbar_model_->GetEVCertName());
    ev_bubble_decoration_->SetLabel(base::SysWideToNSString(label));

    // TODO(shess): This goes away once the hints are decorations.
    [cell clearHint];
  } else if (!keyword.empty() && is_keyword_hint) {
    // Keyword is a hint, like "Press [Tab] to search Engine".  [Tab]
    // is a parameter to be replaced by an image.  "Engine" is a
    // parameter to be replaced by text based on the keyword.
    std::vector<size_t> content_param_offsets;
    int message_id = is_extension_keyword ?
        IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
    const std::wstring keyword_hint(
        l10n_util::GetStringF(message_id,
                              std::wstring(), short_name,
                              &content_param_offsets));

    // Should always be 2 offsets, see the comment in
    // location_bar_view.cc after IDS_OMNIBOX_KEYWORD_HINT fetch.
    DCHECK_EQ(content_param_offsets.size(), 2U);

    // Where to put the [TAB] image.
    const size_t split(content_param_offsets.front());

    NSString* prefix = base::SysWideToNSString(keyword_hint.substr(0, split));
    NSString* suffix = base::SysWideToNSString(keyword_hint.substr(split));

    NSImage* image = GetTabButtonImage();
    const CGFloat availableWidth([field_ availableDecorationWidth]);
    [cell setKeywordHintPrefix:prefix image:image suffix:suffix
                availableWidth:availableWidth];
  } else {
    // Nothing interesting to show, plain old text field.
    [cell clearHint];
  }

  // These need to change anytime the layout changes.
  // TODO(shess): Anytime the field editor might have changed, the
  // cursor rects almost certainly should have changed.  The tooltips
  // might change even when the rects don't change.
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ updateCursorAndToolTipRects];

  [field_ setNeedsDisplay:YES];
}
