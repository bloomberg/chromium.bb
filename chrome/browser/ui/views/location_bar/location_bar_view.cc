// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/action_box_button_view.h"
#include "chrome/browser/ui/views/location_bar/chrome_to_mobile_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/label.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/first_run_bubble.h"
#endif

#if defined(OS_WIN) || defined(USE_AURA)
#include "chrome/browser/ui/views/location_bar/suggested_text_view.h"
#endif

using content::WebContents;
using views::View;

namespace {

WebContents* GetWebContentsFromDelegate(LocationBarView::Delegate* delegate) {
  const TabContentsWrapper* wrapper = delegate->GetTabContentsWrapper();
  return wrapper ? wrapper->web_contents() : NULL;
}

// A utility function to cast OmniboxView to OmniboxViewViews.
OmniboxViewViews* AsViews(OmniboxView* view) {
  return static_cast<OmniboxViewViews*>(view);
}

// Height of the location bar's round corner region.
const int kBorderRoundCornerHeight = 5;
// Width of location bar's round corner region.
const int kBorderRoundCornerWidth = 4;
// Radius of the round corners inside the location bar.
const int kBorderCornerRadius = 2;

const int kDesktopItemPadding = 3;
const int kDesktopEdgeItemPadding = kDesktopItemPadding;

const int kTouchItemPadding = 8;
const int kTouchEdgeItemPadding = kTouchItemPadding;

}  // namespace

// static
const int LocationBarView::kNormalHorizontalEdgeThickness = 2;
const int LocationBarView::kVerticalEdgeThickness = 3;
const int LocationBarView::kIconInternalPadding = 2;
const int LocationBarView::kBubbleHorizontalPadding = 1;
const char LocationBarView::kViewClassName[] =
    "browser/ui/views/location_bar/LocationBarView";

static const int kEVBubbleBackgroundImages[] = {
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_L,
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_C,
  IDR_OMNIBOX_EV_BUBBLE_BACKGROUND_R,
};

static const int kSelectedKeywordBackgroundImages[] = {
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_L,
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_C,
  IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_R,
};

// LocationBarView -----------------------------------------------------------

LocationBarView::LocationBarView(Profile* profile,
                                 CommandUpdater* command_updater,
                                 ToolbarModel* model,
                                 Delegate* delegate,
                                 Mode mode)
    : profile_(profile),
      command_updater_(command_updater),
      model_(model),
      delegate_(delegate),
      disposition_(CURRENT_TAB),
      transition_(content::PageTransitionFromInt(
          content::PAGE_TRANSITION_TYPED |
          content::PAGE_TRANSITION_FROM_ADDRESS_BAR)),
      location_icon_view_(NULL),
      ev_bubble_view_(NULL),
      location_entry_view_(NULL),
      selected_keyword_view_(NULL),
#if defined(OS_WIN) || defined(USE_AURA)
      suggested_text_view_(NULL),
#endif
      keyword_hint_view_(NULL),
      star_view_(NULL),
      action_box_button_view_(NULL),
      chrome_to_mobile_view_(NULL),
      mode_(mode),
      show_focus_rect_(false),
      template_url_service_(NULL),
      animation_offset_(0) {
  set_id(VIEW_ID_LOCATION_BAR);

  if (mode_ == NORMAL) {
    painter_.reset(
        views::Painter::CreateImagePainter(
            *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                IDR_LOCATION_BAR_BORDER).ToSkBitmap(),
            gfx::Insets(kBorderRoundCornerHeight, kBorderRoundCornerWidth,
                kBorderRoundCornerHeight, kBorderRoundCornerWidth),
            true));
  }

  edit_bookmarks_enabled_.Init(prefs::kEditBookmarksEnabled,
                               profile_->GetPrefs(), this);
}

LocationBarView::~LocationBarView() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
}

