// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>
#include <map>

#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/open_pdf_in_reader_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/toolbar/toolbar_model.h"
#include "components/translate/core/browser/language_state.h"
#include "components/ui/zoom/zoom_controller.h"
#include "components/ui/zoom/zoom_event_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/first_run_bubble.h"
#endif

using content::WebContents;
using views::View;

namespace {

// The border color for MD windows, as well as non-MD popup windows.
const SkColor kBorderColor = SkColorSetA(SK_ColorBLACK, 0x4D);

int GetEditLeadingInternalSpace() {
  // The textfield has 1 px of whitespace before the text in the RTL case only.
  return base::i18n::IsRTL() ? 1 : 0;
}

}  // namespace


// LocationBarView -----------------------------------------------------------

// static
const char LocationBarView::kViewClassName[] = "LocationBarView";

LocationBarView::LocationBarView(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater,
                                 Delegate* delegate,
                                 bool is_popup_mode)
    : LocationBar(profile),
      ChromeOmniboxEditController(command_updater),
      browser_(browser),
      omnibox_view_(nullptr),
      delegate_(delegate),
      location_icon_view_(nullptr),
      ime_inline_autocomplete_view_(nullptr),
      selected_keyword_view_(nullptr),
      suggested_text_view_(nullptr),
      keyword_hint_view_(nullptr),
      zoom_view_(nullptr),
      open_pdf_in_reader_view_(nullptr),
      manage_passwords_icon_view_(nullptr),
      save_credit_card_icon_view_(nullptr),
      translate_icon_view_(nullptr),
      star_view_(nullptr),
      size_animation_(this),
      is_popup_mode_(is_popup_mode),
      show_focus_rect_(false),
      template_url_service_(NULL),
      web_contents_null_at_last_refresh_(true) {
  edit_bookmarks_enabled_.Init(
      bookmarks::prefs::kEditBookmarksEnabled, profile->GetPrefs(),
      base::Bind(&LocationBarView::UpdateWithoutTabRestore,
                 base::Unretained(this)));

  ui_zoom::ZoomEventManager::GetForBrowserContext(profile)
      ->AddZoomEventManagerObserver(this);
}

LocationBarView::~LocationBarView() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);

  ui_zoom::ZoomEventManager::GetForBrowserContext(profile())
      ->RemoveZoomEventManagerObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public:

// static
SkColor LocationBarView::GetBorderColor(bool incognito) {
  return color_utils::AlphaBlend(
      SkColorSetA(kBorderColor, SK_AlphaOPAQUE),
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_TOOLBAR,
                                       incognito),
      SkColorGetA(kBorderColor));
}

