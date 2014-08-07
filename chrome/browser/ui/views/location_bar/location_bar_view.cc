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
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/add_to_app_launcher_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/generated_credit_card_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/open_pdf_in_reader_view.h"
#include "chrome/browser/ui/views/location_bar/origin_chip_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/location_bar/search_button.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/translate_icon_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/translate/core/browser/language_state.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/component_scaled_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/first_run_bubble.h"
#endif

using content::WebContents;
using views::View;

namespace {

const gfx::Tween::Type kShowTweenType = gfx::Tween::LINEAR_OUT_SLOW_IN;
const gfx::Tween::Type kHideTweenType = gfx::Tween::FAST_OUT_LINEAR_IN;

// The search button images are made to look as if they overlay the normal edge
// images, but to align things, the search button needs to be inset horizontally
// by 1 px.
const int kSearchButtonInset = 1;

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
    font_list = font_list.DeriveWithSizeDelta(-1);
  }
  return font_list;
}

int GetEditLeadingInternalSpace() {
  // The textfield has 1 px of whitespace before the text in the RTL case only.
  return base::i18n::IsRTL() ? 1 : 0;
}

// Functor for moving BookmarkManagerPrivate page actions to the right via
// stable_partition.
class IsPageActionViewRightAligned {
 public:
  explicit IsPageActionViewRightAligned(
      extensions::ExtensionRegistry* extension_registry)
      : extension_registry_(extension_registry) {}

  bool operator()(PageActionWithBadgeView* page_action_view) {
    return extension_registry_->enabled_extensions().GetByID(
        page_action_view->image_view()->extension_action()->extension_id())->
        permissions_data()->
        HasAPIPermission(extensions::APIPermission::kBookmarkManagerPrivate);
  }

 private:
  extensions::ExtensionRegistry* extension_registry_;

  // NOTE: Can't DISALLOW_COPY_AND_ASSIGN as we pass this object by value to
  // std::stable_partition().
};

}  // namespace


// LocationBarView -----------------------------------------------------------

// static
const int LocationBarView::kNormalEdgeThickness = 2;
const int LocationBarView::kPopupEdgeThickness = 1;
const int LocationBarView::kItemPadding = 3;
const int LocationBarView::kIconInternalPadding = 2;
const int LocationBarView::kBubblePadding = 1;
const char LocationBarView::kViewClassName[] = "LocationBarView";

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : LocationBar(profile),
      OmniboxEditController(command_updater),
      browser_(browser),
      omnibox_view_(NULL),
      delegate_(delegate),
      origin_chip_view_(NULL),
      location_icon_view_(NULL),
      ev_bubble_view_(NULL),
      ime_inline_autocomplete_view_(NULL),
      selected_keyword_view_(NULL),
      suggested_text_view_(NULL),
      keyword_hint_view_(NULL),
      mic_search_view_(NULL),
      zoom_view_(NULL),
      generated_credit_card_view_(NULL),
      open_pdf_in_reader_view_(NULL),
      manage_passwords_icon_view_(NULL),
      translate_icon_view_(NULL),
      add_to_app_launcher_view_(NULL),
      star_view_(NULL),
      search_button_(NULL),
      is_popup_mode_(is_popup_mode),
      show_focus_rect_(false),
      template_url_service_(NULL),
      dropdown_animation_offset_(0),
      starting_omnibox_offset_(0),
      current_omnibox_offset_(0),
      starting_omnibox_leading_inset_(0),
      current_omnibox_leading_inset_(0),
      current_omnibox_width_(0),
      ending_omnibox_width_(0) {
  edit_bookmarks_enabled_.Init(
      prefs::kEditBookmarksEnabled, profile->GetPrefs(),
      base::Bind(&LocationBarView::Update, base::Unretained(this),
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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public:

void LocationBarView::Init() {
  // We need to be in a Widget, otherwise GetNativeTheme() may change and we're
  // not prepared for that.
  DCHECK(GetWidget());

  const int kOmniboxPopupBorderImages[] =
      IMAGE_GRID(IDR_OMNIBOX_POPUP_BORDER_AND_SHADOW);
  const int kOmniboxBorderImages[] = IMAGE_GRID(IDR_TEXTFIELD);
  border_painter_.reset(views::Painter::CreateImageGridPainter(
      is_popup_mode_ ? kOmniboxPopupBorderImages : kOmniboxBorderImages));

  location_icon_view_ = new LocationIconView(this);
  location_icon_view_->set_drag_controller(this);
  AddChildView(location_icon_view_);

  // Determine the main font.
  gfx::FontList font_list = ResourceBundle::GetSharedInstance().GetFontList(
      ResourceBundle::BaseFont);
  const int current_font_size = font_list.GetFontSize();
  const int desired_font_size = browser_defaults::kOmniboxFontPixelSize;
  if (current_font_size != desired_font_size) {
    font_list =
        font_list.DeriveWithSizeDelta(desired_font_size - current_font_size);
  }
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
  omnibox_view_ = new OmniboxViewViews(
      this, profile(), command_updater(),
      is_popup_mode_ ||
          (browser_->is_app() && CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kEnableStreamlinedHostedApps)),
      this, font_list);
  omnibox_view_->Init();
  omnibox_view_->SetFocusable(true);
  AddChildView(omnibox_view_);

  // Initialize the inline autocomplete view which is visible only when IME is
  // turned on.  Use the same font with the omnibox and highlighted background.
  ime_inline_autocomplete_view_ = new views::Label(base::string16(), font_list);
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

  origin_chip_view_ = new OriginChipView(this, profile(), font_list);
  origin_chip_view_->SetFocusable(false);
  origin_chip_view_->set_drag_controller(this);
  AddChildView(origin_chip_view_);

  const SkColor text_color = GetColor(ToolbarModel::NONE, TEXT);
  selected_keyword_view_ = new SelectedKeywordView(
      bubble_font_list, text_color, background_color, profile());
  AddChildView(selected_keyword_view_);

  suggested_text_view_ = new views::Label(base::string16(), font_list);
  suggested_text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  suggested_text_view_->SetAutoColorReadabilityEnabled(false);
  suggested_text_view_->SetEnabledColor(GetColor(
      ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT));
  suggested_text_view_->SetVisible(false);
  AddChildView(suggested_text_view_);

  keyword_hint_view_ = new KeywordHintView(
      profile(), font_list,
      GetColor(ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT),
      background_color);
  AddChildView(keyword_hint_view_);

  mic_search_view_ = new views::ImageButton(this);
  mic_search_view_->set_id(VIEW_ID_MIC_SEARCH_BUTTON);
  mic_search_view_->SetAccessibilityFocusable(true);
  mic_search_view_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_MIC_SEARCH));
  mic_search_view_->SetImage(
      views::Button::STATE_NORMAL,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_OMNIBOX_MIC_SEARCH));
  mic_search_view_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                      views::ImageButton::ALIGN_MIDDLE);
  mic_search_view_->SetVisible(false);
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

  open_pdf_in_reader_view_ = new OpenPDFInReaderView();
  AddChildView(open_pdf_in_reader_view_);

  manage_passwords_icon_view_ = new ManagePasswordsIconView(command_updater());
  AddChildView(manage_passwords_icon_view_);

  translate_icon_view_ = new TranslateIconView(command_updater());
  translate_icon_view_->SetVisible(false);
  AddChildView(translate_icon_view_);

  add_to_app_launcher_view_ = new AddToAppLauncherView(
      this, bubble_font_list, text_color, background_color);
  add_to_app_launcher_view_->SetVisible(false);
  AddChildView(add_to_app_launcher_view_);

  star_view_ = new StarView(command_updater());
  star_view_->SetVisible(false);
  AddChildView(star_view_);

  search_button_ = new SearchButton(this);
  search_button_->SetVisible(false);
  AddChildView(search_button_);

  show_url_animation_.reset(new gfx::SlideAnimation(this));
  show_url_animation_->SetTweenType(kShowTweenType);
  show_url_animation_->SetSlideDuration(200);

  hide_url_animation_.reset(new gfx::SlideAnimation(this));
  hide_url_animation_->SetTweenType(kHideTweenType);
  hide_url_animation_->SetSlideDuration(175);

  content::Source<Profile> profile_source = content::Source<Profile>(profile());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 profile_source);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 profile_source);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);
}