void LocationBarView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (mode_ == POPUP) {
    font_ = rb.GetFont(ui::ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = rb.GetFont(ui::ResourceBundle::MediumFont);
  }

  // If this makes the font too big, try to make it smaller so it will fit.
  const int height = GetHeight();
  while ((font_.GetHeight() > height) && (font_.GetFontSize() > 1))
    font_ = font_.DeriveFont(-1);

  location_icon_view_ = new LocationIconView(this);
  AddChildView(location_icon_view_);
  location_icon_view_->SetVisible(true);
  location_icon_view_->set_drag_controller(this);

  ev_bubble_view_ =
      new EVBubbleView(kEVBubbleBackgroundImages, IDR_OMNIBOX_HTTPS_VALID,
                       GetColor(ToolbarModel::EV_SECURE, SECURITY_TEXT), this);
  AddChildView(ev_bubble_view_);
  ev_bubble_view_->SetVisible(false);
  ev_bubble_view_->set_drag_controller(this);

  // URL edit field.
  // View container for URL edit field.
  location_entry_.reset(OmniboxView::CreateOmniboxView(
      this,
      model_,
      profile_,
      command_updater_,
      mode_ == POPUP,
      this));
  SetLocationEntryFocusable(true);

  location_entry_view_ = location_entry_->AddToView(this);
  location_entry_view_->set_id(VIEW_ID_AUTOCOMPLETE);

  selected_keyword_view_ = new SelectedKeywordView(
      kSelectedKeywordBackgroundImages, IDR_KEYWORD_SEARCH_MAGNIFIER,
      GetColor(ToolbarModel::NONE, TEXT), profile_);
  AddChildView(selected_keyword_view_);
  selected_keyword_view_->SetFont(font_);
  selected_keyword_view_->SetVisible(false);

  keyword_hint_view_ = new KeywordHintView(profile_);
  AddChildView(keyword_hint_view_);
  keyword_hint_view_->SetVisible(false);
  keyword_hint_view_->SetFont(font_);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_blocked_view =
        new ContentSettingImageView(static_cast<ContentSettingsType>(i), this);
    content_setting_views_.push_back(content_blocked_view);
    AddChildView(content_blocked_view);
    content_blocked_view->SetVisible(false);
  }

  if (extensions::switch_utils::IsActionBoxEnabled()) {
    action_box_button_view_ = new ActionBoxButtonView();
    AddChildView(action_box_button_view_);
  } else if (browser_defaults::bookmarks_enabled && (mode_ == NORMAL)) {
    // Hide the star and ChromeToMobile icons in popups and in the app launcher.
    star_view_ = new StarView(command_updater_);
    AddChildView(star_view_);
    star_view_->SetVisible(true);

    // Also disable Chrome To Mobile for off-the-record and non-synced profiles,
    // or if the feature is disabled by a command line flag or chrome://flags.
    if (!profile_->IsOffTheRecord() && profile_->IsSyncAccessible() &&
        ChromeToMobileService::IsChromeToMobileEnabled()) {
      chrome_to_mobile_view_ = new ChromeToMobileView(this, command_updater_);
      AddChildView(chrome_to_mobile_view_);
      ChromeToMobileService* service =
          ChromeToMobileServiceFactory::GetForProfile(profile_);
      service->RequestMobileListUpdate();
      chrome_to_mobile_view_->SetVisible(service->HasDevices());
    }
  }

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);

  OnChanged();
}

bool LocationBarView::IsInitialized() const {
  return location_entry_view_ != NULL;
}

// static
SkColor LocationBarView::GetColor(ToolbarModel::SecurityLevel security_level,
                                  ColorKind kind) {
  switch (kind) {
#if defined(OS_WIN)
    case BACKGROUND:    return color_utils::GetSysSkColor(COLOR_WINDOW);
    case TEXT:          return color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    case SELECTED_TEXT: return color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
#else
    // TODO(beng): source from theme provider.
    case BACKGROUND:    return SK_ColorWHITE;
    case TEXT:          return SK_ColorBLACK;
    case SELECTED_TEXT: return SK_ColorWHITE;
#endif

    case DEEMPHASIZED_TEXT:
      return color_utils::AlphaBlend(GetColor(security_level, TEXT),
                                     GetColor(security_level, BACKGROUND), 128);

    case SECURITY_TEXT: {
      SkColor color;
      switch (security_level) {
        case ToolbarModel::EV_SECURE:
        case ToolbarModel::SECURE:
          color = SkColorSetRGB(7, 149, 0);
          break;

        case ToolbarModel::SECURITY_WARNING:
          return GetColor(security_level, DEEMPHASIZED_TEXT);
          break;

        case ToolbarModel::SECURITY_ERROR:
          color = SkColorSetRGB(162, 0, 0);
          break;

        default:
          NOTREACHED();
          return GetColor(security_level, TEXT);
      }
      return color_utils::GetReadableColor(color, GetColor(security_level,
                                                           BACKGROUND));
    }

    default:
      NOTREACHED();
      return GetColor(security_level, TEXT);
  }
}

// static
int LocationBarView::GetItemPadding() {
  return (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) ?
      kTouchItemPadding : kDesktopItemPadding;
}

// static
int LocationBarView::GetEdgeItemPadding() {
  return (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) ?
      kTouchEdgeItemPadding : kDesktopEdgeItemPadding;
}

// DropdownBarHostDelegate
void LocationBarView::SetFocusAndSelection(bool select_all) {
  FocusLocation(select_all);
}

void LocationBarView::SetAnimationOffset(int offset) {
  animation_offset_ = offset;
}