void LocationBarView::Init() {
  // We need to be in a Widget, otherwise GetNativeTheme() may change and we're
  // not prepared for that.
  DCHECK(GetWidget());

  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Make sure children with layers are clipped. See http://crbug.com/589497
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetMasksToBounds(true);
  } else if (is_popup_mode_) {
    const int kOmniboxPopupBorderImages[] =
        IMAGE_GRID(IDR_OMNIBOX_POPUP_BORDER_AND_SHADOW);
    border_painter_.reset(
        views::Painter::CreateImageGridPainter(kOmniboxPopupBorderImages));
  } else {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::Insets omnibox_border_insets(14, 9);
    border_painter_.reset(views::Painter::CreateImagePainter(
        *rb.GetImageSkiaNamed(IDR_OMNIBOX_BORDER), omnibox_border_insets));
  }

  // Determine the main font.
  gfx::FontList font_list = ResourceBundle::GetSharedInstance().GetFontList(
      ResourceBundle::BaseFont);
  const int current_font_size = font_list.GetFontSize();
  const int desired_font_size = GetLayoutConstant(OMNIBOX_FONT_PIXEL_SIZE);
  if (current_font_size != desired_font_size) {
    font_list =
        font_list.DeriveWithSizeDelta(desired_font_size - current_font_size);
  }
  // Shrink large fonts to make them fit.
  // TODO(pkasting): Stretch the location bar instead in this case.
  const int vertical_padding = GetVerticalEdgeThicknessWithPadding();
  const int location_height =
      std::max(GetPreferredSize().height() - (vertical_padding * 2), 0);
  font_list = font_list.DeriveWithHeightUpperBound(location_height);

  // Determine the font for use inside the bubbles.
  const int bubble_padding =
      GetLayoutConstant(LOCATION_BAR_BUBBLE_VERTICAL_PADDING) +
      GetLayoutConstant(LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING);
  const int bubble_height = location_height - (bubble_padding * 2);
  gfx::FontList bubble_font_list =
      font_list.DeriveWithHeightUpperBound(bubble_height);

  const SkColor background_color = GetColor(BACKGROUND);
  location_icon_view_ =
      new LocationIconView(bubble_font_list, background_color, this);
  location_icon_view_->set_drag_controller(this);
  AddChildView(location_icon_view_);

  // Initialize the Omnibox view.
  omnibox_view_ = new OmniboxViewViews(
      this, profile(), command_updater(), is_popup_mode_, this, font_list);
  omnibox_view_->Init();
  omnibox_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
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

  const SkColor selected_text_color = GetColor(TEXT);
  selected_keyword_view_ = new SelectedKeywordView(
      bubble_font_list, selected_text_color, background_color, profile());
  AddChildView(selected_keyword_view_);

  suggested_text_view_ = new views::Label(base::string16(), font_list);
  suggested_text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  suggested_text_view_->SetAutoColorReadabilityEnabled(false);
  suggested_text_view_->SetEnabledColor(
      GetColor(LocationBarView::DEEMPHASIZED_TEXT));
  suggested_text_view_->SetVisible(false);
  AddChildView(suggested_text_view_);

  keyword_hint_view_ = new KeywordHintView(
      profile(), font_list, bubble_font_list, location_height,
      GetColor(LocationBarView::DEEMPHASIZED_TEXT), background_color);
  AddChildView(keyword_hint_view_);

  ScopedVector<ContentSettingImageModel> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (ContentSettingImageModel* model : models.get()) {
    // ContentSettingImageView takes ownership of its model.
    ContentSettingImageView* image_view = new ContentSettingImageView(
        model, this, bubble_font_list, background_color);
    content_setting_views_.push_back(image_view);
    image_view->SetVisible(false);
    AddChildView(image_view);
  }
  models.weak_clear();

  zoom_view_ = new ZoomView(delegate_);
  AddChildView(zoom_view_);

  open_pdf_in_reader_view_ = new OpenPDFInReaderView();
  AddChildView(open_pdf_in_reader_view_);

  manage_passwords_icon_view_ = new ManagePasswordsIconViews(command_updater());
  AddChildView(manage_passwords_icon_view_);

  save_credit_card_icon_view_ =
      new autofill::SaveCardIconView(command_updater(), browser_);
  save_credit_card_icon_view_->SetVisible(false);
  AddChildView(save_credit_card_icon_view_);

  translate_icon_view_ = new TranslateIconView(command_updater());
  translate_icon_view_->SetVisible(false);
  AddChildView(translate_icon_view_);

  star_view_ = new StarView(command_updater(), browser_);
  star_view_->SetVisible(false);
  AddChildView(star_view_);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(nullptr);

  size_animation_.Reset(1);
}

bool LocationBarView::IsInitialized() const {
  return omnibox_view_ != nullptr;
}

SkColor LocationBarView::GetColor(
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
      return color_utils::AlphaBlend(GetColor(TEXT), GetColor(BACKGROUND), 128);

    case EV_BUBBLE_TEXT_AND_BORDER:
      return ui::MaterialDesignController::IsModeMaterial()
                 ? gfx::kGoogleGreen700
                 : SkColorSetRGB(7, 149, 0);
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

SkColor LocationBarView::GetSecureTextColor(
    security_state::SecurityStateModel::SecurityLevel security_level) const {
  bool inverted = color_utils::IsDark(GetColor(BACKGROUND));
  SkColor color;
  switch (security_level) {
    case security_state::SecurityStateModel::EV_SECURE:
    case security_state::SecurityStateModel::SECURE:
      if (ui::MaterialDesignController::IsModeMaterial() && inverted)
        return GetColor(TEXT);
      color = GetColor(EV_BUBBLE_TEXT_AND_BORDER);
      break;

    case security_state::SecurityStateModel::SECURITY_POLICY_WARNING:
      return GetColor(DEEMPHASIZED_TEXT);
      break;

    case security_state::SecurityStateModel::SECURITY_ERROR: {
      bool md = ui::MaterialDesignController::IsModeMaterial();
      if (md && inverted)
        return GetColor(TEXT);
      color = md ? gfx::kGoogleRed700 : SkColorSetRGB(162, 0, 0);
      break;
    }

    case security_state::SecurityStateModel::SECURITY_WARNING:
      return GetColor(TEXT);
      break;

    default:
      NOTREACHED();
      return gfx::kPlaceholderColor;
  }
  return color_utils::GetReadableColor(color, GetColor(BACKGROUND));
}

void LocationBarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  DCHECK(zoom_view_);
  if (RefreshZoomView()) {
    Layout();
    SchedulePaint();
  }

  WebContents* web_contents = GetWebContents();
  if (can_show_bubble && zoom_view_->visible() && web_contents)
    ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
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
  return nullptr;
}