bool LocationBarView::IsInitialized() const {
  return omnibox_view_ != NULL;
}

SkColor LocationBarView::GetColor(ToolbarModel::SecurityLevel security_level,
                                  ColorKind kind) const {
  const ui::NativeTheme* native_theme = GetNativeTheme();
  switch (kind) {
    case BACKGROUND:
      return native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);

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

void LocationBarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  DCHECK(zoom_view_);
  if (RefreshZoomView()) {
    Layout();
    SchedulePaint();
  }

  WebContents* web_contents = GetWebContents();
  if (can_show_bubble && zoom_view_->visible() && web_contents)
    ZoomBubbleView::ShowBubble(web_contents, true);
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (is_popup_mode_)
    return;

  DCHECK(page_action);
  WebContents* web_contents = GetWebContents();

  RefreshPageActionViews();
  PageActionWithBadgeView* page_action_view =
      static_cast<PageActionWithBadgeView*>(GetPageActionView(page_action));
  DCHECK(page_action_view);
  if (!page_action_view)
    return;

  page_action_view->image_view()->set_preview_enabled(preview_enabled);
  page_action_view->UpdateVisibility(web_contents);
  Layout();
  SchedulePaint();
}

PageActionWithBadgeView* LocationBarView::GetPageActionView(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->image_view()->extension_action() == page_action)
      return *i;
  }
  return NULL;
}

void LocationBarView::SetStarToggled(bool on) {
  if (star_view_)
    star_view_->SetToggled(on);
}

void LocationBarView::SetTranslateIconToggled(bool on) {
  translate_icon_view_->SetToggled(on);
}

gfx::Point LocationBarView::GetOmniboxViewOrigin() const {
  gfx::Point origin(omnibox_view_->bounds().origin());
  origin.set_x(GetMirroredXInView(origin.x() - current_omnibox_offset_));
  views::View::ConvertPointToScreen(this, &origin);
  return origin;
}

void LocationBarView::SetImeInlineAutocompletion(const base::string16& text) {
  ime_inline_autocomplete_view_->SetText(text);
  ime_inline_autocomplete_view_->SetVisible(!text.empty());
}

void LocationBarView::SetGrayTextAutocompletion(const base::string16& text) {
  if (suggested_text_view_->text() != text) {
    suggested_text_view_->SetText(text);
    suggested_text_view_->SetVisible(!text.empty());
    Layout();
    SchedulePaint();
  }
}

base::string16 LocationBarView::GetGrayTextAutocompletion() const {
  return HasValidSuggestText() ?
      suggested_text_view_->text() : base::string16();
}

void LocationBarView::SetShowFocusRect(bool show) {
  show_focus_rect_ = show;
  SchedulePaint();
}

void LocationBarView::SelectAll() {
  omnibox_view_->SelectAll(true);
}

gfx::Point LocationBarView::GetLocationBarAnchorPoint() const {
  // The +1 in the next line creates a 1-px gap between icon and arrow tip.
  gfx::Point icon_bottom(0, location_icon_view_->GetImageBounds().bottom() -
      LocationBarView::kIconInternalPadding + 1);
  gfx::Point icon_center(location_icon_view_->GetImageBounds().CenterPoint());
  gfx::Point point(icon_center.x(), icon_bottom.y());
  ConvertPointToTarget(location_icon_view_, this, &point);
  return point;
}

views::View* LocationBarView::generated_credit_card_view() {
  return generated_credit_card_view_;
}

