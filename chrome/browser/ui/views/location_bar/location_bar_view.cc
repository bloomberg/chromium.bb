// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_prompt_view.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/generated_credit_card_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/location_bar/open_pdf_in_reader_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/location_bar/script_bubble_icon_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/translate_icon_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_views.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "ui/native_theme/native_theme_win.h"
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/first_run_bubble.h"
#endif

#if defined(USE_AURA)
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#endif

using content::WebContents;
using views::View;


namespace {

Browser* GetBrowserFromDelegate(LocationBarView::Delegate* delegate) {
  WebContents* contents = delegate->GetWebContents();
  return contents ? chrome::FindBrowserWithWebContents(contents) : NULL;
}

// Given a containing |height| and a |base_font_list|, shrinks the font size
// until the font list will fit within |height| while having its cap height
// vertically centered.  Returns the correctly-sized font list.
//
// The expected layout:
//   +--------+-----------------------------------------------+------------+
//   |        | y offset                                      | space      |
//   |        +--------+-------------------+------------------+ above      |
//   |        |        |                   | internal leading | cap height |
//   | box    | font   | ascent (baseline) +------------------+------------+
//   | height | height |                   | cap height                    |
//   |        |        |-------------------+------------------+------------+
//   |        |        | descent (height - baseline)          | space      |
//   |        +--------+--------------------------------------+ below      |
//   |        | space at bottom                               | cap height |
//   +--------+-----------------------------------------------+------------+
// Goal:
//     center of box height == center of cap height
//     (i.e. space above cap height == space below cap height)
// Restrictions:
//     y offset >= 0
//     space at bottom >= 0
//     (i.e. Entire font must be visible inside the box.)
gfx::FontList GetLargestFontListWithHeightBound(
    const gfx::FontList& base_font_list,
    int height) {
  gfx::FontList font_list = base_font_list;
  for (int font_size = font_list.GetFontSize(); font_size > 1; --font_size) {
    const int internal_leading =
        font_list.GetBaseline() - font_list.GetCapHeight();
    // Some platforms don't support getting the cap height, and simply return
    // the entire font ascent from GetCapHeight().  Centering the ascent makes
    // the font look too low, so if GetCapHeight() returns the ascent, center
    // the entire font height instead.
    const int space =
        height - ((internal_leading != 0) ?
                  font_list.GetCapHeight() : font_list.GetHeight());
    const int y_offset = space / 2 - internal_leading;
    const int space_at_bottom = height - (y_offset + font_list.GetHeight());
    if ((y_offset >= 0) && (space_at_bottom >= 0))
      break;
    font_list = font_list.DeriveFontListWithSizeDelta(-1);
  }
  return font_list;
}

}  // namespace


// LocationBarView -----------------------------------------------------------

// static
const int LocationBarView::kNormalEdgeThickness = 2;
const int LocationBarView::kPopupEdgeThickness = 1;
const int LocationBarView::kIconInternalPadding = 2;
const int LocationBarView::kBubblePadding = 1;
const char LocationBarView::kViewClassName[] = "LocationBarView";

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : OmniboxEditController(command_updater),
      browser_(browser),
      profile_(profile),
      delegate_(delegate),
      location_icon_view_(NULL),
      ev_bubble_view_(NULL),
      location_entry_view_(NULL),
      ime_inline_autocomplete_view_(NULL),
      selected_keyword_view_(NULL),
      suggested_text_view_(NULL),
      keyword_hint_view_(NULL),
      mic_search_view_(NULL),
      zoom_view_(NULL),
      generated_credit_card_view_(NULL),
      open_pdf_in_reader_view_(NULL),
      manage_passwords_icon_view_(NULL),
      script_bubble_icon_view_(NULL),
      star_view_(NULL),
      translate_icon_view_(NULL),
      is_popup_mode_(is_popup_mode),
      show_focus_rect_(false),
      template_url_service_(NULL),
      animation_offset_(0),
      weak_ptr_factory_(this) {
  if (!views::Textfield::IsViewsTextfieldEnabled())
    set_id(VIEW_ID_OMNIBOX);

  const int kOmniboxBorderImages[] = IMAGE_GRID(IDR_OMNIBOX_BORDER);
  const int kOmniboxPopupImages[] = IMAGE_GRID(IDR_OMNIBOX_POPUP_BORDER);
  background_border_painter_.reset(
      views::Painter::CreateImageGridPainter(
          is_popup_mode_ ? kOmniboxPopupImages : kOmniboxBorderImages));
#if defined(OS_CHROMEOS)
  if (!is_popup_mode_) {
    const int kOmniboxFillingImages[] = IMAGE_GRID(IDR_OMNIBOX_FILLING);
    background_filling_painter_.reset(
        views::Painter::CreateImageGridPainter(kOmniboxFillingImages));
  }
#endif

  edit_bookmarks_enabled_.Init(
      prefs::kEditBookmarksEnabled,
      profile_->GetPrefs(),
      base::Bind(&LocationBarView::Update,
                 base::Unretained(this),
                 static_cast<content::WebContents*>(NULL)));

  if (browser_)
    browser_->search_model()->AddObserver(this);
}

LocationBarView::~LocationBarView() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
  if (browser_)
    browser_->search_model()->RemoveObserver(this);
}

