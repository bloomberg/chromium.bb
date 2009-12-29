// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_view_mac.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#include "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"

// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

namespace {

// Returns the short name for a keyword.
// TODO(shess): Copied from views/location_bar_view.cc.  Try to share
// it.
std::wstring GetKeywordName(Profile* profile, const std::wstring& keyword) {
// Make sure the TemplateURL still exists.
// TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
// to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url)
    return template_url->AdjustedShortNameForLocaleDirection();
  return std::wstring();
}

// Values for the green text color displayed for EV certificates, based
// on the values for kEvTextColor in location_bar_view_gtk.cc.
static const CGFloat kEvTextColorRedComponent = 0.0;
static const CGFloat kEvTextColorGreenComponent = 0.59;
static const CGFloat kEvTextColorBlueComponent = 0.08;

// Build a short string to use in keyword-search when the field isn't
// very big.
// TODO(shess): Copied from views/location_bar_view.cc.  Try to share.
std::wstring CalculateMinString(const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  std::wstring min_string;
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = l10n_util::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  l10n_util::AdjustStringForLocaleDirection(min_string, &min_string);
  return min_string;
}

}  // namespace

LocationBarViewMac::LocationBarViewMac(
    AutocompleteTextField* field,
    const BubblePositioner* bubble_positioner,
    CommandUpdater* command_updater,
    ToolbarModel* toolbar_model,
    Profile* profile)
    : edit_view_(new AutocompleteEditViewMac(this, bubble_positioner,
          toolbar_model, profile, command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      security_image_view_(profile, toolbar_model),
      page_action_views_(new PageActionViewList(this, profile, toolbar_model)),
      profile_(profile),
      toolbar_model_(toolbar_model),
      transition_(PageTransition::TYPED) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setSecurityImageView:&security_image_view_];
  [cell setPageActionViewList:page_action_views_];

  registrar_.Add(this,
      NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      NotificationService::AllSources());
}

LocationBarViewMac::~LocationBarViewMac() {
  // TODO(shess): Placeholder for omnibox changes.
  delete page_action_views_;
}

std::wstring LocationBarViewMac::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition_;
}

// TODO(shess): Verify that this TODO is TODONE.
// TODO(rohitrao): Fix this to return different types once autocomplete and
// the onmibar are implemented.  For now, any URL that comes from the
// LocationBar has to have been entered by the user, and thus is of type
// PageTransition::TYPED.
PageTransition::Type LocationBarViewMac::GetPageTransition() const {
  return transition_;
}

void LocationBarViewMac::AcceptInput() {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  AcceptInputWithDisposition(disposition);
}