void LocationBarView::SetStarToggled(bool on) {
  if (star_view_)
    star_view_->SetToggled(on);
}

gfx::Point LocationBarView::GetOmniboxViewOrigin() const {
  gfx::Point origin(omnibox_view_->bounds().origin());
  origin.set_x(GetMirroredXInView(origin.x()));
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
  const views::ImageView* image = location_icon_view_->GetImageView();
  const gfx::Rect image_bounds(image->GetImageBounds());
  gfx::Point point(image_bounds.CenterPoint().x(), image_bounds.bottom());
  ConvertPointToTarget(image, this, &point);
  return point;
}

void LocationBarView::GetOmniboxPopupPositioningInfo(
    gfx::Point* top_left_screen_coord,
    int* popup_width,
    int* left_margin,
    int* right_margin,
    int top_edge_overlap) {
  *top_left_screen_coord = gfx::Point(0, parent()->height() - top_edge_overlap);
  views::View::ConvertPointToScreen(parent(), top_left_screen_coord);

  *popup_width = parent()->width();
  gfx::Rect location_bar_bounds(bounds());
  location_bar_bounds.Inset(GetHorizontalEdgeThickness(), 0);
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
  gfx::Size min_size;
  if (ui::MaterialDesignController::IsModeMaterial() || is_popup_mode_) {
    min_size.set_height(GetLayoutConstant(LOCATION_BAR_HEIGHT));
  } else {
    min_size = border_painter_->GetMinimumSize();
  }

  if (!IsInitialized())
    return min_size;

  min_size.set_height(min_size.height() * size_animation_.GetCurrentValue());

  const int padding = GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);

  // Compute width of omnibox-leading content.
  const int edge_thickness = GetHorizontalEdgeThickness();
  int leading_width = edge_thickness;
  if (ShouldShowKeywordBubble()) {
    // The selected keyword view can collapse completely.
  } else if (ShouldShowEVBubble()) {
    leading_width += GetLayoutConstant(LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING) +
                     location_icon_view_->GetMinimumSizeForLabelText(
                                            GetToolbarModel()->GetEVCertName())
                         .width();
  } else {
    leading_width += padding + location_icon_view_->GetMinimumSize().width();
  }

  // Compute width of omnibox-trailing content.
  int trailing_width = edge_thickness;
  trailing_width += IncrementalMinimumWidth(star_view_) +
                    IncrementalMinimumWidth(translate_icon_view_) +
                    IncrementalMinimumWidth(open_pdf_in_reader_view_) +
                    IncrementalMinimumWidth(save_credit_card_icon_view_) +
                    IncrementalMinimumWidth(manage_passwords_icon_view_) +
                    IncrementalMinimumWidth(zoom_view_);
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    trailing_width += IncrementalMinimumWidth((*i));
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i)
    trailing_width += IncrementalMinimumWidth((*i));

  min_size.set_width(leading_width + omnibox_view_->GetMinimumSize().width() +
                     2 * padding - omnibox_view_->GetInsets().width() +
                     trailing_width);
  return min_size;
}