// static
void LocationBarView::InitTouchableLocationBarChildView(views::View* view) {
  int horizontal_padding = GetBuiltInHorizontalPaddingForChildViews();
  if (horizontal_padding != 0) {
    view->set_border(views::Border::CreateEmptyBorder(
        3, horizontal_padding, 3, horizontal_padding));
  }
}

void LocationBarView::Init() {
  // We need to be in a Widget, otherwise GetNativeTheme() may change and we're
  // not prepared for that.
  DCHECK(GetWidget());

  location_icon_view_ = new LocationIconView(this);
  location_icon_view_->set_drag_controller(this);
  AddChildView(location_icon_view_);

  // Determine the main font.
  gfx::FontList font_list = ResourceBundle::GetSharedInstance().GetFontList(
      ResourceBundle::BaseFont);
  const int current_font_size = font_list.GetFontSize();
  const int desired_font_size = browser_defaults::kOmniboxFontPixelSize;
  if (current_font_size < desired_font_size)
    font_list = font_list.DeriveFontListWithSize(desired_font_size);
  // Shrink large fonts to make them fit.
  // TODO(pkasting): Stretch the location bar instead in this case.
  const int location_height = GetInternalHeight(true);
  font_list = GetLargestFontListWithHeightBound(font_list, location_height);

  // Determine the font for use inside the bubbles.  The bubble background
  // images have 1 px thick edges, which we don't want to overlap.
  const int kBubbleInteriorVerticalPadding = 1;
  const int bubble_vertical_padding =
      (kBubblePadding + kBubbleInteriorVerticalPadding) * 2;
  const gfx::FontList bubble_font_list(
      GetLargestFontListWithHeightBound(
          font_list, location_height - bubble_vertical_padding));

  const SkColor background_color =
      GetColor(ToolbarModel::NONE, LocationBarView::BACKGROUND);
  ev_bubble_view_ = new EVBubbleView(
      bubble_font_list, GetColor(ToolbarModel::EV_SECURE, SECURITY_TEXT),
      background_color, this);
  ev_bubble_view_->set_drag_controller(this);
  AddChildView(ev_bubble_view_);

  // Initialize the Omnibox view.
  location_entry_.reset(CreateOmniboxView(this, profile_, command_updater(),
                                          is_popup_mode_, this, font_list));
  SetLocationEntryFocusable(true);
  location_entry_view_ = location_entry_->AddToView(this);

  // Initialize the inline autocomplete view which is visible only when IME is
  // turned on.  Use the same font with the omnibox and highlighted background.
  ime_inline_autocomplete_view_ = new views::Label(string16(), font_list);
  ime_inline_autocomplete_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ime_inline_autocomplete_view_->SetAutoColorReadabilityEnabled(false);
  ime_inline_autocomplete_view_->set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused)));
  ime_inline_autocomplete_view_->SetEnabledColor(
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionColor));
  ime_inline_autocomplete_view_->SetVisible(false);
  AddChildView(ime_inline_autocomplete_view_);

  const SkColor text_color = GetColor(ToolbarModel::NONE, TEXT);
  selected_keyword_view_ = new SelectedKeywordView(
      bubble_font_list, text_color, background_color, profile_);
  AddChildView(selected_keyword_view_);

  suggested_text_view_ = new views::Label(string16(), font_list);
  suggested_text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  suggested_text_view_->SetAutoColorReadabilityEnabled(false);
  suggested_text_view_->SetEnabledColor(GetColor(
      ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT));
  suggested_text_view_->SetVisible(false);
  AddChildView(suggested_text_view_);

  keyword_hint_view_ = new KeywordHintView(
      profile_, font_list,
      GetColor(ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT),
      background_color);
  AddChildView(keyword_hint_view_);

  mic_search_view_ = new views::ImageButton(this);
  mic_search_view_->set_id(VIEW_ID_MIC_SEARCH_BUTTON);
  mic_search_view_->set_accessibility_focusable(true);
  mic_search_view_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_MIC_SEARCH));
  mic_search_view_->SetImage(
      views::Button::STATE_NORMAL,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_OMNIBOX_MIC_SEARCH));
  mic_search_view_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                      views::ImageButton::ALIGN_MIDDLE);
  mic_search_view_->SetVisible(false);
  InitTouchableLocationBarChildView(mic_search_view_);
  AddChildView(mic_search_view_);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_blocked_view =
        new ContentSettingImageView(static_cast<ContentSettingsType>(i), this,
                                    bubble_font_list, text_color,
                                    background_color);
    content_setting_views_.push_back(content_blocked_view);
    content_blocked_view->SetVisible(false);
    AddChildView(content_blocked_view);
  }

  generated_credit_card_view_ = new GeneratedCreditCardView(delegate_);
  AddChildView(generated_credit_card_view_);

  zoom_view_ = new ZoomView(delegate_);
  zoom_view_->set_id(VIEW_ID_ZOOM_BUTTON);
  AddChildView(zoom_view_);

  open_pdf_in_reader_view_ = new OpenPDFInReaderView(this);
  AddChildView(open_pdf_in_reader_view_);

  manage_passwords_icon_view_ = new ManagePasswordsIconView(delegate_);
  manage_passwords_icon_view_->set_id(VIEW_ID_MANAGE_PASSWORDS_ICON_BUTTON);
  AddChildView(manage_passwords_icon_view_);

  script_bubble_icon_view_ = new ScriptBubbleIconView(delegate());
  script_bubble_icon_view_->SetVisible(false);
  AddChildView(script_bubble_icon_view_);

  star_view_ = new StarView(command_updater());
  star_view_->SetVisible(false);
  AddChildView(star_view_);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTranslateNewUX)) {
    translate_icon_view_ = new TranslateIconView(command_updater());
    translate_icon_view_->SetVisible(false);
    AddChildView(translate_icon_view_);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
                 content::Source<Profile>(profile_));

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);
}