void LocationBarViewMac::AcceptInputWithDisposition(
    WindowOpenDisposition disposition) {
  edit_view_->model()->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation() {
  edit_view_->FocusLocation();
}

void LocationBarViewMac::FocusSearch() {
  edit_view_->SetForcedQuery();
  // TODO(pkasting): Focus the edit a la Linux/Win
}

void LocationBarViewMac::UpdatePageActions() {
  page_action_views_->RefreshViews();
  [field_ resetFieldEditorFrameIfNeeded];
}

void LocationBarViewMac::InvalidatePageActions() {
  page_action_views_->DeleteAll();
}

void LocationBarViewMac::SaveStateToContents(TabContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  edit_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Update(const TabContents* contents,
                                bool should_restore_state) {
  SetSecurityIcon(toolbar_model_->GetIcon());
  page_action_views_->RefreshViews();
  // AutocompleteEditView restores state if the tab is non-NULL.
  edit_view_->Update(should_restore_state ? contents : NULL);
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

  scoped_ptr<AlternateNavURLFetcher> fetcher(
      new AlternateNavURLFetcher(alternate_nav_url));
  // The AlternateNavURLFetcher will listen for the pending navigation
  // notification that will be issued as a result of the "open URL." It
  // will automatically install itself into that navigation controller.
  command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
  if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
    // I'm not sure this should be reachable, but I'm not also sure enough
    // that it shouldn't to stick in a NOTREACHED().  In any case, this is
    // harmless; we can simply let the fetcher get deleted here and it will
    // clean itself up properly.
  } else {
    fetcher.release();  // The navigation controller will delete the fetcher.
  }
}

void LocationBarViewMac::OnChangedImpl(AutocompleteTextField* field,
                                       const std::wstring& keyword,
                                       const std::wstring& short_name,
                                       const bool is_keyword_hint,
                                       const bool show_search_hint,
                                       NSImage* image) {
  AutocompleteTextFieldCell* cell = [field autocompleteTextFieldCell];
  const CGFloat availableWidth([field availableDecorationWidth]);

  if (!keyword.empty() && !is_keyword_hint) {
    // Keyword search mode.  The text will be like "Search Engine:".
    // "Engine" is a parameter to be replaced by text based on the
    // keyword.

    const std::wstring min_name(CalculateMinString(short_name));
    NSString* partial_string = nil;
    if (!min_name.empty()) {
      partial_string =
          l10n_util::GetNSStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                  WideToUTF16(min_name));
    }

    NSString* keyword_string =
        l10n_util::GetNSStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                WideToUTF16(short_name));
    [cell setKeywordString:keyword_string
             partialString:partial_string
            availableWidth:availableWidth];
  } else if (!keyword.empty() && is_keyword_hint) {
    // Keyword is a hint, like "Press [Tab] to search Engine".  [Tab]
    // is a parameter to be replaced by an image.  "Engine" is a
    // parameter to be replaced by text based on the keyword.
    std::vector<size_t> content_param_offsets;
    const std::wstring keyword_hint(
        l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_HINT,
                              std::wstring(), short_name,
                              &content_param_offsets));

    // Should always be 2 offsets, see the comment in
    // location_bar_view.cc after IDS_OMNIBOX_KEYWORD_HINT fetch.
    DCHECK_EQ(content_param_offsets.size(), 2U);

    // Where to put the [TAB] image.
    const size_t split(content_param_offsets.front());

    NSString* prefix = base::SysWideToNSString(keyword_hint.substr(0, split));
    NSString* suffix = base::SysWideToNSString(keyword_hint.substr(split));

    [cell setKeywordHintPrefix:prefix image:image suffix:suffix
                availableWidth:availableWidth];
  } else if (show_search_hint) {
    // Show a search hint right-justified in the field if there is no
    // keyword.
    const std::wstring hint(l10n_util::GetString(IDS_OMNIBOX_EMPTY_TEXT));
    [cell setSearchHintString:base::SysWideToNSString(hint)
               availableWidth:availableWidth];
  } else {
    // Nothing interesting to show, plain old text field.
    [cell clearKeywordAndHint];
  }

  // The field needs to re-layout if the visible decoration changed.
  [field resetFieldEditorFrameIfNeeded];
}

void LocationBarViewMac::OnChanged() {
  // Unfortunately, the unit-test Profile doesn't have the right stuff
  // setup to do what GetKeywordName() needs to do.  So do that out
  // here where we have a Profile and pass it into OnChangedImpl().
  const std::wstring keyword(edit_view_->model()->keyword());
  std::wstring short_name;
  if (!keyword.empty()) {
    short_name = GetKeywordName(profile_, keyword);
  }

  // TODO(shess): Implementation exported to a static so that it can
  // be unit tested without having to setup the entire object.  This
  // makes me sad.  I should fix that.
  OnChangedImpl(field_,
                keyword,
                short_name,
                edit_view_->model()->is_keyword_hint(),
                edit_view_->model()->show_search_hint(),
                GetTabButtonImage());
}

void LocationBarViewMac::OnInputInProgress(bool in_progress) {
  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL, false);
}

void LocationBarViewMac::OnKillFocus() {
}

void LocationBarViewMac::OnSetFocus() {
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
  return static_cast<int>(page_action_views_->Count());
}

int LocationBarViewMac::PageActionVisibleCount() {
  return static_cast<int>(page_action_views_->VisibleCount());
}