int LocationBarView::GetInternalHeight(bool use_preferred_size) {
  int total_height =
      use_preferred_size ? GetPreferredSize().height() : height();
  return std::max(total_height - (vertical_edge_thickness() * 2), 0);
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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public LocationBar implementation:

void LocationBarView::FocusLocation(bool select_all) {
  omnibox_view_->SetFocus();
  if (select_all)
    omnibox_view_->SelectAll(true);
}

void LocationBarView::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* LocationBarView::GetOmniboxView() {
  return omnibox_view_;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public views::View implementation:

bool LocationBarView::HasFocus() const {
  return omnibox_view_->model()->has_focus();
}

void LocationBarView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
}

gfx::Size LocationBarView::GetPreferredSize() const {
  // Compute minimum height.
  gfx::Size min_size(border_painter_->GetMinimumSize());
  if (!IsInitialized())
    return min_size;
  gfx::Size search_button_min_size(search_button_->GetMinimumSize());
  min_size.SetToMax(search_button_min_size);

  // Compute width of omnibox-leading content.
  const int horizontal_edge_thickness = GetHorizontalEdgeThickness();
  int leading_width = horizontal_edge_thickness;
  // TODO(pkasting): Make the origin chip min width sane, and make the chip
  // handle being shrunken down more gracefully; then uncomment this.
  /*if (GetToolbarModel()->ShouldShowOriginChip())
    leading_width += origin_chip_view_->GetMinimumSize().width();*/
  if (ShouldShowKeywordBubble()) {
    // The selected keyword view can collapse completely.
  } else if (ShouldShowEVBubble()) {
    leading_width += kBubblePadding +
        ev_bubble_view_->GetMinimumSizeForLabelText(
            GetToolbarModel()->GetEVCertName()).width();
  } else if (!origin_chip_view_->visible()) {
    leading_width +=
        kItemPadding + location_icon_view_->GetMinimumSize().width();
  }
  leading_width += kItemPadding - GetEditLeadingInternalSpace();

  // Compute width of omnibox-trailing content.
  int trailing_width = search_button_->visible() ?
      (search_button_->GetMinimumSize().width() + kSearchButtonInset) :
      horizontal_edge_thickness;
  trailing_width += IncrementalMinimumWidth(star_view_) +
      IncrementalMinimumWidth(translate_icon_view_) +
      IncrementalMinimumWidth(open_pdf_in_reader_view_) +
      IncrementalMinimumWidth(manage_passwords_icon_view_) +
      IncrementalMinimumWidth(zoom_view_) +
      IncrementalMinimumWidth(generated_credit_card_view_) +
      IncrementalMinimumWidth(mic_search_view_) +
      IncrementalMinimumWidth(add_to_app_launcher_view_) + kItemPadding;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    trailing_width += IncrementalMinimumWidth((*i));
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i)
    trailing_width += IncrementalMinimumWidth((*i));

  min_size.set_width(
      leading_width + omnibox_view_->GetMinimumSize().width() + trailing_width);
  return min_size;
}