bool LocationBarView::IsInitialized() const {
  return location_entry_view_ != NULL;
}

SkColor LocationBarView::GetColor(ToolbarModel::SecurityLevel security_level,
                                  ColorKind kind) const {
  const ui::NativeTheme* native_theme = GetNativeTheme();
  switch (kind) {
    case BACKGROUND:
#if defined(OS_CHROMEOS)
      // Chrome OS requires a transparent omnibox background color.
      return SkColorSetARGB(0, 255, 255, 255);
#else
      return native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);
#endif

    case TEXT:
      return native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultColor);

    case SELECTED_TEXT:
      return native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionColor);

    case DEEMPHASIZED_TEXT:
      return color_utils::AlphaBlend(
          GetColor(security_level, TEXT),
          GetColor(security_level, BACKGROUND),
          128);

    case SECURITY_TEXT: {
      SkColor color;
      switch (security_level) {
        case ToolbarModel::EV_SECURE:
        case ToolbarModel::SECURE:
          color = SkColorSetRGB(7, 149, 0);
          break;

        case ToolbarModel::SECURITY_WARNING:
        case ToolbarModel::SECURITY_POLICY_WARNING:
          return GetColor(security_level, DEEMPHASIZED_TEXT);
          break;

        case ToolbarModel::SECURITY_ERROR:
          color = SkColorSetRGB(162, 0, 0);
          break;

        default:
          NOTREACHED();
          return GetColor(security_level, TEXT);
      }
      return color_utils::GetReadableColor(
          color, GetColor(security_level, BACKGROUND));
    }

    default:
      NOTREACHED();
      return GetColor(security_level, TEXT);
  }
}

void LocationBarView::GetOmniboxPopupPositioningInfo(
    gfx::Point* top_left_screen_coord,
    int* popup_width,
    int* left_margin,
    int* right_margin) {
  // Because the popup might appear atop the attached bookmark bar, there won't
  // necessarily be a client edge separating it from the rest of the toolbar.
  // Therefore we position the popup high enough so it can draw its own client
  // edge at the top, in the same place the toolbar would normally draw the
  // client edge.
  *top_left_screen_coord = gfx::Point(
      0,
      parent()->height() - views::NonClientFrameView::kClientEdgeThickness);
  views::View::ConvertPointToScreen(parent(), top_left_screen_coord);
  *popup_width = parent()->width();

  gfx::Rect location_bar_bounds(bounds());
  location_bar_bounds.Inset(kNormalEdgeThickness, 0);
  *left_margin = location_bar_bounds.x();
  *right_margin = *popup_width - location_bar_bounds.right();
}

// static
int LocationBarView::GetItemPadding() {
  const int kTouchItemPadding = 8;
  if (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH)
    return kTouchItemPadding;

  const int kDesktopScriptBadgeItemPadding = 9;
  const int kDesktopItemPadding = 3;
  return extensions::FeatureSwitch::script_badges()->IsEnabled() ?
      kDesktopScriptBadgeItemPadding : kDesktopItemPadding;
}

// DropdownBarHostDelegate
void LocationBarView::SetFocusAndSelection(bool select_all) {
  FocusLocation(select_all);
}

void LocationBarView::SetAnimationOffset(int offset) {
  animation_offset_ = offset;
}

void LocationBarView::UpdateContentSettingsIcons() {
  RefreshContentSettingViews();
  Layout();
  SchedulePaint();
}

void LocationBarView::UpdateManagePasswordsIconAndBubble() {
  RefreshManagePasswordsIconView();
  Layout();
  SchedulePaint();
  ShowManagePasswordsBubbleIfNeeded();
}

void LocationBarView::UpdatePageActions() {
  size_t count_before = page_action_views_.size();
  RefreshPageActionViews();
  RefreshScriptBubble();
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

void LocationBarView::UpdateOpenPDFInReaderPrompt() {
  open_pdf_in_reader_view_->Update(
      GetToolbarModel()->input_in_progress() ? NULL : GetWebContents());
  Layout();
  SchedulePaint();
}

void LocationBarView::UpdateGeneratedCreditCardView() {
  generated_credit_card_view_->Update();
  Layout();
  SchedulePaint();
}

void LocationBarView::OnFocus() {
  // Focus the view widget first which implements accessibility for
  // Chrome OS.  It is noop on Win. This should be removed once
  // Chrome OS migrates to aura, which uses Views' textfield that receives
  // focus. See crbug.com/106428.
  NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_FOCUS, false);

  // Then focus the native location view which implements accessibility for
  // Windows.
  location_entry_->SetFocus();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (is_popup_mode_)
    return;

  DCHECK(page_action);
  WebContents* contents = delegate_->GetWebContents();

  RefreshPageActionViews();
  PageActionWithBadgeView* page_action_view =
      static_cast<PageActionWithBadgeView*>(GetPageActionView(page_action));
  DCHECK(page_action_view);
  if (!page_action_view)
    return;

  page_action_view->image_view()->set_preview_enabled(preview_enabled);
  page_action_view->UpdateVisibility(contents, GetToolbarModel()->GetURL());
  Layout();
  SchedulePaint();
}