void LocationBarView::Layout() {
  if (!IsInitialized())
    return;

  selected_keyword_view_->SetVisible(false);
  location_icon_view_->SetVisible(false);
  keyword_hint_view_->SetVisible(false);

  const int item_padding = GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);
  const int edge_thickness = GetHorizontalEdgeThickness();
  int trailing_edge_item_padding = 0;
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    trailing_edge_item_padding =
        item_padding - edge_thickness - omnibox_view_->GetInsets().right();
  }

  LocationBarLayout leading_decorations(
      LocationBarLayout::LEFT_EDGE, item_padding,
      item_padding - omnibox_view_->GetInsets().left() -
          GetEditLeadingInternalSpace());
  LocationBarLayout trailing_decorations(
      LocationBarLayout::RIGHT_EDGE, item_padding, trailing_edge_item_padding);

  const base::string16 keyword(omnibox_view_->model()->keyword());
  // In some cases (e.g. fullscreen mode) we may have 0 height.  We still want
  // to position our child views in this case, because other things may be
  // positioned relative to them (e.g. the "bookmark added" bubble if the user
  // hits ctrl-d).
  const int bubble_horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING);
  const int vertical_padding = GetVerticalEdgeThicknessWithPadding();
  const int location_height = std::max(height() - (vertical_padding * 2), 0);

  location_icon_view_->SetLabel(base::string16());
  location_icon_view_->SetBackground(false);
  if (ShouldShowKeywordBubble()) {
    leading_decorations.AddDecoration(vertical_padding, location_height, true,
                                      0, bubble_horizontal_padding,
                                      item_padding, selected_keyword_view_);
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
        selected_keyword_view_->ResetImage();
        selected_keyword_view_->set_is_extension_icon(false);
      }
    }
  } else if (ShouldShowEVBubble()) {
    location_icon_view_->SetLabel(GetToolbarModel()->GetEVCertName());
    location_icon_view_->SetBackground(true);
    // The largest fraction of the omnibox that can be taken by the EV bubble.
    const double kMaxBubbleFraction = 0.5;
    leading_decorations.AddDecoration(
        vertical_padding, location_height, false, kMaxBubbleFraction,
        bubble_horizontal_padding, item_padding, location_icon_view_);
  } else {
    leading_decorations.AddDecoration(vertical_padding, location_height,
                                      location_icon_view_);
  }

  if (star_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       star_view_);
  }
  if (translate_icon_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       translate_icon_view_);
  }
  if (open_pdf_in_reader_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       open_pdf_in_reader_view_);
  }
  if (save_credit_card_icon_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       save_credit_card_icon_view_);
  }
  if (manage_passwords_icon_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       manage_passwords_icon_view_);
  }
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(vertical_padding, location_height,
                                         (*i));
    }
  }
  if (zoom_view_->visible()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height,
                                       zoom_view_);
  }
  for (ContentSettingViews::const_reverse_iterator i(
           content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->visible()) {
      trailing_decorations.AddDecoration(vertical_padding, location_height,
                                         false, 0, item_padding, item_padding,
                                         *i);
    }
  }
  // Because IMEs may eat the tab key, we don't show "press tab to search" while
  // IME composition is in progress.
  if (!keyword.empty() && omnibox_view_->model()->is_keyword_hint() &&
      !omnibox_view_->IsImeComposing()) {
    trailing_decorations.AddDecoration(vertical_padding, location_height, true,
                                       0, item_padding, item_padding,
                                       keyword_hint_view_);
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // Perform layout.
  int full_width = width() - (2 * edge_thickness);

  int entry_width = full_width;
  leading_decorations.LayoutPass1(&entry_width);
  trailing_decorations.LayoutPass1(&entry_width);
  leading_decorations.LayoutPass2(&entry_width);
  trailing_decorations.LayoutPass2(&entry_width);

  int location_needed_width = omnibox_view_->GetTextWidth();
  int available_width = entry_width - location_needed_width;
  // The bounds must be wide enough for all the decorations to fit.
  gfx::Rect location_bounds(edge_thickness, vertical_padding,
                            std::max(full_width, full_width - entry_width),
                            location_height);
  leading_decorations.LayoutPass3(&location_bounds, &available_width);
  trailing_decorations.LayoutPass3(&location_bounds, &available_width);

  // Lay out the suggested text view right-aligned to the location entry. Only
  // show the suggested text if we can fit the text from one character before
  // the end of the selection to the end of the text and the suggested text. If
  // we can't it means either the suggested text is too big, or the user has
  // scrolled.

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

  omnibox_view_->SetBorder(
      views::Border::CreateEmptyBorder(0, 0, 0, omnibox_view_margin));

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
}

void LocationBarView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    RefreshLocationIcon();
    if (!is_popup_mode_) {
      set_background(new BackgroundWith1PxBorder(GetColor(BACKGROUND),
                                                 kBorderColor));
    }
  }
}