void LocationBarView::Layout() {
  if (!IsInitialized())
    return;

  origin_chip_view_->SetVisible(GetToolbarModel()->ShouldShowOriginChip());
  selected_keyword_view_->SetVisible(false);
  location_icon_view_->SetVisible(false);
  ev_bubble_view_->SetVisible(false);
  keyword_hint_view_->SetVisible(false);

  LocationBarLayout leading_decorations(
      LocationBarLayout::LEFT_EDGE,
      kItemPadding - GetEditLeadingInternalSpace());
  LocationBarLayout trailing_decorations(LocationBarLayout::RIGHT_EDGE,
                                         kItemPadding);

  const int origin_chip_preferred_width =
      origin_chip_view_->GetPreferredSize().width();
  const int origin_chip_width =
      origin_chip_view_->visible() ? origin_chip_preferred_width : 0;
  // Always give the origin chip view its desired size and lay it out, even when
  // it's not visible, so we can calculate the correct animation values below
  // when switching to tabs that have the origin chip hidden.
  origin_chip_view_->SetBounds(0, 0, origin_chip_preferred_width, height());
  origin_chip_view_->Layout();

  const int bubble_location_y = vertical_edge_thickness() + kBubblePadding;
  const base::string16 keyword(omnibox_view_->model()->keyword());
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  const int location_height = GetInternalHeight(false);
  const int bubble_height = std::max(location_height - (kBubblePadding * 2), 0);
  if (ShouldShowKeywordBubble()) {
    leading_decorations.AddDecoration(bubble_location_y, bubble_height, true, 0,
                                      kBubblePadding, kItemPadding,
                                      selected_keyword_view_);
    if (selected_keyword_view_->keyword() != keyword) {
      selected_keyword_view_->SetKeyword(keyword);
      const TemplateURL* template_url =
          TemplateURLServiceFactory::GetForProfile(profile())->
          GetTemplateURLForKeyword(keyword);
      if (template_url &&
          (template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION)) {
        gfx::Image image = extensions::OmniboxAPI::Get(profile())->
            GetOmniboxIcon(template_url->GetExtensionId());
        selected_keyword_view_->SetImage(image.AsImageSkia());
        selected_keyword_view_->set_is_extension_icon(true);
      } else {
        selected_keyword_view_->SetImage(
            *(GetThemeProvider()->GetImageSkiaNamed(IDR_OMNIBOX_SEARCH)));
        selected_keyword_view_->set_is_extension_icon(false);
      }
    }
  } else if (ShouldShowEVBubble()) {
    ev_bubble_view_->SetLabel(GetToolbarModel()->GetEVCertName());
    // The largest fraction of the omnibox that can be taken by the EV bubble.
    const double kMaxBubbleFraction = 0.5;
    leading_decorations.AddDecoration(bubble_location_y, bubble_height, false,
                                      kMaxBubbleFraction, kBubblePadding,
                                      kItemPadding, ev_bubble_view_);
  } else if (!origin_chip_view_->visible()) {
    leading_decorations.AddDecoration(
        vertical_edge_thickness(), location_height,
        location_icon_view_);
  }

  if (star_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height, star_view_);
  }
  if (add_to_app_launcher_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height, add_to_app_launcher_view_);
  }
  if (translate_icon_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height, translate_icon_view_);
  }
  if (open_pdf_in_reader_view_->visible()) {
    trailing_decorations.AddDecoration(
        vertical_edge_thickness(), location_height, open_pdf_in_reader_view_);
  }
  if (manage_passwords_icon_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height,
                                       manage_passwords_icon_view_);
  }
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(
          vertical_edge_thickness(), location_height, (*i));
    }
  }
  if (zoom_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, zoom_view_);
  }
  for (ContentSettingViews::const_reverse_iterator i(
           content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(
          bubble_location_y, bubble_height, false, 0, kItemPadding,
          kItemPadding, (*i));
    }
  }
  if (generated_credit_card_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height,
                                       generated_credit_card_view_);
  }
  if (mic_search_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, mic_search_view_);
  }
  // Because IMEs may eat the tab key, we don't show "press tab to search" while
  // IME composition is in progress.
  if (!keyword.empty() && omnibox_view_->model()->is_keyword_hint() &&
      !omnibox_view_->IsImeComposing()) {
    trailing_decorations.AddDecoration(vertical_edge_thickness(),
                                       location_height, true, 0, kItemPadding,
                                       kItemPadding, keyword_hint_view_);
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // Perform layout.
  const int horizontal_edge_thickness = GetHorizontalEdgeThickness();
  int full_width = width() - horizontal_edge_thickness - origin_chip_width;

  const gfx::Size search_button_size(search_button_->GetPreferredSize());
  const int search_button_reserved_width =
      search_button_size.width() + kSearchButtonInset;
  full_width -= search_button_->visible() ?
      search_button_reserved_width : horizontal_edge_thickness;
  int entry_width = full_width;
  leading_decorations.LayoutPass1(&entry_width);
  trailing_decorations.LayoutPass1(&entry_width);
  leading_decorations.LayoutPass2(&entry_width);
  trailing_decorations.LayoutPass2(&entry_width);

  int location_needed_width = omnibox_view_->GetTextWidth();
  int available_width = entry_width - location_needed_width;
  // The bounds must be wide enough for all the decorations to fit.
  gfx::Rect location_bounds(
      origin_chip_width + horizontal_edge_thickness, vertical_edge_thickness(),
      std::max(full_width, full_width - entry_width), location_height);
  leading_decorations.LayoutPass3(&location_bounds, &available_width);
  trailing_decorations.LayoutPass3(&location_bounds, &available_width);

  // Calculate the animation parameters (see comments on these members in the
  // header).  We have to do this in Layout, after |origin_chip_view_| is laid
  // out, because that may affect the host label offset in the origin chip.
  const base::string16& chip_text(origin_chip_view_->host_label_text());
  // If the chip is clicked, the omnibox text will become the toolbar model's
  // formatted URL.  We can't ask the omnibox for its current text, because
  // while the chip is visible the current text is empty.
  size_t prefix_end = 0;
  const base::string16& omnibox_text(
      GetToolbarModel()->GetFormattedURL(&prefix_end));
  // Do a case-insensitive search to better match cases like
  // "Settings" <-> "chrome://settings".  Skip any pre-hostname text.
  size_t chip_text_offset = std::search(
      omnibox_text.begin() + prefix_end, omnibox_text.end(),
      chip_text.begin(), chip_text.end(),
      base::CaseInsensitiveCompare<base::char16>()) - omnibox_text.begin();
  // If we couldn't find the chip text, try checking whether the omnibox text
  // starts with it, as is true for e.g. file: URLs.
  if ((chip_text_offset >= omnibox_text.length()) &&
      StartsWith(omnibox_text, chip_text, true))
    chip_text_offset = 0;
  const gfx::FontList& font_list = omnibox_view_->GetFontList();
  const int chip_text_width = gfx::GetStringWidth(chip_text, font_list);
  const int old_starting_offset = starting_omnibox_offset_;
  const int old_starting_leading_inset = starting_omnibox_leading_inset_;
  const int old_ending_width = ending_omnibox_width_;
  starting_omnibox_offset_ = current_omnibox_offset_ = 0;
  starting_omnibox_leading_inset_ = current_omnibox_leading_inset_ = 0;
  ending_omnibox_width_ = gfx::GetStringWidth(omnibox_text, font_list);
  if (chip_text_offset < omnibox_text.length()) {
    if (base::i18n::IsRTL())
      chip_text_offset += chip_text.length();
    base::string16 extra_omnibox_text(base::i18n::IsRTL() ?
        omnibox_text.substr(chip_text_offset) :
        omnibox_text.substr(0, chip_text_offset));
    starting_omnibox_leading_inset_ =
        gfx::GetStringWidth(extra_omnibox_text, font_list);
    starting_omnibox_offset_ = origin_chip_view_->HostLabelOffset() -
        starting_omnibox_leading_inset_;
    current_omnibox_width_ = chip_text_width;
  } else {
    // If the chip text wasn't found in the omnibox text, then instead of
    // starting the show animation clipped to the "hostname", we'll start with
    // the entire omnibox text visible, clipped to the remaining chip width, and
    // only animate any necessary expansion of that width, without moving the
    // omnibox bounds.
    current_omnibox_width_ = origin_chip_view_->WidthFromStartOfLabels();
  }

  // End the animations immediately if the parameters have changed.
  if ((starting_omnibox_offset_ != old_starting_offset) ||
      (starting_omnibox_leading_inset_ != old_starting_leading_inset) ||
      (ending_omnibox_width_ != old_ending_width))
    EndOriginChipAnimations(true);

  // Also end the animations immediately if there's nothing to animate (but do
  // allow the chip to fade back in).
  const ui::NativeTheme* native_theme = GetNativeTheme();
  const SkColor ending_selection_text_color = native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionColor);
  const SkColor ending_selection_background_color =
      native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);
  if ((starting_omnibox_offset_ == 0) &&
      (starting_omnibox_leading_inset_ == 0) &&
      (ending_omnibox_width_ == chip_text_width) &&
      (hide_url_animation_->is_animating() ||
       ((ending_selection_text_color ==
            origin_chip_view_->pressed_text_color()) &&
        (ending_selection_background_color ==
            origin_chip_view_->pressed_background_color()))))
    EndOriginChipAnimations(false);

  if (show_url_animation_->is_animating()) {
    omnibox_view_->SetSelectionTextColor(gfx::Tween::ColorValueBetween(
        show_url_animation_->GetCurrentValue(),
        origin_chip_view_->pressed_text_color(),
        ending_selection_text_color));
    omnibox_view_->SetSelectionBackgroundColor(gfx::Tween::ColorValueBetween(
        show_url_animation_->GetCurrentValue(),
        origin_chip_view_->pressed_background_color(),
        ending_selection_background_color));
    current_omnibox_offset_ =
        show_url_animation_->CurrentValueBetween(starting_omnibox_offset_, 0);
    current_omnibox_leading_inset_ = show_url_animation_->CurrentValueBetween(
        starting_omnibox_leading_inset_, 0);
    current_omnibox_width_ = show_url_animation_->CurrentValueBetween(
        chip_text_width, ending_omnibox_width_);
  } else if (hide_url_animation_->is_animating()) {
    current_omnibox_offset_ =
        hide_url_animation_->CurrentValueBetween(0, starting_omnibox_offset_);
    current_omnibox_leading_inset_ = hide_url_animation_->CurrentValueBetween(
        0, starting_omnibox_leading_inset_);
    current_omnibox_width_ = hide_url_animation_->CurrentValueBetween(
        ending_omnibox_width_, chip_text_width);
  }
  // Contract |available_width| as necessary, but never expand it.  This way,
  // we'll never draw suggested text at first and then have it disappear
  // midway through the animation.
  if (current_omnibox_offset_ > 0)
    available_width -= current_omnibox_offset_;
  location_bounds.Inset(current_omnibox_offset_, 0, 0, 0);

  // Layout out the suggested text view right aligned to the location
  // entry. Only show the suggested text if we can fit the text from one
  // character before the end of the selection to the end of the text and the
  // suggested text. If we can't it means either the suggested text is too big,
  // or the user has scrolled.

  // TODO(sky): We could potentially adjust this to take into account suggested
  // text to force using minimum size if necessary, but currently the chance of
  // showing keyword hints and suggested text is minimal and we're not confident
  // this is the right approach for suggested text.

  int omnibox_view_margin = 0;
  if (suggested_text_view_->visible()) {
    // We do not display the suggested text when it contains a mix of RTL and
    // LTR characters since this could mean the suggestion should be displayed
    // in the middle of the string.
    base::i18n::TextDirection text_direction =
        base::i18n::GetStringDirection(omnibox_view_->GetText());
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

      // We reverse the order of the location entry and suggested text if:
      // - Chrome is RTL but the text is fully LTR, or
      // - Chrome is LTR but the text is fully RTL.
      // This ensures the suggested text is correctly displayed to the right
      // (or left) of the user text.
      if (text_direction == (base::i18n::IsRTL() ?
          base::i18n::LEFT_TO_RIGHT : base::i18n::RIGHT_TO_LEFT)) {
        // TODO(sky): Figure out why we need the +1.
        suggested_text_bounds.set_x(location_bounds.x() + 1);
        // Use a margin to prevent omnibox text from overlapping suggest text.
        omnibox_view_margin = suggested_text_bounds.width();
      }
      suggested_text_view_->SetBoundsRect(suggested_text_bounds);
    }
  }

  const gfx::Insets insets = omnibox_view_->GetInsets();
  omnibox_view_->SetBorder(views::Border::CreateEmptyBorder(
      insets.top(), insets.left(), insets.bottom(), omnibox_view_margin));

  // Layout |ime_inline_autocomplete_view_| next to the user input.
  if (ime_inline_autocomplete_view_->visible()) {
    int width =
        gfx::GetStringWidth(ime_inline_autocomplete_view_->text(),
                            ime_inline_autocomplete_view_->font_list()) +
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

  omnibox_view_->SetBoundsRect(location_bounds);

  search_button_->SetBoundsRect(gfx::Rect(
      gfx::Point(width() - search_button_reserved_width, 0),
      search_button_size));
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public OmniboxEditController implementation:

void LocationBarView::Update(const WebContents* contents) {
  mic_search_view_->SetVisible(
      !GetToolbarModel()->input_in_progress() && browser_ &&
      browser_->search_model()->voice_search_supported());
  RefreshContentSettingViews();
  generated_credit_card_view_->Update();
  ZoomBubbleView::CloseBubble();
  TranslateBubbleView::CloseBubble();
  RefreshZoomView();
  RefreshPageActionViews();
  RefreshTranslateIcon();
  RefreshManagePasswordsIconView();
  content::WebContents* web_contents_for_sub_views =
      GetToolbarModel()->input_in_progress() ? NULL : GetWebContents();
  open_pdf_in_reader_view_->Update(web_contents_for_sub_views);
  add_to_app_launcher_view_->Update(web_contents_for_sub_views);

  if (star_view_) {
    star_view_->SetVisible(
        browser_defaults::bookmarks_enabled && !is_popup_mode_ &&
        !GetToolbarModel()->input_in_progress() &&
        edit_bookmarks_enabled_.GetValue() &&
        !IsBookmarkStarHiddenByExtension());
  }

  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();

  OnChanged();  // NOTE: Calls Layout().
}

void LocationBarView::ShowURL() {
  // Start the animation before calling ShowURL(), since the latter eventually
  // calls back to Layout(), and if the animation is not marked as "running",
  // we'll draw the omnibox in its final position briefly until the first
  // animation callback reaches us.
  if (chrome::ShouldDisplayOriginChip()) {
    // If we're currently hiding, reverse the hide by swapping to the show
    // animation, offset so that the text is in the same position.
    if (hide_url_animation_->is_animating()) {
      const double show_value = GetValueForAnimation(false);
      hide_url_animation_->Reset();
      show_url_animation_->Show();
      // This must be done after calling Show() and is not equivalent to
      // calling Reset(n) before Show(); Reset() would have caused the entire
      // animation curve (and time) to run between this value and the final
      // value, whereas Show() + SetCurrentValue() skips the animation forward
      // to the supplied value.
      show_url_animation_->SetCurrentValue(show_value);
    } else {
      show_url_animation_->Show();
    }
  }
  omnibox_view_->ShowURL();
}

void LocationBarView::EndOriginChipAnimations(bool cancel_fade) {
  show_url_animation_->End();
  hide_url_animation_->End();
  if (cancel_fade)
    origin_chip_view_->CancelFade();
}

ToolbarModel* LocationBarView::GetToolbarModel() {
  return delegate_->GetToolbarModel();
}

WebContents* LocationBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private:

// static
int LocationBarView::IncrementalMinimumWidth(views::View* view) {
  return view->visible() ? (kItemPadding + view->GetMinimumSize().width()) : 0;
}

int LocationBarView::GetHorizontalEdgeThickness() const {
  // In maximized popup mode, there isn't any edge.
  return (is_popup_mode_ && browser_ && browser_->window() &&
      browser_->window()->IsMaximized()) ? 0 : vertical_edge_thickness();
}

bool LocationBarView::RefreshContentSettingViews() {
  bool visibility_changed = false;
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    const bool was_visible = (*i)->visible();
    (*i)->Update(GetToolbarModel()->input_in_progress() ?
        NULL : GetWebContents());
    if (was_visible != (*i)->visible())
      visibility_changed = true;
  }
  return visibility_changed;
}