views::View* LocationBarView::GetPageActionView(ExtensionAction *page_action) {
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

void LocationBarView::ShowBookmarkPrompt() {
  if (star_view_ && star_view_->visible())
    BookmarkPromptView::ShowPrompt(star_view_, profile_->GetPrefs());
}

void LocationBarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  DCHECK(zoom_view_);
  RefreshZoomView();

  Layout();
  SchedulePaint();

  if (can_show_bubble && zoom_view_->visible() && delegate_->GetWebContents())
    ZoomBubbleView::ShowBubble(delegate_->GetWebContents(), true);
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

void LocationBarView::SetImeInlineAutocompletion(const string16& text) {
  ime_inline_autocomplete_view_->SetText(text);
  ime_inline_autocomplete_view_->SetVisible(!text.empty());
}

void LocationBarView::SetGrayTextAutocompletion(const string16& text) {
  if (suggested_text_view_->text() != text) {
    suggested_text_view_->SetText(text);
    suggested_text_view_->SetVisible(!text.empty());
    Layout();
    SchedulePaint();
  }
}

string16 LocationBarView::GetGrayTextAutocompletion() const {
  return HasValidSuggestText() ? suggested_text_view_->text() : string16();
}

void LocationBarView::SetLocationEntryFocusable(bool focusable) {
  OmniboxViewViews* omnibox_views = GetOmniboxViewViews(location_entry_.get());
  if (omnibox_views)
    omnibox_views->set_focusable(focusable);
  else
    set_focusable(focusable);
}

bool LocationBarView::IsLocationEntryFocusableInRootView() const {
  OmniboxViewViews* omnibox_views = GetOmniboxViewViews(location_entry_.get());
  return omnibox_views ? omnibox_views->IsFocusable() : View::IsFocusable();
}

gfx::Size LocationBarView::GetPreferredSize() {
  return background_border_painter_->GetMinimumSize();
}

void LocationBarView::Layout() {
  if (!location_entry_.get())
    return;

  selected_keyword_view_->SetVisible(false);
  location_icon_view_->SetVisible(false);
  ev_bubble_view_->SetVisible(false);
  keyword_hint_view_->SetVisible(false);

  const int item_padding = GetItemPadding();
  // The native edit has 1 px of whitespace inside it before the text when the
  // text is not scrolled off the leading edge.  The views textfield has 1 px of
  // whitespace before the text in the RTL case only.
  const int kEditLeadingInternalSpace =
      (base::i18n::IsRTL() || GetOmniboxViewWin(location_entry_.get())) ? 1 : 0;
  LocationBarLayout leading_decorations(
      LocationBarLayout::LEFT_EDGE, item_padding - kEditLeadingInternalSpace);
  LocationBarLayout trailing_decorations(LocationBarLayout::RIGHT_EDGE,
                                         item_padding);

  const string16 keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const int bubble_location_y = vertical_edge_thickness() + kBubblePadding;
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  const int location_height = GetInternalHeight(false);
  const int bubble_height = std::max(location_height - (kBubblePadding * 2), 0);
  if (!keyword.empty() && !is_keyword_hint) {
    leading_decorations.AddDecoration(bubble_location_y, bubble_height, true, 0,
                                      kBubblePadding, item_padding, 0,
                                      selected_keyword_view_);
    if (selected_keyword_view_->keyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile_)->
          GetTemplateURLForKeyword(keyword);
      if (template_url &&
          (template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION)) {
        gfx::Image image = extensions::OmniboxAPI::Get(profile_)->
            GetOmniboxIcon(template_url->GetExtensionId());
        selected_keyword_view_->SetImage(image.AsImageSkia());
        selected_keyword_view_->set_is_extension_icon(true);
      } else {
        selected_keyword_view_->SetImage(
            *(GetThemeProvider()->GetImageSkiaNamed(IDR_OMNIBOX_SEARCH)));
        selected_keyword_view_->set_is_extension_icon(false);
      }
    }
  } else if (GetToolbarModel()->GetSecurityLevel(false) ==
      ToolbarModel::EV_SECURE) {
    ev_bubble_view_->SetLabel(GetToolbarModel()->GetEVCertName());
    // The largest fraction of the omnibox that can be taken by the EV bubble.
    const double kMaxBubbleFraction = 0.5;
    leading_decorations.AddDecoration(bubble_location_y, bubble_height, false,
                                      kMaxBubbleFraction, kBubblePadding,
                                      item_padding, 0, ev_bubble_view_);
  } else {
    leading_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        GetBuiltInHorizontalPaddingForChildViews(),
        location_icon_view_);
  }

  if (star_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        GetBuiltInHorizontalPaddingForChildViews(), star_view_);
  }
  if (translate_icon_view_ && translate_icon_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        GetBuiltInHorizontalPaddingForChildViews(),
        translate_icon_view_);
  }
  if (script_bubble_icon_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        GetBuiltInHorizontalPaddingForChildViews(),
        script_bubble_icon_view_);
  }
  if (open_pdf_in_reader_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        GetBuiltInHorizontalPaddingForChildViews(),
        open_pdf_in_reader_view_);
  }
  if (manage_passwords_icon_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, 0,
                                       manage_passwords_icon_view_);
  }
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(
          vertical_edge_thickness(), location_height,
          GetBuiltInHorizontalPaddingForChildViews(), (*i));
    }
  }
  if (zoom_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, 0, zoom_view_);
  }
  for (ContentSettingViews::const_reverse_iterator i(
           content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(
          bubble_location_y, bubble_height, false, 0, item_padding,
          item_padding, GetBuiltInHorizontalPaddingForChildViews(), (*i));
    }
  }
  if (generated_credit_card_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, 0,
                                       generated_credit_card_view_);
  }
  if (mic_search_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, 0, mic_search_view_);
  }
  // Because IMEs may eat the tab key, we don't show "press tab to search" while
  // IME composition is in progress.
  if (!keyword.empty() && is_keyword_hint &&
      !location_entry_->IsImeComposing()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, true, 0, item_padding,
                                       item_padding, 0, keyword_hint_view_);
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // Perform layout.
  const int horizontal_edge_thickness = GetHorizontalEdgeThickness();
  int full_width = width() - 2 * horizontal_edge_thickness;
  int entry_width = full_width;
  leading_decorations.LayoutPass1(&entry_width);
  trailing_decorations.LayoutPass1(&entry_width);
  leading_decorations.LayoutPass2(&entry_width);
  trailing_decorations.LayoutPass2(&entry_width);

  int location_needed_width = location_entry_->TextWidth();
  int available_width = entry_width - location_needed_width;
  // The bounds must be wide enough for all the decorations to fit.
  gfx::Rect location_bounds(
      horizontal_edge_thickness, vertical_edge_thickness(),
      std::max(full_width, full_width - entry_width), location_height);
  leading_decorations.LayoutPass3(&location_bounds, &available_width);
  trailing_decorations.LayoutPass3(&location_bounds, &available_width);

  // Layout out the suggested text view right aligned to the location
  // entry. Only show the suggested text if we can fit the text from one
  // character before the end of the selection to the end of the text and the
  // suggested text. If we can't it means either the suggested text is too big,
  // or the user has scrolled.

  // TODO(sky): We could potentially adjust this to take into account suggested
  // text to force using minimum size if necessary, but currently the chance of
  // showing keyword hints and suggested text is minimal and we're not confident
  // this is the right approach for suggested text.

  OmniboxViewViews* omnibox_views =
      GetOmniboxViewViews(location_entry_.get());
  int omnibox_views_margin = 0;
  if (suggested_text_view_->visible()) {
    // We do not display the suggested text when it contains a mix of RTL and
    // LTR characters since this could mean the suggestion should be displayed
    // in the middle of the string.
    base::i18n::TextDirection text_direction =
        base::i18n::GetStringDirection(location_entry_->GetText());
    if (text_direction !=
        base::i18n::GetStringDirection(suggested_text_view_->text()))
      text_direction = base::i18n::UNKNOWN_DIRECTION;

    // TODO(sky): need to layout when the user changes caret position.
    gfx::Size suggested_text_size(suggested_text_view_->GetPreferredSize());
    if (suggested_text_size.width() > available_width ||
        text_direction == base::i18n::UNKNOWN_DIRECTION) {
      // Hide the suggested text if the user has scrolled or we can't fit all
      // the suggested text, or we have a mix of RTL and LTR characters.
      suggested_text_view_->SetBounds(0, 0, 0, 0);
    } else {
      location_needed_width =
          std::min(location_needed_width,
                   location_bounds.width() - suggested_text_size.width());
      gfx::Rect suggested_text_bounds(location_bounds.x(), location_bounds.y(),
                                      suggested_text_size.width(),
                                      location_bounds.height());
      // TODO(sky): figure out why this needs the -1.
      suggested_text_bounds.Offset(location_needed_width - 1, 0);
      // For non-views the omnibox needs to be shrunk so that the suggest text
      // is visible.
      if (!omnibox_views)
        location_bounds.set_width(location_needed_width);

      // We reverse the order of the location entry and suggested text if:
      // - Chrome is RTL but the text is fully LTR, or
      // - Chrome is LTR but the text is fully RTL.
      // This ensures the suggested text is correctly displayed to the right
      // (or left) of the user text.
      if (text_direction == (base::i18n::IsRTL() ?
          base::i18n::LEFT_TO_RIGHT : base::i18n::RIGHT_TO_LEFT)) {
        // TODO(sky): Figure out why we need the +1.
        suggested_text_bounds.set_x(location_bounds.x() + 1);
        if (omnibox_views) {
          // Use a margin to prevent the omnibox text from overlapping the
          // suggest text.
          omnibox_views_margin = suggested_text_bounds.width();
        } else {
          // Non-views doesn't support margins so move the omnibox over.
          location_bounds.set_x(
              location_bounds.x() + suggested_text_bounds.width());
        }
      }
      suggested_text_view_->SetBoundsRect(suggested_text_bounds);
    }
  }

  if (omnibox_views)
    omnibox_views->SetHorizontalMargins(0, omnibox_views_margin);

  // Layout |ime_inline_autocomplete_view_| next to the user input.
  if (ime_inline_autocomplete_view_->visible()) {
    int width =
        ime_inline_autocomplete_view_->font().GetStringWidth(
            ime_inline_autocomplete_view_->text()) +
        ime_inline_autocomplete_view_->GetInsets().width();
    // All the target languages (IMEs) are LTR, and we do not need to support
    // RTL so far.  In other words, no testable RTL environment so far.
    int x = location_needed_width;
    if (width > entry_width)
      x = 0;
    else if (location_needed_width + width > entry_width)
      x = entry_width - width;
    location_bounds.set_width(x);
    ime_inline_autocomplete_view_->SetBounds(
        location_bounds.right(), location_bounds.y(),
        std::min(width, entry_width), location_bounds.height());
  }

  location_entry_view_->SetBoundsRect(location_bounds);
}