void LocationBarView::Update(const WebContents* contents) {
  RefreshContentSettingViews();
  RefreshZoomView();
  RefreshPageActionViews();
  RefreshTranslateIcon();
  RefreshSaveCreditCardIconView();
  RefreshManagePasswordsIconView();
  content::WebContents* web_contents_for_sub_views =
      GetToolbarModel()->input_in_progress() ? nullptr : GetWebContents();
  open_pdf_in_reader_view_->Update(web_contents_for_sub_views);

  if (star_view_)
    UpdateBookmarkStarVisibility();

  if (contents)
    omnibox_view_->OnTabChanged(contents);
  else
    omnibox_view_->Update();

  OnChanged();  // NOTE: Calls Layout().
}

void LocationBarView::ResetTabState(WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, public OmniboxEditController implementation:

void LocationBarView::UpdateWithoutTabRestore() {
  Update(nullptr);
}

void LocationBarView::ShowURL() {
  omnibox_view_->ShowURL();
}

ToolbarModel* LocationBarView::GetToolbarModel() {
  return delegate_->GetToolbarModel();
}

WebContents* LocationBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private:

int LocationBarView::IncrementalMinimumWidth(views::View* view) const {
  return view->visible() ?
      (GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING) +
          view->GetMinimumSize().width()) : 0;
}

int LocationBarView::GetHorizontalEdgeThickness() const {
  return is_popup_mode_ ? 0 : GetLayoutConstant(LOCATION_BAR_BORDER_THICKNESS);
}

int LocationBarView::GetVerticalEdgeThickness() const {
  if (ui::MaterialDesignController::IsModeMaterial())
    return GetLayoutConstant(LOCATION_BAR_BORDER_THICKNESS);
  return is_popup_mode_ ? views::NonClientFrameView::kClientEdgeThickness
                        : GetLayoutConstant(LOCATION_BAR_BORDER_THICKNESS);
}

int LocationBarView::GetVerticalEdgeThicknessWithPadding() const {
  return GetVerticalEdgeThickness() +
         GetLayoutConstant(LOCATION_BAR_VERTICAL_PADDING);
}

void LocationBarView::RefreshLocationIcon() {
  // |omnibox_view_| may not be ready yet if Init() has not been called. The
  // icon will be set soon by OnChanged().
  if (!omnibox_view_)
    return;

  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::VectorIconId icon_id = gfx::VectorIconId::VECTOR_ICON_NONE;
    const int kIconSize = 16;
    SkColor icon_color = gfx::kPlaceholderColor;
    if (ShouldShowEVBubble()) {
      icon_id = gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID_IN_CHIP;
      icon_color = location_icon_view_->GetTextColor();
    } else {
      icon_id = omnibox_view_->GetVectorIcon(
          color_utils::IsDark(GetColor(BACKGROUND)));
      icon_color = color_utils::DeriveDefaultIconColor(GetColor(TEXT));
    }
    location_icon_view_->SetImage(
        gfx::CreateVectorIcon(icon_id, kIconSize, icon_color));
  } else {
    location_icon_view_->SetImage(
        *GetThemeProvider()->GetImageSkiaNamed(omnibox_view_->GetIcon()));
  }
}

bool LocationBarView::RefreshContentSettingViews() {
  bool visibility_changed = false;
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    const bool was_visible = (*i)->visible();
    (*i)->Update(GetToolbarModel()->input_in_progress() ? nullptr
                                                        : GetWebContents());
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
  PageActions new_page_actions;

  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents);
    extensions::LocationBarController* controller =
        extensions_tab_helper->location_bar_controller();
    new_page_actions = controller->GetCurrentActions();
  }
  web_contents_null_at_last_refresh_ = web_contents == nullptr;

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (PageActionsDiffer(new_page_actions)) {
    changed = true;

    DeletePageActionViews();

    // Create the page action views.
    for (PageActions::const_iterator i = new_page_actions.begin();
         i != new_page_actions.end(); ++i) {
      PageActionWithBadgeView* page_action_view = new PageActionWithBadgeView(
          delegate_->CreatePageActionImageView(this, *i));
      page_action_view->SetVisible(false);
      page_action_views_.push_back(page_action_view);
    }

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

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    bool old_visibility = (*i)->visible();
    (*i)->UpdateVisibility(
        GetToolbarModel()->input_in_progress() ? nullptr : web_contents);
    changed |= old_visibility != (*i)->visible();
  }
  return changed;
}