void LocationBarView::Update(const WebContents* tab_for_state_restoring) {
  bool star_enabled = star_view_ && !model_->input_in_progress() &&
                      edit_bookmarks_enabled_.GetValue();
  command_updater_->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, star_enabled);
  if (star_view_)
    star_view_->SetVisible(star_enabled);

  bool enabled = chrome_to_mobile_view_ && !model_->input_in_progress() &&
      ChromeToMobileServiceFactory::GetForProfile(profile_)->HasDevices();
  command_updater_->UpdateCommandEnabled(IDC_CHROME_TO_MOBILE_PAGE, enabled);

  RefreshContentSettingViews();
  RefreshPageActionViews();
  // Don't Update in app launcher mode so that the location entry does not show
  // a URL or security background.
  if (mode_ != APP_LAUNCHER)
    location_entry_->Update(tab_for_state_restoring);
  OnChanged();
}

void LocationBarView::UpdateContentSettingsIcons() {
  RefreshContentSettingViews();

  Layout();
  SchedulePaint();
}

void LocationBarView::UpdatePageActions() {
  size_t count_before = page_action_views_.size();
  RefreshPageActionViews();
  if (page_action_views_.size() != count_before) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }

  Layout();
  SchedulePaint();
}

void LocationBarView::InvalidatePageActions() {
  size_t count_before = page_action_views_.size();
  DeletePageActionViews();
  if (page_action_views_.size() != count_before) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }
}

void LocationBarView::OnFocus() {
  // Focus the view widget first which implements accessibility for
  // Chrome OS.  It is noop on Win. This should be removed once
  // Chrome OS migrates to aura, which uses Views' textfield that receives
  // focus. See crbug.com/106428.
  GetWidget()->NotifyAccessibilityEvent(
      this, ui::AccessibilityTypes::EVENT_FOCUS, false);

  // Then focus the native location view which implements accessibility for
  // Windows.
  location_entry_->SetFocus();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (mode_ != NORMAL)
    return;

  DCHECK(page_action);
  WebContents* contents = GetWebContentsFromDelegate(delegate_);

  RefreshPageActionViews();
  PageActionWithBadgeView* page_action_view =
      static_cast<PageActionWithBadgeView*>(GetPageActionView(page_action));
  DCHECK(page_action_view);
  if (!page_action_view)
    return;

  page_action_view->image_view()->set_preview_enabled(preview_enabled);
  page_action_view->UpdateVisibility(contents, GURL(model_->GetText()));
  Layout();
  SchedulePaint();
}

views::View* LocationBarView::GetPageActionView(
    ExtensionAction *page_action) {
  DCHECK(page_action);
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->image_view()->page_action() == page_action)
      return *i;
  }
  return NULL;
}

void LocationBarView::SetStarToggled(bool on) {
  if (star_view_)
    star_view_->SetToggled(on);
}

void LocationBarView::ShowStarBubble(const GURL& url, bool newly_bookmarked) {
  browser::ShowBookmarkBubbleView(star_view_, profile_, url,
                                  newly_bookmarked);
}

void LocationBarView::ShowChromeToMobileBubble() {
  browser::ShowChromeToMobileBubbleView(chrome_to_mobile_view_, profile_);
}

gfx::Point LocationBarView::GetLocationEntryOrigin() const {
  gfx::Point origin(location_entry_view_->bounds().origin());
  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (base::i18n::IsRTL())
    origin.set_x(width() - origin.x());
  views::View::ConvertPointToScreen(this, &origin);
  return origin;
}

#if defined(OS_WIN) || defined(USE_AURA)
void LocationBarView::SetInstantSuggestion(const string16& text,
                                           bool animate_to_complete) {
  // Don't show the suggested text if inline autocomplete is prevented.
  if (!text.empty()) {
    if (!suggested_text_view_) {
      suggested_text_view_ = new SuggestedTextView(location_entry_->model());
      suggested_text_view_->SetText(text);
#if defined(USE_AURA)
      NOTIMPLEMENTED();
#else
      suggested_text_view_->SetFont(GetOmniboxViewWin()->GetFont());
#endif
      AddChildView(suggested_text_view_);
    } else if (suggested_text_view_->text() != text) {
      suggested_text_view_->SetText(text);
    }
    if (animate_to_complete && !location_entry_->IsImeComposing())
      suggested_text_view_->StartAnimation();
  } else if (suggested_text_view_) {
    delete suggested_text_view_;
    suggested_text_view_ = NULL;
  } else {
    return;
  }

  Layout();
  SchedulePaint();
}

string16 LocationBarView::GetInstantSuggestion() const {
  return HasValidSuggestText() ? suggested_text_view_->text() : string16();
}
#endif

void LocationBarView::SetLocationEntryFocusable(bool focusable) {
#if defined(USE_AURA)
    AsViews(location_entry_.get())->SetLocationEntryFocusable(focusable);
#else
    set_focusable(focusable);
#endif
}

bool LocationBarView::IsLocationEntryFocusableInRootView() const {
#if defined(USE_AURA)
  return AsViews(location_entry_.get())->IsLocationEntryFocusableInRootView();
#else
  return views::View::IsFocusable();
#endif
}

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0, GetThemeProvider()->GetBitmapNamed(mode_ == POPUP ?
      IDR_LOCATIONBG_POPUPMODE_CENTER : IDR_LOCATIONBG_C)->height());
}