void LocationBarView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  // Fill the location bar background color behind the border.  Parts of the
  // border images are meant to rest atop the toolbar background and parts atop
  // the omnibox background, so we can't just blindly fill our entire bounds.
  const int horizontal_edge_thickness = GetHorizontalEdgeThickness();
  if (!background_filling_painter_) {
    gfx::Rect bounds(GetContentsBounds());
    bounds.Inset(horizontal_edge_thickness, vertical_edge_thickness());
    SkColor color(GetColor(ToolbarModel::NONE, BACKGROUND));
    if (is_popup_mode_) {
      canvas->FillRect(bounds, color);
    } else {
      SkPaint paint;
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor(color);
      const int kBorderCornerRadius = 2;
      canvas->DrawRoundRect(bounds, kBorderCornerRadius, paint);
    }
  }

  // Maximized popup windows don't draw the horizontal edges.  We implement this
  // by simply expanding the paint area outside the view by the edge thickness.
  gfx::Rect background_rect(GetContentsBounds());
  if (is_popup_mode_ && (horizontal_edge_thickness == 0))
    background_rect.Inset(-kPopupEdgeThickness, 0);
  views::Painter::PaintPainterAt(canvas, background_border_painter_.get(),
                                 background_rect);
  if (background_filling_painter_)
    background_filling_painter_->Paint(canvas, size());

  if (!is_popup_mode_)
    PaintPageActionBackgrounds(canvas);

  // For non-InstantExtendedAPI cases, if necessary, show focus rect.
  // Note: |Canvas::DrawFocusRect| paints a dashed rect with gray color.
  if (show_focus_rect_ && HasFocus()) {
    gfx::Rect r = location_entry_view_->bounds();
    // TODO(jamescook): Is this still needed?
    r.Inset(-1, 0);
#if defined(OS_WIN)
    r.Inset(0, -1);
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
bool LocationBarView::OnMousePressed(const ui::MouseEvent& event) {
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

bool LocationBarView::OnMouseDragged(const ui::MouseEvent& event) {
  OnMouseEvent(event, WM_MOUSEMOVE);
  return true;
}

void LocationBarView::OnMouseReleased(const ui::MouseEvent& event) {
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
  OmniboxViewWin* omnibox_win = GetOmniboxViewWin(location_entry_.get());
  if (omnibox_win)
    omnibox_win->HandleExternalMsg(WM_CAPTURECHANGED, 0, CPoint());
}
#endif

views::View* LocationBarView::generated_credit_card_view() {
  return generated_credit_card_view_;
}

void LocationBarView::Update(const WebContents* contents) {
  // UpdateManagePasswordsIconAndBubble does not need to be called here. This is
  // handled by the PasswordObserver in ManagePasswordsIconController once we
  // know that a password form was submitted.
  mic_search_view_->SetVisible(
      !GetToolbarModel()->input_in_progress() && browser_ &&
      browser_->search_model()->voice_search_supported());
  RefreshContentSettingViews();
  generated_credit_card_view_->Update();
  ZoomBubbleView::CloseBubble();
  RefreshZoomView();
  RefreshPageActionViews();
  RefreshScriptBubble();
  RefreshTranslateIcon();
  open_pdf_in_reader_view_->Update(
      GetToolbarModel()->input_in_progress() ? NULL : GetWebContents());

  bool star_enabled = browser_defaults::bookmarks_enabled && !is_popup_mode_ &&
      star_view_ && !GetToolbarModel()->input_in_progress() &&
      edit_bookmarks_enabled_.GetValue();

  command_updater()->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, star_enabled);
  command_updater()->UpdateCommandEnabled(IDC_BOOKMARK_PAGE_FROM_STAR,
                                          star_enabled);
  if (star_view_)
    star_view_->SetVisible(star_enabled);

  if (contents)
    location_entry_->OnTabChanged(contents);
  else
    location_entry_->Update();

  OnChanged();  // NOTE: Calls Layout().
}

void LocationBarView::OnChanged() {
  location_icon_view_->SetImage(
      GetThemeProvider()->GetImageSkiaNamed(location_entry_->GetIcon()));
  location_icon_view_->ShowTooltip(!GetLocationEntry()->IsEditingOrEmpty());

  Layout();
  SchedulePaint();
}

void LocationBarView::OnSetFocus() {
  GetFocusManager()->SetFocusedView(this);
}

InstantController* LocationBarView::GetInstant() {
  return delegate_->GetInstant();
}

WebContents* LocationBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

ToolbarModel* LocationBarView::GetToolbarModel() {
  return delegate_->GetToolbarModel();
}

const ToolbarModel* LocationBarView::GetToolbarModel() const {
  return delegate_->GetToolbarModel();
}

const char* LocationBarView::GetClassName() const {
  return kViewClassName;
}

bool LocationBarView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
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
  }