ExtensionAction* LocationBarViewMac::GetPageAction(size_t index) {
  if (index < page_action_views_->Count())
    return page_action_views_->ViewAt(index)->page_action();
  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarViewMac::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_->Count(); ++i) {
    if (page_action_views_->ViewAt(i)->IsVisible()) {
      if (current == index)
        return page_action_views_->ViewAt(i)->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewMac::TestPageActionPressed(size_t index) {
  NOTIMPLEMENTED();
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

void LocationBarViewMac::SetSecurityIconLabel() {
  std::wstring info_text;
  std::wstring info_tooltip;
  ToolbarModel::InfoTextType info_text_type =
      toolbar_model_->GetInfoText(&info_text, &info_tooltip);
  if (info_text_type == ToolbarModel::INFO_EV_TEXT) {
    NSString* icon_label = base::SysWideToNSString(info_text);
    NSColor* color = [NSColor colorWithCalibratedRed:kEvTextColorRedComponent
                                               green:kEvTextColorGreenComponent
                                                blue:kEvTextColorBlueComponent
                                               alpha:1.0];
    security_image_view_.SetLabel(icon_label, [field_ font], color);
  } else {
    security_image_view_.SetLabel(nil, nil, nil);
  }
}

void LocationBarViewMac::SetSecurityIcon(ToolbarModel::Icon icon) {
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      security_image_view_.SetImageShown(SecurityImageView::LOCK);
      security_image_view_.SetVisible(true);
      SetSecurityIconLabel();
      break;
    case ToolbarModel::WARNING_ICON:
      security_image_view_.SetImageShown(SecurityImageView::WARNING);
      security_image_view_.SetVisible(true);
      SetSecurityIconLabel();
      break;
    case ToolbarModel::NO_ICON:
      security_image_view_.SetVisible(false);
      break;
    default:
      NOTREACHED();
      security_image_view_.SetVisible(false);
      break;
  }
  [field_ resetFieldEditorFrameIfNeeded];
}

void LocationBarViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
      TabContents* contents =
          BrowserList::GetLastActive()->GetSelectedTabContents();
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

// LocationBarImageView---------------------------------------------------------

void LocationBarViewMac::LocationBarImageView::SetImage(NSImage* image) {
  image_.reset([image retain]);
}

void LocationBarViewMac::LocationBarImageView::SetImage(SkBitmap* image) {
  SetImage(gfx::SkBitmapToNSImage(*image));
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

// SecurityImageView------------------------------------------------------------

LocationBarViewMac::SecurityImageView::SecurityImageView(
    Profile* profile,
    ToolbarModel* model)
    : lock_icon_(nil),
      warning_icon_(nil),
      profile_(profile),
      model_(model) {}

LocationBarViewMac::SecurityImageView::~SecurityImageView() {}

void LocationBarViewMac::SecurityImageView::SetImageShown(Image image) {
  switch (image) {
    case LOCK:
      if (!lock_icon_.get()) {
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        lock_icon_.reset([rb.GetNSImageNamed(IDR_LOCK) retain]);
      }
      SetImage(lock_icon_);
      break;
    case WARNING:
      if (!warning_icon_.get()) {
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        warning_icon_.reset([rb.GetNSImageNamed(IDR_WARNING) retain]);
      }
      SetImage(warning_icon_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool LocationBarViewMac::SecurityImageView::OnMousePressed() {
  TabContents* tab = BrowserList::GetLastActive()->GetSelectedTabContents();
  NavigationEntry* nav_entry = tab->controller().GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return true;
  }
  tab->ShowPageInfo(nav_entry->url(), nav_entry->ssl(), true);
  return true;
}

// PageActionImageView----------------------------------------------------------

LocationBarViewMac::PageActionImageView::PageActionImageView(
    LocationBarViewMac* owner,
    Profile* profile,
    ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      popup_controller_(nil),
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

  tracker_ = new ImageLoadingTracker(this, icon_paths.size());
  for (std::vector<std::string>::iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    tracker_->PostLoadImageTask(extension->GetResource(*iter),
                                gfx::Size(Extension::kPageActionIconMaxSize,
                                          Extension::kPageActionIconMaxSize));
  }

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      Source<Profile>(profile_));
}

LocationBarViewMac::PageActionImageView::~PageActionImageView() {
  if (tracker_)
    tracker_->StopTrackingImageLoad();
}

// Overridden from LocationBarImageView. Either notify listeners or show a
// popup depending on the Page Action.
bool LocationBarViewMac::PageActionImageView::OnMousePressed(NSRect bounds) {
  if (page_action_->has_popup()) {
    AutocompleteTextField* textField = owner_->GetAutocompleteTextField();
    NSWindow* window = [textField window];
    NSRect relativeBounds = [[window contentView] convertRect:bounds
                                                     fromView:textField];
    NSPoint arrowPoint = [window convertBaseToScreen:NSMakePoint(
        NSMinX(relativeBounds),
        NSMinY(relativeBounds))];

    // Adjust the anchor point to be at the center of the page action icon.
    arrowPoint.x += [GetImage() size].width / 2;

    popup_controller_ =
        [ExtensionPopupController showURL:page_action_->popup_url()
                                inBrowser:BrowserList::GetLastActive()
                               anchoredAt:arrowPoint
                            arrowLocation:kTopRight];
    } else {
      ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
          profile_, page_action_->extension_id(), page_action_->id(),
          current_tab_id_, current_url_.spec(),
          1);  // TODO(pamg): Add support for middle and right buttons.
  }
  return true;
}

void LocationBarViewMac::PageActionImageView::OnImageLoaded(SkBitmap* image,
                                                            size_t index) {
  // We loaded icons()->size() icons, plus one extra if the Page Action had
  // a default icon.
  int total_icons = page_action_->icon_paths()->size();
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(static_cast<int>(index) < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    if (index < page_action_->icon_paths()->size())
      page_action_icons_[page_action_->icon_paths()->at(index)] = *image;
    else
      page_action_icons_[page_action_->default_icon_path()] = *image;
  }

  // If we are done, release the tracker.
  if (static_cast<int>(index) == (total_icons - 1))
    tracker_ = NULL;

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
  SetVisible(visible);
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

const NSString* LocationBarViewMac::PageActionImageView::GetToolTip() {
  return tooltip_.get();
}

void LocationBarViewMac::PageActionImageView::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      if (popup_controller_ &&
          Details<ExtensionHost>([popup_controller_ host]) == details) {
        HidePopup();
      }
      break;
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

void LocationBarViewMac::PageActionImageView::HidePopup() {
  [popup_controller_ close];
  popup_controller_ = nil;
}

// PageActionViewList-----------------------------------------------------------

void LocationBarViewMac::PageActionViewList::DeleteAll() {
  if (!views_.empty()) {
    STLDeleteContainerPointers(views_.begin(), views_.end());
    views_.clear();
  }
}

void LocationBarViewMac::PageActionViewList::RefreshViews() {
  std::vector<ExtensionAction*> page_actions;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  // Remember the previous visibility of the Page Actions so that we can
  // notify when this changes.
  std::map<ExtensionAction*, bool> old_visibility;
  for (size_t i = 0; i < views_.size(); ++i) {
    old_visibility[views_[i]->page_action()] = views_[i]->IsVisible();
  }

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

  Browser* browser = BrowserList::GetLastActive();
  // The last-active browser can be NULL during startup.
  if (!browser)
    return;

  TabContents* contents = browser->GetSelectedTabContents();
  if (!contents)
    return;

  GURL url = GURL(WideToUTF8(toolbar_model_->GetText()));
  for (size_t i = 0; i < views_.size(); ++i) {
    views_[i]->UpdateVisibility(contents, url);
    // Check if the visibility of the action changed and notify if it did.
    ExtensionAction* action = views_[i]->page_action();
    if (old_visibility.find(action) == old_visibility.end() ||
        old_visibility[action] != views_[i]->IsVisible()) {
      NotificationService::current()->Notify(
          NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
          Source<ExtensionAction>(action),
          Details<TabContents>(contents));
    }
  }
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