void LocationBarView::Layout() {
  if (!location_entry_.get())
    return;

  // TODO(jhawkins): Remove once crbug.com/101994 is fixed.
  CHECK(location_icon_view_);

  // TODO(sky): baseline layout.
  int location_y = kVerticalEdgeThickness;
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  int location_height = std::max(height() - (kVerticalEdgeThickness * 2), 0);

  // The edge stroke is 1 px thick.  In popup mode, the edges are drawn by the
  // omnibox' parent, so there isn't any edge to account for at all.
  const int kEdgeThickness = (mode_ == NORMAL) ?
      kNormalHorizontalEdgeThickness : 0;
  // The edit has 1 px of horizontal whitespace inside it before the text.
  const int kEditInternalSpace = 1;
  // The space between an item and the edit is the normal item space, minus the
  // edit's built-in space (so the apparent space will be the same).
  const int kItemEditPadding = GetItemPadding() - kEditInternalSpace;
  const int kEdgeEditPadding = GetEdgeItemPadding() - kEditInternalSpace;
  const int kBubbleVerticalPadding = (mode_ == POPUP) ?
      -1 : kBubbleHorizontalPadding;

  // Start by reserving the padding at the right edge.
  int entry_width = width() - kEdgeThickness - GetEdgeItemPadding();

  // |location_icon_view_| is visible except when |ev_bubble_view_| or
  // |selected_keyword_view_| are visible.
  int location_icon_width = 0;
  int ev_bubble_width = 0;
  location_icon_view_->SetVisible(false);
  ev_bubble_view_->SetVisible(false);
  const string16 keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  if (show_selected_keyword) {
    // Assume the keyword might be hidden.
    entry_width -= (kEdgeThickness + kEdgeEditPadding);
  } else if (model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    ev_bubble_view_->SetVisible(true);
    ev_bubble_view_->SetLabel(model_->GetEVCertName());
    ev_bubble_width = ev_bubble_view_->GetPreferredSize().width();
    // We'll adjust this width and take it out of |entry_width| below.
  } else {
    location_icon_view_->SetVisible(true);
    location_icon_width = location_icon_view_->GetPreferredSize().width();
    entry_width -= (kEdgeThickness + GetEdgeItemPadding() +
                    location_icon_width + kItemEditPadding);
  }

  if (star_view_ && star_view_->visible())
    entry_width -= star_view_->GetPreferredSize().width() + GetItemPadding();
  if (chrome_to_mobile_view_ && chrome_to_mobile_view_->visible())
    entry_width -= chrome_to_mobile_view_->GetPreferredSize().width() +
        GetItemPadding();
  if (action_box_button_view_)
    entry_width -= action_box_button_view_->GetPreferredSize().width() +
        GetItemPadding();
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->visible())
      entry_width -= ((*i)->GetPreferredSize().width() + GetItemPadding());
  }
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    if ((*i)->visible())
      entry_width -= ((*i)->GetPreferredSize().width() + GetItemPadding());
  }
  // The gap between the edit and whatever is to its right is shortened.
  entry_width += kEditInternalSpace;

  // Size the EV bubble after taking star/ChromeToMobile/page actions/content
  // settings out of |entry_width| so we won't take too much space.
  if (ev_bubble_width) {
    // Try to elide the bubble to be no larger than half the total available
    // space, but never elide it any smaller than 150 px.
    static const int kMinElidedBubbleWidth = 150;
    static const double kMaxBubbleFraction = 0.5;
    const int total_padding =
        kEdgeThickness + kBubbleHorizontalPadding + kItemEditPadding;
    ev_bubble_width = std::min(ev_bubble_width, std::max(kMinElidedBubbleWidth,
        static_cast<int>((entry_width - total_padding) * kMaxBubbleFraction)));
    entry_width -= (total_padding + ev_bubble_width);
  }

  int max_edit_width = location_entry_->GetMaxEditWidth(entry_width);
  if (max_edit_width < 0)
    return;
  const int available_width = AvailableWidth(max_edit_width);

  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;
  selected_keyword_view_->SetVisible(show_selected_keyword);
  keyword_hint_view_->SetVisible(show_keyword_hint);
  if (show_selected_keyword) {
    if (selected_keyword_view_->keyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile_)->
          GetTemplateURLForKeyword(keyword);
      if (template_url && template_url->IsExtensionKeyword()) {
        const SkBitmap& bitmap =
            profile_->GetExtensionService()->GetOmniboxIcon(
                template_url->GetExtensionId());
        selected_keyword_view_->SetImage(bitmap);
        selected_keyword_view_->set_is_extension_icon(true);
      } else {
        ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
        selected_keyword_view_->SetImage(
            *rb.GetBitmapNamed(IDR_OMNIBOX_SEARCH));
        selected_keyword_view_->set_is_extension_icon(false);
      }
    }
  } else if (show_keyword_hint) {
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // Lay out items to the right of the edit field.
  int offset = width() - kEdgeThickness - GetEdgeItemPadding();
  if (star_view_ && star_view_->visible()) {
    int star_width = star_view_->GetPreferredSize().width();
    offset -= star_width;
    star_view_->SetBounds(offset, location_y, star_width, location_height);
    offset -= GetItemPadding();
  }

  if (chrome_to_mobile_view_ && chrome_to_mobile_view_->visible()) {
    int icon_width = chrome_to_mobile_view_->GetPreferredSize().width();
    offset -= icon_width;
    chrome_to_mobile_view_->SetBounds(offset, location_y,
                                      icon_width, location_height);
    offset -= GetItemPadding();
  }

  if (action_box_button_view_) {
    int button_width = action_box_button_view_->GetPreferredSize().width();
    offset -= button_width;
    action_box_button_view_->SetBounds(offset, location_y, button_width,
                                       location_height);
    offset -= GetItemPadding();
  }

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->visible()) {
      int page_action_width = (*i)->GetPreferredSize().width();
      offset -= page_action_width;
      (*i)->SetBounds(offset, location_y, page_action_width, location_height);
      offset -= GetItemPadding();
    }
  }
  // We use a reverse_iterator here because we're laying out the views from
  // right to left but in the vector they're ordered left to right.
  for (ContentSettingViews::const_reverse_iterator
       i(content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->visible()) {
      int content_blocked_width = (*i)->GetPreferredSize().width();
      offset -= content_blocked_width;
      (*i)->SetBounds(offset, location_y, content_blocked_width,
                      location_height);
      offset -= GetItemPadding();
    }
  }

  // Now lay out items to the left of the edit field.
  if (location_icon_view_->visible()) {
    location_icon_view_->SetBounds(kEdgeThickness + GetEdgeItemPadding(),
        location_y, location_icon_width, location_height);
    offset = location_icon_view_->bounds().right() + kItemEditPadding;
  } else if (ev_bubble_view_->visible()) {
    ev_bubble_view_->SetBounds(kEdgeThickness + kBubbleHorizontalPadding,
        location_y + kBubbleVerticalPadding, ev_bubble_width,
        ev_bubble_view_->GetPreferredSize().height());
    offset = ev_bubble_view_->bounds().right() + kItemEditPadding;
  } else {
    offset = kEdgeThickness +
        (show_selected_keyword ? kBubbleHorizontalPadding : kEdgeEditPadding);
  }

  // Now lay out the edit field and views that autocollapse to give it more
  // room.
  gfx::Rect location_bounds(offset, location_y, entry_width, location_height);
  if (show_selected_keyword) {
    selected_keyword_view_->SetBounds(0, location_y + kBubbleVerticalPadding,
        0, selected_keyword_view_->GetPreferredSize().height());
    LayoutView(selected_keyword_view_, kItemEditPadding, available_width,
               true, &location_bounds);
    location_bounds.set_x(selected_keyword_view_->visible() ?
        (offset + selected_keyword_view_->width() + kItemEditPadding) :
        (kEdgeThickness + kEdgeEditPadding));
  } else if (show_keyword_hint) {
    keyword_hint_view_->SetBounds(0, location_y, 0, location_height);
    // Tricky: |entry_width| has already been enlarged by |kEditInternalSpace|.
    // But if we add a trailing view, it needs to have that enlargement be to
    // its left.  So we undo the enlargement, then include it in the padding for
    // the added view.
    location_bounds.Inset(0, 0, kEditInternalSpace, 0);
    LayoutView(keyword_hint_view_, kItemEditPadding, available_width, false,
               &location_bounds);
    if (!keyword_hint_view_->visible()) {
      // Put back the enlargement that we undid above.
      location_bounds.Inset(0, 0, -kEditInternalSpace, 0);
    }
  }