#if defined(USE_AURA)
  NOTIMPLEMENTED();
#else
  OmniboxViewWin* omnibox_win = GetOmniboxViewWin(location_entry_.get());
  if (omnibox_win)
    return omnibox_win->SkipDefaultKeyEventProcessing(event);
#endif  // USE_AURA
#endif  // OS_WIN

  // This method is not used for Linux ports. See FocusManager::OnKeyEvent() in
  // src/ui/views/focus/focus_manager.cc for details.
  return false;
}

void LocationBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (!location_entry_)
    return;

  state->role = ui::AccessibilityTypes::ROLE_LOCATION_BAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
  state->value = location_entry_->GetText();

  string16::size_type entry_start;
  string16::size_type entry_end;
  location_entry_->GetSelectionBounds(&entry_start, &entry_end);
  state->selection_start = entry_start;
  state->selection_end = entry_end;

  if (is_popup_mode_) {
    state->state |= ui::AccessibilityTypes::STATE_READONLY;
  } else {
    state->set_value_callback =
        base::Bind(&LocationBarView::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }
}

bool LocationBarView::HasFocus() const {
  return location_entry_->model()->has_focus();
}

void LocationBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (browser_ && browser_->instant_controller() && parent())
    browser_->instant_controller()->SetOmniboxBounds(bounds());
  OmniboxPopupView* popup = location_entry_->model()->popup_model()->view();
  if (popup->IsOpen())
    popup->UpdatePopupAppearance();
}

