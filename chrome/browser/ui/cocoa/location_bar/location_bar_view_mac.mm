// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#import "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
#import "chrome/browser/ui/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

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
    : edit_view_(new AutocompleteEditViewMac(this, toolbar_model, profile,
                                             command_updater, field)),
      command_updater_(command_updater),
      field_(field),
      disposition_(CURRENT_TAB),
      location_icon_decoration_(new LocationIconDecoration(this)),
      selected_keyword_decoration_(
          new SelectedKeywordDecoration(
              AutocompleteEditViewMac::GetFieldFont())),
      ev_bubble_decoration_(
          new EVBubbleDecoration(location_icon_decoration_.get(),
                                 AutocompleteEditViewMac::GetFieldFont())),
      star_decoration_(new StarDecoration(command_updater)),
      keyword_hint_decoration_(
          new KeywordHintDecoration(AutocompleteEditViewMac::GetFieldFont())),
      profile_(profile),
      browser_(browser),
      toolbar_model_(toolbar_model),
      update_instant_(true),
      transition_(PageTransition::TYPED),
      first_run_bubble_(this) {
  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    DCHECK_EQ(i, content_setting_decorations_.size());
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    content_setting_decorations_.push_back(
        new ContentSettingDecoration(type, this, profile_));
  }

  registrar_.Add(this,
      NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      NotificationService::AllSources());
}

LocationBarViewMac::~LocationBarViewMac() {
  // Disconnect from cell in case it outlives us.
  [[field_ cell] clearDecorations];
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

  // The first run bubble's left edge should line up with the left edge of the
  // omnibox. This is different from other bubbles, which line up at a point
  // set by their top arrow. Because the BaseBubbleController adjusts the
  // window origin left to account for the arrow spacing, the first run bubble
  // moves the window origin right by this spacing, so that the
  // BaseBubbleController will move it back to the correct position.
  const NSPoint kOffset = NSMakePoint(
      info_bubble::kBubbleArrowXOffset + info_bubble::kBubbleArrowWidth/2.0,
      kFirstRunBubbleYOffset);
  [FirstRunBubbleController showForView:field_ offset:kOffset profile:profile_];
}

std::wstring LocationBarViewMac::GetInputString() const {
  return location_input_;
}

void LocationBarViewMac::SetSuggestedText(const string16& text) {
  edit_view_->SetInstantSuggestion(
      edit_view_->model()->UseVerbatimInstant() ? string16() : text);
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
  if (RefreshContentSettingsDecorations()) {
    [field_ updateCursorAndToolTipRects];
    [field_ setNeedsDisplay:YES];
  }
}