#if defined(OS_WIN)
  // Layout out the suggested text view right aligned to the location
  // entry. Only show the suggested text if we can fit the text from one
  // character before the end of the selection to the end of the text and the
  // suggested text. If we can't it means either the suggested text is too big,
  // or the user has scrolled.

  // TODO(sky): We could potentially combine this with the previous step to
  // force using minimum size if necessary, but currently the chance of showing
  // keyword hints and suggested text is minimal and we're not confident this
  // is the right approach for suggested text.
  if (suggested_text_view_) {
#if defined(USE_AURA)
    NOTIMPLEMENTED();
#else
    // TODO(sky): need to layout when the user changes caret position.
    int suggested_text_width =
        suggested_text_view_->GetPreferredSize().width();
    int vis_text_width = GetOmniboxViewWin()->WidthOfTextAfterCursor();
    if (vis_text_width + suggested_text_width > entry_width) {
      // Hide the suggested text if the user has scrolled or we can't fit all
      // the suggested text.
      suggested_text_view_->SetBounds(0, 0, 0, 0);
    } else {
      int location_needed_width = location_entry_->TextWidth();
      location_bounds.set_width(std::min(location_needed_width,
                                         entry_width - suggested_text_width));
      // TODO(sky): figure out why this needs the -1.
      suggested_text_view_->SetBounds(location_bounds.right() - 1,
                                      location_bounds.y(),
                                      suggested_text_width,
                                      location_bounds.height());
    }
#endif
  }