void LocationBarView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK_EQ(mic_search_view_, sender);
  command_updater()->ExecuteCommand(IDC_TOGGLE_SPEECH_INPUT);
}

void LocationBarView::WriteDragDataForView(views::View* sender,
                                           const gfx::Point& press_pt,
                                           OSExchangeData* data) {
  DCHECK_NE(GetDragOperationsForView(sender, press_pt),
            ui::DragDropTypes::DRAG_NONE);

  WebContents* web_contents = GetWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_tab_helper->GetFavicon().AsImageSkia();
  button_drag_utils::SetURLAndDragImage(web_contents->GetURL(),
                                        web_contents->GetTitle(),
                                        favicon,
                                        data,
                                        sender->GetWidget());
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK((sender == location_icon_view_) || (sender == ev_bubble_view_));
  WebContents* web_contents = delegate_->GetWebContents();
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

GURL LocationBarView::GetDestinationURL() const {
  return destination_url();
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition();
}

content::PageTransition LocationBarView::GetPageTransition() const {
  return transition();
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
        page_action_views_[i]->image_view()->ExecuteAction(
            ExtensionPopup::SHOW);
        return;
      }
      ++current;
    }
  }

  NOTREACHED();
}

bool LocationBarView::GetBookmarkStarVisibility() {
  DCHECK(star_view_);
  return star_view_->visible();
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
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED: {
      // Only update if the updated action box was for the active tab contents.
      WebContents* target_tab = content::Details<WebContents>(details).ptr();
      if (target_tab == GetWebContents())
        UpdatePageActions();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification.";
  }
}

void LocationBarView::ModelChanged(const SearchModel::State& old_state,
                                   const SearchModel::State& new_state) {
  const bool visible = !GetToolbarModel()->input_in_progress() &&
      new_state.voice_search_supported;
  if (mic_search_view_->visible() != visible) {
    mic_search_view_->SetVisible(visible);
    Layout();
  }
}

int LocationBarView::GetInternalHeight(bool use_preferred_size) {
  int total_height =
      use_preferred_size ? GetPreferredSize().height() : height();
  return std::max(total_height - (vertical_edge_thickness() * 2), 0);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private:

// static
int LocationBarView::GetBuiltInHorizontalPaddingForChildViews() {
  return (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) ?
      GetItemPadding() / 2 : 0;
}

int LocationBarView::GetHorizontalEdgeThickness() const {
  // In maximized popup mode, there isn't any edge.
  return (is_popup_mode_ && browser_ && browser_->window() &&
      browser_->window()->IsMaximized()) ? 0 : vertical_edge_thickness();
}

void LocationBarView::RefreshContentSettingViews() {
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->Update(GetToolbarModel()->input_in_progress() ?
        NULL : GetWebContents());
  }
}

void LocationBarView::DeletePageActionViews() {
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    RemoveChildView(*i);
  STLDeleteElements(&page_action_views_);
}