void LocationBarView::DeletePageActionViews() {
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    RemoveChildView(*i);
  STLDeleteElements(&page_action_views_);
}

bool LocationBarView::RefreshPageActionViews() {
  if (is_popup_mode_)
    return false;

  bool changed = false;

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  std::map<ExtensionAction*, bool> old_visibility;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    old_visibility[(*i)->image_view()->extension_action()] = (*i)->visible();
  }

  PageActions new_page_actions;

  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents);
    extensions::LocationBarController* controller =
        extensions_tab_helper->location_bar_controller();
    new_page_actions = controller->GetCurrentActions();
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions_ != new_page_actions) {
    changed = true;

    page_actions_.swap(new_page_actions);
    DeletePageActionViews();  // Delete the old views (if any).

    // Create the page action views.
    for (PageActions::const_iterator i = page_actions_.begin();
         i != page_actions_.end(); ++i) {
      PageActionWithBadgeView* page_action_view = new PageActionWithBadgeView(
          delegate_->CreatePageActionImageView(this, *i));
      page_action_view->SetVisible(false);
      page_action_views_.push_back(page_action_view);
    }

    // Move rightmost extensions to the start.
    std::stable_partition(
        page_action_views_.begin(),
        page_action_views_.end(),
        IsPageActionViewRightAligned(
            extensions::ExtensionRegistry::Get(profile())));

    View* right_anchor = open_pdf_in_reader_view_;
    if (!right_anchor)
      right_anchor = star_view_;
    DCHECK(right_anchor);

    // |page_action_views_| are ordered right-to-left.  Add them as children in
    // reverse order so the logical order and visual order match for
    // accessibility purposes.
    for (PageActionViews::reverse_iterator i = page_action_views_.rbegin();
         i != page_action_views_.rend(); ++i)
      AddChildViewAt(*i, GetIndexOf(right_anchor));
  }

  if (!page_action_views_.empty() && web_contents) {
    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(
          GetToolbarModel()->input_in_progress() ? NULL : web_contents);

      // Check if the visibility of the action changed and notify if it did.
      ExtensionAction* action = (*i)->image_view()->extension_action();
      if (old_visibility.find(action) == old_visibility.end() ||
          old_visibility[action] != (*i)->visible()) {
        changed = true;
        content::NotificationService::current()->Notify(
            extensions::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
            content::Source<ExtensionAction>(action),
            content::Details<WebContents>(web_contents));
      }
    }
  }
  return changed;
}

