// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_view_mac.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#include "chrome/browser/cocoa/event_utils.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
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

}  // namespace

LocationBarViewMac::LocationBarViewMac(AutocompleteTextField* field,
                                       CommandUpdater* command_updater,
                                       ToolbarModel* toolbar_model,
                                       Profile* profile)
  : edit_view_(new AutocompleteEditViewMac(this, toolbar_model, profile,
                                           command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      profile_(profile),
      transition_(PageTransition::TYPED) {
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

  if (!keyword.empty() && !is_keyword_hint) {
    // Keyword search mode.  The text will be like "Search Engine:".
    // "Engine" is a parameter to be replaced by text based on the
    // keyword.

    // TODO(shess): This needs to additionally support a minimized
    // version, to be used when the string below is too long.
    const std::wstring keyword_text(
        l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, short_name));
    [cell setKeywordString:base::SysWideToNSString(keyword_text)];
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

    [cell setKeywordHintPrefix:prefix image:image suffix:suffix];
  } else if (show_search_hint) {
    // Show a search hint right-justified in the field if there is no
    // keyword.
    const std::wstring hint(l10n_util::GetString(IDS_OMNIBOX_EMPTY_TEXT));
    [cell setSearchHintString:base::SysWideToNSString(hint)];
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
  NOTIMPLEMENTED();
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