#endif

  location_entry_view_->SetBoundsRect(location_bounds);
}

void LocationBarView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (painter_.get()) {
    painter_->Paint(canvas, size());
  } else if (mode_ == POPUP) {
    canvas->TileImageInt(*GetThemeProvider()->GetBitmapNamed(
        IDR_LOCATIONBG_POPUPMODE_CENTER), 0, 0, 0, 0, width(), height());
  }
  // When used in the app launcher, don't draw a border, the LocationBarView has
  // its own views::Border.

  // Draw the background color so that the graphical elements at the edges
  // appear over the correct color.  (The edit draws its own background, so this
  // isn't important for that.)
  // TODO(pkasting): We need images that are transparent in the middle, so we
  // can draw the border images over the background color instead of the
  // reverse; this antialiases better (see comments in
  // AutocompletePopupContentsView::OnPaint()).
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(0, kVerticalEdgeThickness);
  SkColor color(GetColor(ToolbarModel::NONE, BACKGROUND));
  if (mode_ == NORMAL) {
    SkPaint paint;
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    // TODO(jamescook): Make the corners of the dropdown match the corners of
    // the omnibox.
    const SkScalar radius(SkIntToScalar(kBorderCornerRadius));
    bounds.Inset(kNormalHorizontalEdgeThickness, 0);
    canvas->sk_canvas()->drawRoundRect(gfx::RectToSkRect(bounds), radius,
                                       radius, paint);
  } else {
    canvas->FillRect(bounds, color);
  }

  if (show_focus_rect_ && HasFocus()) {
    gfx::Rect r = location_entry_view_->bounds();
    // TODO(jamescook): Is this still needed?
#if defined(OS_WIN)
    r.Inset(-1,  -1);
#else
    r.Inset(-1, 0);
#endif
    canvas->DrawFocusRect(r);
  }
}

void LocationBarView::SetShowFocusRect(bool show) {
  show_focus_rect_ = show;
  SchedulePaint();
}

void LocationBarView::SelectAll() {
  location_entry_->SelectAll(true);
}

#if defined(OS_WIN) && !defined(USE_AURA)
bool LocationBarView::OnMousePressed(const views::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = (event.flags() & ui::EF_IS_DOUBLE_CLICK) ?
        WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
  } else if (event.IsMiddleMouseButton()) {
    msg = (event.flags() & ui::EF_IS_DOUBLE_CLICK) ?
        WM_MBUTTONDBLCLK : WM_MBUTTONDOWN;
  } else if (event.IsRightMouseButton()) {
    msg = (event.flags() & ui::EF_IS_DOUBLE_CLICK) ?
        WM_RBUTTONDBLCLK : WM_RBUTTONDOWN;
  } else {
    NOTREACHED();
    return false;
  }
  OnMouseEvent(event, msg);
  return true;
}

bool LocationBarView::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseEvent(event, WM_MOUSEMOVE);
  return true;
}

void LocationBarView::OnMouseReleased(const views::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = WM_LBUTTONUP;
  } else if (event.IsMiddleMouseButton()) {
    msg = WM_MBUTTONUP;
  } else if (event.IsRightMouseButton()) {
    msg = WM_RBUTTONUP;
  } else {
    NOTREACHED();
    return;
  }
  OnMouseEvent(event, msg);
}

void LocationBarView::OnMouseCaptureLost() {
  GetOmniboxViewWin()->HandleExternalMsg(WM_CAPTURECHANGED, 0, CPoint());
}
#endif

void LocationBarView::OnAutocompleteAccept(
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

void LocationBarView::OnChanged() {
  location_icon_view_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
          location_entry_->GetIcon()));
  location_icon_view_->ShowTooltip(!GetLocationEntry()->IsEditingOrEmpty());

  Layout();
  SchedulePaint();
}

void LocationBarView::OnSelectionBoundsChanged() {
#if defined(OS_WIN)
  if (suggested_text_view_)
    suggested_text_view_->StopAnimation();
#else
  NOTREACHED();
#endif
}

void LocationBarView::OnInputInProgress(bool in_progress) {
  delegate_->OnInputInProgress(in_progress);
}

void LocationBarView::OnKillFocus() {
}

void LocationBarView::OnSetFocus() {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
}