bool LocationBarView::PageActionsDiffer(
    const PageActions& page_actions) const {
  if (page_action_views_.size() != page_actions.size())
    return true;

  for (size_t index = 0; index < page_actions.size(); ++index) {
    PageActionWithBadgeView* view = page_action_views_[index];
    if (view->image_view()->extension_action() != page_actions[index])
      return true;
  }

  return false;
}

bool LocationBarView::RefreshZoomView() {
  DCHECK(zoom_view_);
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;
  const bool was_visible = zoom_view_->visible();
  zoom_view_->Update(ui_zoom::ZoomController::FromWebContents(web_contents));
  if (!zoom_view_->visible())
    ZoomBubbleView::CloseCurrentBubble();
  return was_visible != zoom_view_->visible();
}

void LocationBarView::OnDefaultZoomLevelChanged() {
  RefreshZoomView();
}

bool LocationBarView::RefreshSaveCreditCardIconView() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  const bool was_visible = save_credit_card_icon_view_->visible();
  // |controller| may be nullptr due to lazy initialization.
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  bool enabled = controller && controller->IsIconVisible();
  command_updater()->UpdateCommandEnabled(IDC_SAVE_CREDIT_CARD_FOR_PAGE,
                                          enabled);
  save_credit_card_icon_view_->SetVisible(enabled);

  return was_visible != save_credit_card_icon_view_->visible();
}

void LocationBarView::RefreshTranslateIcon() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  translate::LanguageState& language_state =
      ChromeTranslateClient::FromWebContents(web_contents)->GetLanguageState();
  bool enabled = language_state.translate_enabled();
  command_updater()->UpdateCommandEnabled(IDC_TRANSLATE_PAGE, enabled);
  translate_icon_view_->SetVisible(enabled);
  if (!enabled)
    TranslateBubbleView::CloseCurrentBubble();
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
  return (GetToolbarModel()->GetSecurityLevel(false) ==
          security_state::SecurityStateModel::EV_SECURE);
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