bool LocationBarView::RefreshZoomView() {
  DCHECK(zoom_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;
  const bool was_visible = zoom_view_->visible();
  zoom_view_->Update(ZoomController::FromWebContents(web_contents));
  return was_visible != zoom_view_->visible();
}

void LocationBarView::RefreshTranslateIcon() {
  if (!TranslateService::IsTranslateBubbleEnabled())
    return;

  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  translate::LanguageState& language_state =
      ChromeTranslateClient::FromWebContents(web_contents)->GetLanguageState();
  bool enabled = language_state.translate_enabled();
  command_updater()->UpdateCommandEnabled(IDC_TRANSLATE_PAGE, enabled);
  translate_icon_view_->SetVisible(enabled);
  translate_icon_view_->SetToggled(language_state.IsPageTranslated());
}

bool LocationBarView::RefreshManagePasswordsIconView() {
  DCHECK(manage_passwords_icon_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;
  const bool was_visible = manage_passwords_icon_view_->visible();
  ManagePasswordsUIController::FromWebContents(
      web_contents)->UpdateIconAndBubbleState(manage_passwords_icon_view_);
  return was_visible != manage_passwords_icon_view_->visible();
}

void LocationBarView::ShowFirstRunBubbleInternal() {
  // First run bubble doesn't make sense for Chrome OS.
#if !defined(OS_CHROMEOS)
  WebContents* web_contents = delegate_->GetWebContents();
  if (!web_contents)
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    FirstRunBubble::ShowBubble(browser, location_icon_view_);
#endif
}

bool LocationBarView::HasValidSuggestText() const {
  return suggested_text_view_->visible() &&
      !suggested_text_view_->size().IsEmpty();
}

bool LocationBarView::ShouldShowKeywordBubble() const {
  return !omnibox_view_->model()->keyword().empty() &&
      !omnibox_view_->model()->is_keyword_hint();
}

bool LocationBarView::ShouldShowEVBubble() const {
  return !chrome::ShouldDisplayOriginChip() &&
      (GetToolbarModel()->GetSecurityLevel(false) == ToolbarModel::EV_SECURE);
}

double LocationBarView::GetValueForAnimation(bool hide) const {
  int calculated_offset;
  const gfx::Tween::Type tween_type = hide ? kHideTweenType : kShowTweenType;
  int start_offset = starting_omnibox_offset_, end_offset = 0;
  if (hide)
    std::swap(start_offset, end_offset);
  const int desired_offset = abs(current_omnibox_offset_);
  // Binary-search the value space (0 <= value <= 1) to find the appropriate
  // position.  We only bother to iterate to within 1/64 of the desired value,
  // because the longer of the two animations will only run for twelve frames
  // anyway (200 ms * 60 Hz), so at this point we'll have a maximum error of
  // less than a fifth of an animation frame, which the user isn't going to
  // notice.
  //
  // We have to use this method because Tween::CalculateValue() is not
  // necessarily easily invertible.  Luckily, this only runs when the user
  // reverses the animation (rare), and the limit on how many iterations we'll
  // do ensures the cost is unnoticeable.
  double value = 0.5;
  double step = value / 2;
  do {
    calculated_offset = abs(gfx::Tween::IntValueBetween(
        gfx::Tween::CalculateValue(tween_type, value), start_offset,
        end_offset));
    if (calculated_offset < desired_offset)
      value += step;
    else if (calculated_offset > desired_offset)
      value -= step;
    step /= 2;
  } while ((calculated_offset != desired_offset) && (step >= (1.0 / 64)));
  return value;
}

void LocationBarView::ResetShowAnimationAndColors() {
  show_url_animation_->Reset();
  omnibox_view_->UseDefaultSelectionTextColor();
  omnibox_view_->UseDefaultSelectionBackgroundColor();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private LocationBar implementation:

void LocationBarView::ShowFirstRunBubble() {
  // Wait until search engines have loaded to show the first run bubble.
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
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
  omnibox_view_->model()->AcceptInput(CURRENT_TAB, false);
}

void LocationBarView::FocusSearch() {
  omnibox_view_->SetFocus();
  omnibox_view_->SetForcedQuery();
}

void LocationBarView::UpdateContentSettingsIcons() {
  if (RefreshContentSettingViews()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateManagePasswordsIconAndBubble() {
  if (RefreshManagePasswordsIconView()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdatePageActions() {
  size_t count_before = page_action_views_.size();
  bool changed = RefreshPageActionViews();
  if (page_action_views_.size() != count_before) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }

  if (changed) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::InvalidatePageActions() {
  size_t count_before = page_action_views_.size();
  DeletePageActionViews();
  if (page_action_views_.size() != count_before) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
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

void LocationBarView::SaveStateToContents(WebContents* contents) {
  // If we're about to switch tabs, complete any current animations, so that if
  // the user is in the midst of hiding the URL, when he returns to this tab,
  // the URL will be hidden rather than shown.
  // NOTE: This must be called before SaveStateToTab().
  EndOriginChipAnimations(true);
  omnibox_view_->SaveStateToTab(contents);
}

const OmniboxView* LocationBarView::GetOmniboxView() const {
  return omnibox_view_;
}

LocationBarTesting* LocationBarView::GetLocationBarForTesting() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private LocationBarTesting implementation:

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
    return page_action_views_[index]->image_view()->extension_action();

  NOTREACHED();
  return NULL;
}

ExtensionAction* LocationBarView::GetVisiblePageAction(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->visible()) {
      if (current == index)
        return page_action_views_[i]->image_view()->extension_action();

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
        page_action_views_[i]->image_view()->view_controller()->
            ExecuteAction(ExtensionPopup::SHOW, true);
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

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::View implementation:

const char* LocationBarView::GetClassName() const {
  return kViewClassName;
}

void LocationBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  InstantServiceFactory::GetForProfile(profile())->OnOmniboxStartMarginChanged(
      bounds().x());

  OmniboxPopupView* popup = omnibox_view_->model()->popup_model()->view();
  if (popup->IsOpen())
    popup->UpdatePopupAppearance();
}

void LocationBarView::OnFocus() {
  // Explicitly focus the omnibox so a focus ring will be displayed around it on
  // Windows.
  omnibox_view_->SetFocus();
}

void LocationBarView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  // Fill the location bar background color behind the border.  Parts of the
  // border images are meant to rest atop the toolbar background and parts atop
  // the omnibox background, so we can't just blindly fill our entire bounds.
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(GetHorizontalEdgeThickness(), vertical_edge_thickness());
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

  // The border itself will be drawn in PaintChildren() since it includes an
  // inner shadow which should be drawn over the contents.
}

void LocationBarView::PaintChildren(gfx::Canvas* canvas,
                                    const views::CullSet& cull_set) {
  // Paint all the children except for the omnibox itself, which may need to be
  // clipped if it's animating in, and the origin chip and the search button,
  // which will be painted after the border.
  for (int i = 0, count = child_count(); i < count; ++i) {
    views::View* child = child_at(i);
    if (!child->layer() && (child != omnibox_view_) &&
        (child != origin_chip_view_) && (child != search_button_))
      child->Paint(canvas, cull_set);
  }

  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    if (show_url_animation_->is_animating() ||
        hide_url_animation_->is_animating()) {
      gfx::Rect clip_rect(omnibox_view_->bounds());
      clip_rect.Inset(current_omnibox_leading_inset_, 0, 0, 0);
      clip_rect.set_width(current_omnibox_width_);
      clip_rect.set_x(GetMirroredXForRect(clip_rect));
      canvas->ClipRect(clip_rect);
    }
    omnibox_view_->Paint(canvas, cull_set);
  }

  // For non-InstantExtendedAPI cases, if necessary, show focus rect. As we need
  // the focus rect to appear on top of children we paint here rather than
  // OnPaint().
  // Note: |Canvas::DrawFocusRect| paints a dashed rect with gray color.
  if (show_focus_rect_ && HasFocus())
    canvas->DrawFocusRect(omnibox_view_->bounds());

  // Maximized popup windows don't draw the horizontal edges.  We implement this
  // by simply expanding the paint area outside the view by the edge thickness.
  gfx::Rect border_rect(GetContentsBounds());
  if (is_popup_mode_ && (GetHorizontalEdgeThickness() == 0))
    border_rect.Inset(-kPopupEdgeThickness, 0);
  views::Painter::PaintPainterAt(canvas, border_painter_.get(), border_rect);

  // The origin chip and the search button must be painted after the border so
  // that the border shadow is not drawn over them.
  origin_chip_view_->Paint(canvas, cull_set);
  search_button_->Paint(canvas, cull_set);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::ButtonListener implementation:

void LocationBarView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender == mic_search_view_) {
    command_updater()->ExecuteCommand(IDC_TOGGLE_SPEECH_INPUT);
    return;
  }

  DCHECK_EQ(search_button_, sender);
  // TODO(pkasting): When macourteau adds UMA stats for this, wire them up here.
  omnibox_view_->model()->AcceptInput(
      ui::DispositionFromEventFlags(event.flags()), false);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::DragController implementation:

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
                                        NULL,
                                        data,
                                        sender->GetWidget());
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK((sender == location_icon_view_) || (sender == ev_bubble_view_) ||
         (sender == origin_chip_view_));
  WebContents* web_contents = delegate_->GetWebContents();
  return (web_contents && web_contents->GetURL().is_valid() &&
          (!GetOmniboxView()->IsEditingOrEmpty() ||
           sender == origin_chip_view_)) ?
      (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK) :
      ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDragForView(View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private OmniboxEditController implementation:

void LocationBarView::OnChanged() {
  int icon_id = omnibox_view_->GetIcon();
  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon_id));
  location_icon_view_->ShowTooltip(!GetOmniboxView()->IsEditingOrEmpty());

  ToolbarModel* toolbar_model = GetToolbarModel();
  chrome::DisplaySearchButtonConditions conditions =
      chrome::GetDisplaySearchButtonConditions();
  bool meets_conditions =
      (conditions == chrome::DISPLAY_SEARCH_BUTTON_ALWAYS) ||
      ((conditions != chrome::DISPLAY_SEARCH_BUTTON_NEVER) &&
       (toolbar_model->WouldPerformSearchTermReplacement(true) ||
        ((conditions == chrome::DISPLAY_SEARCH_BUTTON_FOR_STR_OR_IIP) &&
         toolbar_model->input_in_progress())));
  search_button_->SetVisible(!is_popup_mode_ && meets_conditions);
  search_button_->UpdateIcon(icon_id == IDR_OMNIBOX_SEARCH);

  origin_chip_view_->OnChanged();

  Layout();
  SchedulePaint();
}

void LocationBarView::OnSetFocus() {
  GetFocusManager()->SetFocusedView(this);
}

InstantController* LocationBarView::GetInstant() {
  return delegate_->GetInstant();
}

const ToolbarModel* LocationBarView::GetToolbarModel() const {
  return delegate_->GetToolbarModel();
}

void LocationBarView::HideURL() {
  DCHECK(chrome::ShouldDisplayOriginChip());

  // If we're currently showing, reverse the hide by swapping to the hide
  // animation, offset so that the text is in the same position.
  if (show_url_animation_->is_animating()) {
    const double hide_value = GetValueForAnimation(true);
    ResetShowAnimationAndColors();
    hide_url_animation_->Show();
    // This must be done after calling Show() and is not equivalent to Reset(n);
    // see comments in ShowURL().
    hide_url_animation_->SetCurrentValue(hide_value);
  } else {
    hide_url_animation_->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private DropdownBarHostDelegate implementation:

void LocationBarView::SetFocusAndSelection(bool select_all) {
  FocusLocation(select_all);
}

void LocationBarView::SetAnimationOffset(int offset) {
  dropdown_animation_offset_ = offset;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private gfx::AnimationDelegate implementation:

void LocationBarView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK((animation == show_url_animation_.get()) ||
      (animation == hide_url_animation_.get()));
  Layout();
  SchedulePaint();
}

void LocationBarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == show_url_animation_.get()) {
    ResetShowAnimationAndColors();
    Layout();
    SchedulePaint();
  } else {
    DCHECK(animation == hide_url_animation_.get());
    hide_url_animation_->Reset();
    origin_chip_view_->FadeIn();
    omnibox_view_->HideURL();  // Calls OnChanged(), triggering layout.
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private TemplateURLServiceObserver implementation:

void LocationBarView::OnTemplateURLServiceChanged() {
  template_url_service_->RemoveObserver(this);
  template_url_service_ = NULL;
  // If the browser is no longer active, let's not show the info bubble, as this
  // would make the browser the active window again.
  if (omnibox_view_ && omnibox_view_->GetWidget()->IsActive())
    ShowFirstRunBubble();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private content::NotificationObserver implementation:

void LocationBarView::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED:
      Update(NULL);
      break;

    default:
      NOTREACHED() << "Unexpected notification.";
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private SearchModelObserver implementation:

void LocationBarView::ModelChanged(const SearchModel::State& old_state,
                                   const SearchModel::State& new_state) {
  const bool visible = !GetToolbarModel()->input_in_progress() &&
      new_state.voice_search_supported;
  if (mic_search_view_->visible() != visible) {
    mic_search_view_->SetVisible(visible);
    Layout();
  }
}
