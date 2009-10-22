// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_view_mac.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
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
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

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
      profile_(profile),
      toolbar_model_(toolbar_model),
      transition_(PageTransition::TYPED) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setSecurityImageView:&security_image_view_];
}

LocationBarViewMac::~LocationBarViewMac() {
  // TODO(shess): Placeholder for omnibox changes.
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
  edit_view_->AcceptInput(disposition, false);
}

void LocationBarViewMac::FocusLocation() {
  edit_view_->FocusLocation();
}

void LocationBarViewMac::FocusSearch() {
  edit_view_->SetForcedQuery();
  // TODO(pkasting): Focus the edit a la Linux/Win
}

void LocationBarViewMac::SaveStateToContents(TabContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  edit_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::Update(const TabContents* contents,
                                bool should_restore_state) {
  SetSecurityIcon(toolbar_model_->GetIcon());
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

int LocationBarViewMac::PageActionCount() {
  NOTIMPLEMENTED();
  return -1;
}

int LocationBarViewMac::PageActionVisibleCount() {
  NOTIMPLEMENTED();
  return -1;
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

// LocationBarImageView---------------------------------------------------------

void LocationBarViewMac::LocationBarImageView::SetImage(NSImage* image) {
  image_.reset([image retain]);
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
  DCHECK(!visible || image_);
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