void LocationBarViewMac::UpdatePageActions() {
  size_t count_before = page_action_decorations_.size();
  RefreshPageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarViewMac::InvalidatePageActions() {
  size_t count_before = page_action_decorations_.size();
  DeletePageActionDecorations();
  Layout();
  if (page_action_decorations_.size() != count_before) {
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
  bool star_enabled = browser_defaults::bookmarks_enabled &&
      [field_ isEditable] && !toolbar_model_->input_in_progress();
  command_updater_->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, star_enabled);
  star_decoration_->SetVisible(star_enabled);
  RefreshPageActionDecorations();
  RefreshContentSettingsDecorations();
  // AutocompleteEditView restores state if the tab is non-NULL.
  edit_view_->Update(should_restore_state ? contents : NULL);
  OnChanged();
}

void LocationBarViewMac::OnAutocompleteWillClosePopup() {
  if (!update_instant_)
    return;

  InstantController* controller = browser_->instant();
  if (controller && !controller->commit_on_mouse_up())
    controller->DestroyPreviewContents();
  SetSuggestedText(string16());
}

void LocationBarViewMac::OnAutocompleteLosingFocus(gfx::NativeView unused) {
  SetSuggestedText(string16());

  InstantController* instant = browser_->instant();
  if (!instant)
    return;

  // If |IsMouseDownFromActivate()| returns false, the RenderWidgetHostView did
  // not receive a mouseDown event.  Therefore, we should destroy the preview.
  // Otherwise, the RWHV was clicked, so we commit the preview.
  if (!instant->is_displayable() || !instant->GetPreviewContents() ||
      !instant->IsMouseDownFromActivate()) {
    instant->DestroyPreviewContents();
  } else if (instant->IsShowingInstant()) {
    instant->SetCommitOnMouseUp();
  } else {
    instant->CommitCurrentPreview(INSTANT_COMMIT_FOCUS_LOST);
  }
}

void LocationBarViewMac::OnAutocompleteWillAccept() {
  update_instant_ = false;
}

bool LocationBarViewMac::OnCommitSuggestedText(const std::wstring& typed_text) {
  return edit_view_->CommitSuggestText();
}

bool LocationBarViewMac::AcceptCurrentInstantPreview() {
  return InstantController::CommitIfCurrent(browser_->instant());
}

void LocationBarViewMac::OnPopupBoundsChanged(const gfx::Rect& bounds) {
  InstantController* instant = browser_->instant();
  if (instant)
    instant->SetOmniboxBounds(bounds);
}

void LocationBarViewMac::OnAutocompleteAccept(const GURL& url,
                                              WindowOpenDisposition disposition,
                                              PageTransition::Type transition,
                                              const GURL& alternate_nav_url) {
  // WARNING: don't add an early return here. The calls after the if must
  // happen.
  if (url.is_valid()) {
    location_input_ = UTF8ToWide(url.spec());
    disposition_ = disposition;
    transition_ = transition;

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

  if (browser_->instant() && !edit_view_->model()->popup_model()->IsOpen())
    browser_->instant()->DestroyPreviewContents();

  update_instant_ = true;
}

void LocationBarViewMac::OnChanged() {
  // Update the location-bar icon.
  const int resource_id = edit_view_->GetIcon();
  NSImage* image = AutocompleteEditViewMac::ImageForResource(resource_id);
  location_icon_decoration_->SetImage(image);
  ev_bubble_decoration_->SetImage(image);
  Layout();

  InstantController* instant = browser_->instant();
  string16 suggested_text;
  if (update_instant_ && instant && GetTabContents()) {
    if (edit_view_->model()->user_input_in_progress() &&
        edit_view_->model()->popup_model()->IsOpen()) {
      instant->Update
          (browser_->GetSelectedTabContentsWrapper(),
           edit_view_->model()->CurrentMatch(),
           WideToUTF16(edit_view_->GetText()),
           edit_view_->model()->UseVerbatimInstant(),
           &suggested_text);
      if (!instant->MightSupportInstant()) {
        edit_view_->model()->FinalizeInstantQuery(std::wstring(),
                                                  std::wstring());
      }
    } else {
      instant->DestroyPreviewContents();
      edit_view_->model()->FinalizeInstantQuery(std::wstring(),
                                                std::wstring());
    }
  }

  SetSuggestedText(suggested_text);
}

void LocationBarViewMac::OnSelectionBoundsChanged() {
  NOTIMPLEMENTED();
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

TabContents* LocationBarViewMac::GetTabContents() const {
  return browser_->GetSelectedTabContents();
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
  TabContents* contents = GetTabContents();
  if (!contents)
    return;
  RefreshPageActionDecorations();
  Layout();

  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  DCHECK(decoration);
  if (!decoration)
    return;

  decoration->set_preview_enabled(preview_enabled);
  decoration->UpdateVisibility(contents,
      GURL(WideToUTF8(toolbar_model_->GetText())));
}

NSPoint LocationBarViewMac::GetPageActionBubblePoint(
    ExtensionAction* page_action) {
  PageActionDecoration* decoration = GetPageActionDecoration(page_action);
  if (!decoration)
    return NSZeroPoint;

  AutocompleteTextFieldCell* cell = [field_ cell];
  NSRect frame = [cell frameForDecoration:decoration inFrame:[field_ bounds]];
  DCHECK(!NSIsEmptyRect(frame));
  if (NSIsEmptyRect(frame))
    return NSZeroPoint;

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

void LocationBarViewMac::SetEditable(bool editable) {
  [field_ setEditable:editable ? YES : NO];
  star_decoration_->SetVisible(browser_defaults::bookmarks_enabled &&
      editable && !toolbar_model_->input_in_progress());
  UpdatePageActions();
  Layout();
}

bool LocationBarViewMac::IsEditable() {
  return [field_ isEditable] ? true : false;
}

void LocationBarViewMac::SetStarred(bool starred) {
  star_decoration_->SetStarred(starred);

  // TODO(shess): The field-editor frame and cursor rects should not
  // change, here.
  [field_ updateCursorAndToolTipRects];
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ setNeedsDisplay:YES];
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

NSImage* LocationBarViewMac::GetKeywordImage(const std::wstring& keyword) {
  const TemplateURL* template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(
          WideToUTF16Hack(keyword));
  if (template_url && template_url->IsExtensionKeyword()) {
    const SkBitmap& bitmap = profile_->GetExtensionService()->
        GetOmniboxIcon(template_url->GetExtensionId());
    return gfx::SkBitmapToNSImage(bitmap);
  }

  return AutocompleteEditViewMac::ImageForResource(IDR_OMNIBOX_SEARCH);
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

bool LocationBarViewMac::RefreshContentSettingsDecorations() {
  const bool input_in_progress = toolbar_model_->input_in_progress();
  TabContents* tab_contents =
      input_in_progress ? NULL : browser_->GetSelectedTabContents();
  bool icons_updated = false;
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    icons_updated |=
        content_setting_decorations_[i]->UpdateFromTabContents(tab_contents);
  }
  return icons_updated;
}

void LocationBarViewMac::DeletePageActionDecorations() {
  // TODO(shess): Deleting these decorations could result in the cell
  // refering to them before things are laid out again.  Meanwhile, at
  // least fail safe.
  [[field_ cell] clearDecorations];

  page_action_decorations_.reset();
}

void LocationBarViewMac::RefreshPageActionDecorations() {
  if (!IsEditable()) {
    DeletePageActionDecorations();
    return;
  }

  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;

  std::vector<ExtensionAction*> page_actions;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any Page Actions) load.
  if (page_actions.size() != page_action_decorations_.size()) {
    DeletePageActionDecorations();  // Delete the old views (if any).

    for (size_t i = 0; i < page_actions.size(); ++i) {
      page_action_decorations_.push_back(
          new PageActionDecoration(this, profile_, page_actions[i]));
    }
  }

  if (page_action_decorations_.empty())
    return;

  TabContents* contents = GetTabContents();
  if (!contents)
    return;

  GURL url = GURL(WideToUTF8(toolbar_model_->GetText()));
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    page_action_decorations_[i]->UpdateVisibility(
        toolbar_model_->input_in_progress() ? NULL : contents, url);
  }
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

  // Note that display order is right to left.
  for (size_t i = 0; i < page_action_decorations_.size(); ++i) {
    [cell addRightDecoration:page_action_decorations_[i]];
  }
  for (size_t i = 0; i < content_setting_decorations_.size(); ++i) {
    [cell addRightDecoration:content_setting_decorations_[i]];
  }

  [cell addRightDecoration:keyword_hint_decoration_.get()];

  // By default only the location icon is visible.
  location_icon_decoration_->SetVisible(true);
  selected_keyword_decoration_->SetVisible(false);
  ev_bubble_decoration_->SetVisible(false);
  keyword_hint_decoration_->SetVisible(false);

  // Get the keyword to use for keyword-search and hinting.
  const std::wstring keyword(edit_view_->model()->keyword());
  string16 short_name;
  bool is_extension_keyword = false;
  if (!keyword.empty()) {
    short_name = profile_->GetTemplateURLModel()->
        GetKeywordShortName(WideToUTF16Hack(keyword), &is_extension_keyword);
  }

  const bool is_keyword_hint = edit_view_->model()->is_keyword_hint();

  if (!keyword.empty() && !is_keyword_hint) {
    // Switch from location icon to keyword mode.
    location_icon_decoration_->SetVisible(false);
    selected_keyword_decoration_->SetVisible(true);
    selected_keyword_decoration_->SetKeyword(UTF16ToWideHack(short_name),
                                             is_extension_keyword);
    selected_keyword_decoration_->SetImage(GetKeywordImage(keyword));
  } else if (toolbar_model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    // Switch from location icon to show the EV bubble instead.
    location_icon_decoration_->SetVisible(false);
    ev_bubble_decoration_->SetVisible(true);

    std::wstring label(toolbar_model_->GetEVCertName());
    ev_bubble_decoration_->SetFullLabel(base::SysWideToNSString(label));
  } else if (!keyword.empty() && is_keyword_hint) {
    keyword_hint_decoration_->SetKeyword(short_name,
                                         is_extension_keyword);
    keyword_hint_decoration_->SetVisible(true);
  }

  // These need to change anytime the layout changes.
  // TODO(shess): Anytime the field editor might have changed, the
  // cursor rects almost certainly should have changed.  The tooltips
  // might change even when the rects don't change.
  [field_ resetFieldEditorFrameIfNeeded];
  [field_ updateCursorAndToolTipRects];

  [field_ setNeedsDisplay:YES];
}