ui::PageTransition LocationBarView::GetPageTransition() const {
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

void LocationBarView::UpdateSaveCreditCardIcon() {
  if (RefreshSaveCreditCardIconView()) {
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdatePageActions() {
  if (RefreshPageActionViews()) {  // Changed.
    Layout();
    SchedulePaint();
  }
}

void LocationBarView::UpdateBookmarkStarVisibility() {
  if (star_view_) {
    star_view_->SetVisible(
        browser_defaults::bookmarks_enabled && !is_popup_mode_ &&
        !GetToolbarModel()->input_in_progress() &&
        edit_bookmarks_enabled_.GetValue() &&
        !IsBookmarkStarHiddenByExtension());
  }
}

void LocationBarView::UpdateLocationBarVisibility(bool visible, bool animate) {
  if (!animate) {
    size_animation_.Reset(visible ? 1 : 0);
    return;
  }

  if (visible)
    size_animation_.Show();
  else
    size_animation_.Hide();
}

bool LocationBarView::ShowPageActionPopup(
    const extensions::Extension* extension,
    bool grant_tab_permissions) {
  ExtensionAction* extension_action =
      extensions::ExtensionActionManager::Get(profile())->GetPageAction(
          *extension);
  CHECK(extension_action);
  PageActionWithBadgeView* page_action_view =
      GetPageActionView(extension_action);
  if (!page_action_view) {
    CHECK(!web_contents_null_at_last_refresh_);
    CHECK(!is_popup_mode_);
    CHECK(!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled());
    CHECK(false);
  }
  PageActionImageView* page_action_image_view = page_action_view->image_view();
  CHECK(page_action_image_view);
  ExtensionActionViewController* extension_action_view_controller =
      page_action_image_view->view_controller();
  CHECK(extension_action_view_controller);
  return extension_action_view_controller->ExecuteAction(grant_tab_permissions);
}

void LocationBarView::UpdateOpenPDFInReaderPrompt() {
  open_pdf_in_reader_view_->Update(
      GetToolbarModel()->input_in_progress() ? nullptr : GetWebContents());
  Layout();
  SchedulePaint();
}

void LocationBarView::SaveStateToContents(WebContents* contents) {
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
  return nullptr;
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
  return nullptr;
}

void LocationBarView::TestPageActionPressed(size_t index) {
  size_t current = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->visible()) {
      if (current == index) {
        page_action_views_[i]->image_view()->view_controller()->
            ExecuteAction(true);
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

  if (ui::MaterialDesignController::IsModeMaterial() && !is_popup_mode_)
    return;  // The background and border are painted by our Background.

  // Fill the location bar background color behind the border.  Parts of the
  // border images are meant to rest atop the toolbar background and parts atop
  // the omnibox background, so we can't just blindly fill our entire bounds.
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(GetHorizontalEdgeThickness(),
               is_popup_mode_ ? 0 : GetVerticalEdgeThickness());
  SkColor background_color(GetColor(BACKGROUND));
  if (!is_popup_mode_) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(background_color);
    const int kBorderCornerRadius = 2;
    canvas->DrawRoundRect(bounds, kBorderCornerRadius, paint);
    // The border itself will be drawn in PaintChildren() since it includes an
    // inner shadow which should be drawn over the contents.
    return;
  }

  canvas->FillRect(bounds, background_color);
  const SkColor border_color = GetBorderColor(profile()->IsOffTheRecord());
  BrowserView::Paint1pxHorizontalLine(canvas, border_color, bounds, false);
  BrowserView::Paint1pxHorizontalLine(canvas, border_color, bounds, true);
}

void LocationBarView::PaintChildren(const ui::PaintContext& context) {
  View::PaintChildren(context);

  ui::PaintRecorder recorder(context, size());

  // For non-InstantExtendedAPI cases, if necessary, show focus rect. As we need
  // the focus rect to appear on top of children we paint here rather than
  // OnPaint().
  // Note: |Canvas::DrawFocusRect| paints a dashed rect with gray color.
  if (show_focus_rect_ && HasFocus())
    recorder.canvas()->DrawFocusRect(omnibox_view_->bounds());

  if (!ui::MaterialDesignController::IsModeMaterial() && !is_popup_mode_) {
    views::Painter::PaintPainterAt(recorder.canvas(), border_painter_.get(),
                                   GetContentsBounds());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private views::DragController implementation:

void LocationBarView::WriteDragDataForView(views::View* sender,
                                           const gfx::Point& press_pt,
                                           OSExchangeData* data) {
  DCHECK_NE(GetDragOperationsForView(sender, press_pt),
            ui::DragDropTypes::DRAG_NONE);

  WebContents* web_contents = GetWebContents();
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_driver->GetFavicon().AsImageSkia();
  button_drag_utils::SetURLAndDragImage(web_contents->GetURL(),
                                        web_contents->GetTitle(), favicon,
                                        nullptr, data, sender->GetWidget());
}

int LocationBarView::GetDragOperationsForView(views::View* sender,
                                              const gfx::Point& p) {
  DCHECK_EQ(location_icon_view_, sender);
  WebContents* web_contents = delegate_->GetWebContents();
  return (web_contents && web_contents->GetURL().is_valid() &&
          (!GetOmniboxView()->IsEditingOrEmpty())) ?
      (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK) :
      ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDragForView(View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private gfx::AnimationDelegate implementation:
void LocationBarView::AnimationProgressed(const gfx::Animation* animation) {
  GetWidget()->non_client_view()->Layout();
}

void LocationBarView::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private OmniboxEditController implementation:

void LocationBarView::OnChanged() {
  RefreshLocationIcon();
  location_icon_view_->set_show_tooltip(!GetOmniboxView()->IsEditingOrEmpty());
  Layout();
  SchedulePaint();
}

void LocationBarView::OnSetFocus() {
  GetFocusManager()->SetFocusedView(this);
}

const ToolbarModel* LocationBarView::GetToolbarModel() const {
  return delegate_->GetToolbarModel();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private DropdownBarHostDelegate implementation:

void LocationBarView::SetFocusAndSelection(bool select_all) {
  FocusLocation(select_all);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, private TemplateURLServiceObserver implementation:

void LocationBarView::OnTemplateURLServiceChanged() {
  template_url_service_->RemoveObserver(this);
  template_url_service_ = nullptr;
  // If the browser is no longer active, let's not show the info bubble, as this
  // would make the browser the active window again.
  if (omnibox_view_ && omnibox_view_->GetWidget()->IsActive())
    ShowFirstRunBubble();
}