SkBitmap LocationBarView::GetFavicon() const {
  return delegate_->GetTabContentsWrapper()->favicon_tab_helper()->GetFavicon();
}

string16 LocationBarView::GetTitle() const {
  return GetWebContentsFromDelegate(delegate_)->GetTitle();
}

InstantController* LocationBarView::GetInstant() {
  return delegate_->GetInstant();
}

TabContentsWrapper* LocationBarView::GetTabContentsWrapper() const {
  return delegate_->GetTabContentsWrapper();
}

int LocationBarView::AvailableWidth(int location_bar_width) {
  return location_bar_width - location_entry_->TextWidth();
}

void LocationBarView::LayoutView(views::View* view,
                                 int padding,
                                 int available_width,
                                 bool leading,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  gfx::Size view_size = view->GetPreferredSize();
  if ((view_size.width() + padding) > available_width)
    view_size = view->GetMinimumSize();
  int desired_width = view_size.width() + padding;
  view->SetVisible(desired_width < bounds->width());
  if (view->visible()) {
    view->SetBounds(
        leading ? bounds->x() : (bounds->right() - view_size.width()),
        view->y(), view_size.width(), view->height());
    bounds->set_width(bounds->width() - desired_width);
  }
}

void LocationBarView::RefreshContentSettingViews() {
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->UpdateFromWebContents(model_->input_in_progress() ? NULL :
                                GetWebContentsFromDelegate(delegate_));
  }
}

void LocationBarView::DeletePageActionViews() {
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    RemoveChildView(*i);
  STLDeleteElements(&page_action_views_);
}

void LocationBarView::RefreshPageActionViews() {
  if (mode_ != NORMAL)
    return;

  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;

  std::map<ExtensionAction*, bool> old_visibility;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    old_visibility[(*i)->image_view()->page_action()] = (*i)->visible();

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  std::vector<ExtensionAction*> page_actions;
  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    if ((*it)->page_action())
      page_actions.push_back((*it)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions.size() != page_action_views_.size()) {
    DeletePageActionViews();  // Delete the old views (if any).

    page_action_views_.resize(page_actions.size());
    View* view = chrome_to_mobile_view_ ? chrome_to_mobile_view_ :
                                          static_cast<View*>(star_view_);

    // Add the page actions in reverse order, so that the child views are
    // inserted in left-to-right order for accessibility.
    for (int i = page_actions.size() - 1; i >= 0; --i) {
      page_action_views_[i] = new PageActionWithBadgeView(
          delegate_->CreatePageActionImageView(this, page_actions[i]));
      page_action_views_[i]->SetVisible(false);
      AddChildViewAt(page_action_views_[i], GetIndexOf(view));
    }
  }

  WebContents* contents = GetWebContentsFromDelegate(delegate_);
  if (!page_action_views_.empty() && contents) {
    Browser* browser =
        browser::FindBrowserForController(&contents->GetController(), NULL);
    GURL url = browser->GetSelectedWebContents()->GetURL();

    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(model_->input_in_progress() ? NULL : contents,
                             url);

      // Check if the visibility of the action changed and notify if it did.
      ExtensionAction* action = (*i)->image_view()->page_action();
      if (old_visibility.find(action) == old_visibility.end() ||
          old_visibility[action] != (*i)->visible()) {
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
            content::Source<ExtensionAction>(action),
            content::Details<WebContents>(contents));
      }
    }
  }
}

#if defined(OS_WIN) && !defined(USE_AURA)
void LocationBarView::OnMouseEvent(const views::MouseEvent& event, UINT msg) {
  UINT flags = event.native_event().wParam;
  gfx::Point screen_point(event.location());
  ConvertPointToScreen(this, &screen_point);
  GetOmniboxViewWin()->HandleExternalMsg(msg, flags, screen_point.ToPOINT());
}
#endif

void LocationBarView::ShowFirstRunBubbleInternal() {
#if !defined(OS_CHROMEOS)
  // First run bubble doesn't make sense for Chrome OS.
  FirstRunBubble::ShowBubble(profile_, location_icon_view_);
#endif
}

std::string LocationBarView::GetClassName() const {
  return kViewClassName;
}

bool LocationBarView::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& event) {
#if defined(OS_WIN)
  if (views::FocusManager::IsTabTraversalKeyEvent(event)) {
    if (location_entry_->model()->popup_model()->IsOpen()) {
      // Return true so that the edit sees the tab and moves the selection.
      return true;
    }
    if (keyword_hint_view_->visible() && !event.IsShiftDown()) {
      // Return true so the edit gets the tab event and enters keyword mode.
      return true;
    }

    // Tab while showing instant commits instant immediately.
    // Return true so that focus traversal isn't attempted. The edit ends
    // up doing nothing in this case.
    if (location_entry_->model()->AcceptCurrentInstantPreview())
      return true;
  }

#if defined(USE_AURA)
  NOTIMPLEMENTED();
  return false;
#else
  return GetOmniboxViewWin()->SkipDefaultKeyEventProcessing(event);
#endif

#else
  // This method is not used for Linux ports. See FocusManager::OnKeyEvent() in
  // src/ui/views/focus/focus_manager.cc for details.
  return false;
#endif
}

void LocationBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_LOCATION_BAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
  state->value = location_entry_->GetText();

  string16::size_type entry_start;
  string16::size_type entry_end;
  location_entry_->GetSelectionBounds(&entry_start, &entry_end);
  state->selection_start = entry_start;
  state->selection_end = entry_end;
}

void LocationBarView::WriteDragDataForView(views::View* sender,
                                           const gfx::Point& press_pt,
                                           OSExchangeData* data) {
  DCHECK_NE(GetDragOperationsForView(sender, press_pt),
            ui::DragDropTypes::DRAG_NONE);

  TabContentsWrapper* tab_contents = delegate_->GetTabContentsWrapper();
  DCHECK(tab_contents);
  button_drag_utils::SetURLAndDragImage(
      tab_contents->web_contents()->GetURL(),
      tab_contents->web_contents()->GetTitle(),
      tab_contents->favicon_tab_helper()->GetFavicon(),
      data);
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK((sender == location_icon_view_) || (sender == ev_bubble_view_));
  WebContents* web_contents = GetWebContentsFromDelegate(delegate_);
  return (web_contents && web_contents->GetURL().is_valid() &&
          !GetLocationEntry()->IsEditingOrEmpty()) ?
      (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK) :
      ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDragForView(View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble() {
  // Wait until search engines have loaded to show the first run bubble.
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!url_service->loaded()) {
    template_url_service_ = url_service;
    template_url_service_->AddObserver(this);
    template_url_service_->Load();
    return;
  }
  ShowFirstRunBubbleInternal();
}

void LocationBarView::SetSuggestedText(const string16& text,
                                       InstantCompleteBehavior behavior) {
  location_entry_->model()->SetSuggestedText(text, behavior);
}

string16 LocationBarView::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition_;
}

content::PageTransition LocationBarView::GetPageTransition() const {
  return transition_;
}

void LocationBarView::AcceptInput() {
  location_entry_->model()->AcceptInput(CURRENT_TAB, false);
}

void LocationBarView::FocusLocation(bool select_all) {
  location_entry_->SetFocus();
  if (select_all)
    location_entry_->SelectAll(true);
}

void LocationBarView::FocusSearch() {
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarView::SaveStateToContents(WebContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarView::Revert() {
  location_entry_->RevertAll();
}

const OmniboxView* LocationBarView::GetLocationEntry() const {
  return location_entry_.get();
}

OmniboxView* LocationBarView::GetLocationEntry() {
  return location_entry_.get();
}

LocationBarTesting* LocationBarView::GetLocationBarForTesting() {
  return this;
}

int LocationBarView::PageActionCount() {
  return page_action_views_.size();
}

int LocationBarView::PageActionVisibleCount() {
  int result = 0;
  for (size_t i = 0; i < page_action_views_.size(); i++) {
    if (page_action_views_[i]->visible())
      ++result;
  }
  return result;
}

ExtensionAction* LocationBarView::GetPageAction(size_t index) {
  if (index < page_action_views_.size())
    return page_action_views_[index]->image_view()->page_action();

  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarView::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->visible()) {
      if (current == index)
        return page_action_views_[i]->image_view()->page_action();

      ++current;
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarView::TestPageActionPressed(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->visible()) {
      if (current == index) {
        const int kLeftMouseButton = 1;
        page_action_views_[i]->image_view()->ExecuteAction(kLeftMouseButton);
        return;
      }
      ++current;
    }
  }

  NOTREACHED();
}

void LocationBarView::OnTemplateURLServiceChanged() {
  template_url_service_->RemoveObserver(this);
  template_url_service_ = NULL;
  // If the browser is no longer active, let's not show the info bubble, as this
  // would make the browser the active window again.
  if (location_entry_view_ && location_entry_view_->GetWidget()->IsActive())
    ShowFirstRunBubble();
}

void LocationBarView::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* name = content::Details<std::string>(details).ptr();
    if (*name == prefs::kEditBookmarksEnabled)
      Update(NULL);
  }
}

int LocationBarView::GetHeight() {
  return std::max(
      GetPreferredSize().height() - (kVerticalEdgeThickness * 2), 0);
}

#if defined(OS_WIN) || defined(USE_AURA)
bool LocationBarView::HasValidSuggestText() const {
  return suggested_text_view_ && !suggested_text_view_->size().IsEmpty() &&
      !suggested_text_view_->text().empty();
}

#if !defined(USE_AURA)
OmniboxViewWin* LocationBarView::GetOmniboxViewWin() {
  return static_cast<OmniboxViewWin*>(location_entry_.get());
}
#endif
#endif