void LocationBarView::RefreshPageActionViews() {
  if (is_popup_mode_)
    return;

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  std::map<ExtensionAction*, bool> old_visibility;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    old_visibility[(*i)->image_view()->page_action()] = (*i)->visible();
  }

  std::vector<ExtensionAction*> new_page_actions;

  WebContents* contents = delegate_->GetWebContents();
  if (contents) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(contents);
    extensions::LocationBarController* controller =
        extensions_tab_helper->location_bar_controller();
    new_page_actions = controller->GetCurrentActions();
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions_ != new_page_actions) {
    page_actions_.swap(new_page_actions);
    DeletePageActionViews();  // Delete the old views (if any).

    page_action_views_.resize(page_actions_.size());
    View* right_anchor = open_pdf_in_reader_view_;
    if (!right_anchor)
      right_anchor = star_view_;
    if (!right_anchor)
      right_anchor = script_bubble_icon_view_;
    DCHECK(right_anchor);

    // Add the page actions in reverse order, so that the child views are
    // inserted in left-to-right order for accessibility.
    for (int i = page_actions_.size() - 1; i >= 0; --i) {
      page_action_views_[i] = new PageActionWithBadgeView(
          delegate_->CreatePageActionImageView(this, page_actions_[i]));
      page_action_views_[i]->SetVisible(false);
      AddChildViewAt(page_action_views_[i], GetIndexOf(right_anchor));
    }
  }

  if (!page_action_views_.empty() && contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(contents);
    GURL url = browser->tab_strip_model()->GetActiveWebContents()->GetURL();

    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(
          GetToolbarModel()->input_in_progress() ? NULL : contents, url);

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

size_t LocationBarView::ScriptBubbleScriptsRunning() {
  WebContents* contents = delegate_->GetWebContents();
  if (!contents)
    return false;
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (!extensions_tab_helper)
    return false;
  extensions::ScriptBubbleController* script_bubble_controller =
      extensions_tab_helper->script_bubble_controller();
  if (!script_bubble_controller)
    return false;
  size_t script_count =
      script_bubble_controller->extensions_running_scripts().size();
  return script_count;
}

void LocationBarView::RefreshScriptBubble() {
  if (!script_bubble_icon_view_)
    return;
  size_t script_count = ScriptBubbleScriptsRunning();
  script_bubble_icon_view_->SetVisible(script_count > 0);
  if (script_count > 0)
    script_bubble_icon_view_->SetScriptCount(script_count);
}

void LocationBarView::RefreshZoomView() {
  DCHECK(zoom_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  zoom_view_->Update(ZoomController::FromWebContents(web_contents));
}

void LocationBarView::RefreshManagePasswordsIconView() {
  DCHECK(manage_passwords_icon_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  manage_passwords_icon_view_->Update(
      ManagePasswordsIconController::FromWebContents(web_contents));
}

void LocationBarView::RefreshTranslateIcon() {
  if (!translate_icon_view_)
    return;

  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  bool enabled =
      translate_tab_helper->language_state().translate_enabled();

  command_updater()->UpdateCommandEnabled(IDC_TRANSLATE_PAGE, enabled);
  translate_icon_view_->SetVisible(enabled);
}

void LocationBarView::ShowManagePasswordsBubbleIfNeeded() {
  DCHECK(manage_passwords_icon_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  manage_passwords_icon_view_->ShowBubbleIfNeeded(
      ManagePasswordsIconController::FromWebContents(web_contents));
}

#if defined(OS_WIN) && !defined(USE_AURA)
void LocationBarView::OnMouseEvent(const ui::MouseEvent& event, UINT msg) {
  OmniboxViewWin* omnibox_win = GetOmniboxViewWin(location_entry_.get());
  if (omnibox_win) {
    UINT flags = event.native_event().wParam;
    gfx::Point screen_point(event.location());
    ConvertPointToScreen(this, &screen_point);
    omnibox_win->HandleExternalMsg(msg, flags, screen_point.ToPOINT());
  }
}
#endif

bool LocationBarView::HasValidSuggestText() const {
  return suggested_text_view_->visible() &&
      !suggested_text_view_->size().IsEmpty();
}

void LocationBarView::ShowFirstRunBubbleInternal() {
#if !defined(OS_CHROMEOS)
  // First run bubble doesn't make sense for Chrome OS.
  Browser* browser = GetBrowserFromDelegate(delegate_);
  if (!browser)
    return; // Possible when browser is shutting down.

  FirstRunBubble::ShowBubble(browser, location_icon_view_);
#endif
}

void LocationBarView::PaintPageActionBackgrounds(gfx::Canvas* canvas) {
  WebContents* web_contents = GetWebContents();
  // web_contents may be NULL while the browser is shutting down.
  if (!web_contents)
    return;

  const int32 tab_id = SessionID::IdForTab(web_contents);
  const ToolbarModel::SecurityLevel security_level =
      GetToolbarModel()->GetSecurityLevel(false);
  const SkColor text_color = GetColor(security_level, TEXT);
  const SkColor background_color = GetColor(security_level, BACKGROUND);

  for (PageActionViews::const_iterator
           page_action_view = page_action_views_.begin();
       page_action_view != page_action_views_.end();
       ++page_action_view) {
    gfx::Rect bounds = (*page_action_view)->bounds();
    int horizontal_padding =
        GetItemPadding() - GetBuiltInHorizontalPaddingForChildViews();
    // Make the bounding rectangle include the whole vertical range of the
    // location bar, and the mid-point pixels between adjacent page actions.
    //
    // For odd horizontal_paddings, "horizontal_padding + 1" includes the
    // mid-point between two page actions in the bounding rectangle.  For even
    // paddings, the +1 is dropped, which is right since there is no pixel at
    // the mid-point.
    bounds.Inset(-(horizontal_padding + 1) / 2, 0);
    location_bar_util::PaintExtensionActionBackground(
        *(*page_action_view)->image_view()->page_action(),
        tab_id, canvas, bounds, text_color, background_color);
  }
}

void LocationBarView::AccessibilitySetValue(const string16& new_value) {
  location_entry_->SetUserText(new_value);
}
