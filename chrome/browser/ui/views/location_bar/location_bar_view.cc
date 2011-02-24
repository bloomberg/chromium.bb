// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/command_line.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_util.h"
#include "views/controls/label.h"
#include "views/drag_utils.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/location_bar/suggested_text_view.h"
#include "chrome/browser/ui/views/first_run_bubble.h"
#endif

using views::View;

namespace {
TabContents* GetTabContentsFromDelegate(LocationBarView::Delegate* delegate) {
  const TabContentsWrapper* wrapper = delegate->GetTabContentsWrapper();
  return wrapper ? wrapper->tab_contents() : NULL;
}
}  // namespace

// static
const int LocationBarView::kNormalHorizontalEdgeThickness = 1;
const int LocationBarView::kVerticalEdgeThickness = 2;
const int LocationBarView::kItemPadding = 3;
const int LocationBarView::kExtensionItemPadding = 5;
const int LocationBarView::kEdgeItemPadding = kItemPadding;
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

static const int kNormalModeBackgroundImages[] = {
  IDR_LOCATIONBG_L,
  IDR_LOCATIONBG_C,
  IDR_LOCATIONBG_R,
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
      transition_(PageTransition::LINK),
      location_icon_view_(NULL),
      ev_bubble_view_(NULL),
      location_entry_view_(NULL),
      selected_keyword_view_(NULL),
#if defined(OS_WIN)
      suggested_text_view_(NULL),
#endif
      keyword_hint_view_(NULL),
      star_view_(NULL),
      mode_(mode),
      show_focus_rect_(false),
      bubble_type_(FirstRun::MINIMAL_BUBBLE),
      template_url_model_(NULL),
      update_instant_(true) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (mode_ == NORMAL)
    painter_.reset(new views::HorizontalPainter(kNormalModeBackgroundImages));
}

LocationBarView::~LocationBarView() {
  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

void LocationBarView::Init() {
  if (mode_ == POPUP) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::MediumFont);
  }

  // If this makes the font too big, try to make it smaller so it will fit.
  const int height =
      std::max(GetPreferredSize().height() - (kVerticalEdgeThickness * 2), 0);
  while ((font_.GetHeight() > height) && (font_.GetFontSize() > 1))
    font_ = font_.DeriveFont(-1);

  location_icon_view_ = new LocationIconView(this);
  AddChildView(location_icon_view_);
  location_icon_view_->SetVisible(true);
  location_icon_view_->SetDragController(this);

  ev_bubble_view_ =
      new EVBubbleView(kEVBubbleBackgroundImages, IDR_OMNIBOX_HTTPS_VALID,
                       GetColor(ToolbarModel::EV_SECURE, SECURITY_TEXT), this);
  AddChildView(ev_bubble_view_);
  ev_bubble_view_->SetVisible(false);
  ev_bubble_view_->SetDragController(this);

  // URL edit field.
  // View container for URL edit field.
#if defined(OS_WIN)
  location_entry_.reset(new AutocompleteEditViewWin(font_, this, model_, this,
      GetWidget()->GetNativeView(), profile_, command_updater_,
      mode_ == POPUP, this));
#else
  location_entry_.reset(
      AutocompleteEditViewGtk::Create(
          this, model_, profile_,
          command_updater_, mode_ == POPUP, this));
#endif

  location_entry_view_ = location_entry_->AddToView(this);
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  location_entry_view_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION));

  selected_keyword_view_ = new SelectedKeywordView(
      kSelectedKeywordBackgroundImages, IDR_KEYWORD_SEARCH_MAGNIFIER,
      GetColor(ToolbarModel::NONE, TEXT), profile_),
  AddChildView(selected_keyword_view_);
  selected_keyword_view_->SetFont(font_);
  selected_keyword_view_->SetVisible(false);

  SkColor dimmed_text = GetColor(ToolbarModel::NONE, DEEMPHASIZED_TEXT);

  keyword_hint_view_ = new KeywordHintView(profile_);
  AddChildView(keyword_hint_view_);
  keyword_hint_view_->SetVisible(false);
  keyword_hint_view_->SetFont(font_);
  keyword_hint_view_->SetColor(dimmed_text);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageView* content_blocked_view = new ContentSettingImageView(
        static_cast<ContentSettingsType>(i), this, profile_);
    content_setting_views_.push_back(content_blocked_view);
    AddChildView(content_blocked_view);
    content_blocked_view->SetVisible(false);
  }

  // The star is not visible in popups and in the app launcher.
  if (browser_defaults::bookmarks_enabled && (mode_ == NORMAL)) {
    star_view_ = new StarView(command_updater_);
    AddChildView(star_view_);
    star_view_->SetVisible(true);
  }

  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION));

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

void LocationBarView::Update(const TabContents* tab_for_state_restoring) {
  bool star_enabled = star_view_ && !model_->input_in_progress();
  command_updater_->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, star_enabled);
  if (star_view_)
    star_view_->SetVisible(star_enabled);
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
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }

  Layout();
  SchedulePaint();
}

void LocationBarView::InvalidatePageActions() {
  size_t count_before = page_action_views_.size();
  DeletePageActionViews();
  if (page_action_views_.size() != count_before) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        Source<LocationBar>(this),
        NotificationService::NoDetails());
  }
}

void LocationBarView::OnFocus() {
  // Focus the location entry native view.
  location_entry_->SetFocus();
}

void LocationBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ != profile) {
    profile_ = profile;
    location_entry_->model()->SetProfile(profile);
    selected_keyword_view_->set_profile(profile);
    keyword_hint_view_->set_profile(profile);
    for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
         i != content_setting_views_.end(); ++i)
      (*i)->set_profile(profile);
  }
}

TabContentsWrapper* LocationBarView::GetTabContentsWrapper() const {
  return delegate_->GetTabContentsWrapper();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (mode_ != NORMAL)
    return;

  DCHECK(page_action);
  TabContents* contents = GetTabContentsFromDelegate(delegate_);

  RefreshPageActionViews();
  PageActionWithBadgeView* page_action_view =
      static_cast<PageActionWithBadgeView*>(GetPageActionView(page_action));
  DCHECK(page_action_view);
  if (!page_action_view)
    return;

  page_action_view->image_view()->set_preview_enabled(preview_enabled);
  page_action_view->UpdateVisibility(contents,
      GURL(WideToUTF8(model_->GetText())));
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
  gfx::Rect screen_bounds(star_view_->GetImageBounds());
  // Compensate for some built-in padding in the Star image.
  screen_bounds.Inset(1, 1, 1, 2);
  gfx::Point origin(screen_bounds.origin());
  views::View::ConvertPointToScreen(star_view_, &origin);
  screen_bounds.set_origin(origin);
  browser::ShowBookmarkBubbleView(GetWindow(), screen_bounds, star_view_,
                                  profile_, url, newly_bookmarked);
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

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0, GetThemeProvider()->GetBitmapNamed(mode_ == POPUP ?
      IDR_LOCATIONBG_POPUPMODE_CENTER : IDR_LOCATIONBG_C)->height());
}

void LocationBarView::Layout() {
  if (!location_entry_.get())
    return;

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
  const int kItemEditPadding =
      LocationBarView::kItemPadding - kEditInternalSpace;
  const int kEdgeEditPadding =
      LocationBarView::kEdgeItemPadding - kEditInternalSpace;
  const int kBubbleVerticalPadding = (mode_ == POPUP) ?
      -1 : kBubbleHorizontalPadding;

  // Start by reserving the padding at the right edge.
  int entry_width = width() - kEdgeThickness - kEdgeItemPadding;

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
    entry_width -= (kEdgeThickness + kEdgeItemPadding + location_icon_width +
        kItemEditPadding);
  }

  if (star_view_ && star_view_->IsVisible())
    entry_width -= star_view_->GetPreferredSize().width() + kItemPadding;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= ((*i)->GetPreferredSize().width() + kItemPadding);
  }
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= ((*i)->GetPreferredSize().width() + kItemPadding);
  }
  // The gap between the edit and whatever is to its right is shortened.
  entry_width += kEditInternalSpace;

  // Size the EV bubble.  We do this after taking the star/page actions/content
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

#if defined(OS_WIN)
  RECT formatting_rect;
  location_entry_->GetRect(&formatting_rect);
  RECT edit_bounds;
  location_entry_->GetClientRect(&edit_bounds);
  int max_edit_width = entry_width - formatting_rect.left -
                       (edit_bounds.right - formatting_rect.right);
#else
  int max_edit_width = entry_width;
#endif

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
          profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
      if (template_url && template_url->IsExtensionKeyword()) {
        const SkBitmap& bitmap = profile_->GetExtensionService()->
            GetOmniboxIcon(template_url->GetExtensionId());
        selected_keyword_view_->SetImage(bitmap);
        selected_keyword_view_->SetItemPadding(kExtensionItemPadding);
      } else {
        selected_keyword_view_->SetImage(*ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_OMNIBOX_SEARCH));
        selected_keyword_view_->SetItemPadding(kItemPadding);
      }
    }
  } else if (show_keyword_hint) {
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // Lay out items to the right of the edit field.
  int offset = width() - kEdgeThickness - kEdgeItemPadding;
  if (star_view_ && star_view_->IsVisible()) {
    int star_width = star_view_->GetPreferredSize().width();
    offset -= star_width;
    star_view_->SetBounds(offset, location_y, star_width, location_height);
    offset -= kItemPadding;
  }

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible()) {
      int page_action_width = (*i)->GetPreferredSize().width();
      offset -= page_action_width;
      (*i)->SetBounds(offset, location_y, page_action_width, location_height);
      offset -= kItemPadding;
    }
  }
  // We use a reverse_iterator here because we're laying out the views from
  // right to left but in the vector they're ordered left to right.
  for (ContentSettingViews::const_reverse_iterator
       i(content_setting_views_.rbegin()); i != content_setting_views_.rend();
       ++i) {
    if ((*i)->IsVisible()) {
      int content_blocked_width = (*i)->GetPreferredSize().width();
      offset -= content_blocked_width;
      (*i)->SetBounds(offset, location_y, content_blocked_width,
                      location_height);
      offset -= kItemPadding;
    }
  }

  // Now lay out items to the left of the edit field.
  if (location_icon_view_->IsVisible()) {
    location_icon_view_->SetBounds(kEdgeThickness + kEdgeItemPadding,
        location_y, location_icon_width, location_height);
    offset = location_icon_view_->bounds().right() + kItemEditPadding;
  } else if (ev_bubble_view_->IsVisible()) {
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
    location_bounds.set_x(selected_keyword_view_->IsVisible() ?
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
    if (!keyword_hint_view_->IsVisible()) {
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
    // TODO(sky): need to layout when the user changes caret position.
    int suggested_text_width = suggested_text_view_->GetPreferredSize().width();
    int vis_text_width = location_entry_->WidthOfTextAfterCursor();
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
  }
#endif

  location_entry_view_->SetBoundsRect(location_bounds);
}

void LocationBarView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (painter_.get()) {
    painter_->Paint(width(), height(), canvas);
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
    // The round corners of the omnibox match the round corners of the dropdown
    // below, and all our other bubbles.
    const SkScalar radius(SkIntToScalar(BubbleBorder::GetCornerRadius()));
    bounds.Inset(kNormalHorizontalEdgeThickness, 0);
    canvas->AsCanvasSkia()->drawRoundRect(gfx::RectToSkRect(bounds), radius,
                                          radius, paint);
  } else {
    canvas->FillRectInt(color, bounds.x(), bounds.y(), bounds.width(),
                        bounds.height());
  }

  if (show_focus_rect_ && HasFocus()) {
    gfx::Rect r = location_entry_view_->bounds();
#if defined(OS_WIN)
    canvas->DrawFocusRect(r.x() - 1, r.y() - 1, r.width() + 2, r.height() + 2);
#else
    canvas->DrawFocusRect(r.x() - 1, r.y(), r.width() + 2, r.height());
#endif
  }
}

void LocationBarView::SetShowFocusRect(bool show) {
  show_focus_rect_ = show;
  SchedulePaint();
}

void LocationBarView::SelectAll() {
  location_entry_->SelectAll(true);
}

#if defined(OS_WIN)
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

void LocationBarView::OnMouseReleased(const views::MouseEvent& event,
                                      bool canceled) {
  UINT msg;
  if (canceled) {
    msg = WM_CAPTURECHANGED;
  } else if (event.IsLeftMouseButton()) {
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
#endif

void LocationBarView::OnAutocompleteWillClosePopup() {
  if (!update_instant_)
    return;

  InstantController* instant = delegate_->GetInstant();
  if (instant && !instant->commit_on_mouse_up())
    instant->DestroyPreviewContents();
}

void LocationBarView::OnAutocompleteLosingFocus(
    gfx::NativeView view_gaining_focus) {
  SetSuggestedText(string16());

  InstantController* instant = delegate_->GetInstant();
  if (instant)
    instant->OnAutocompleteLostFocus(view_gaining_focus);
}

void LocationBarView::OnAutocompleteWillAccept() {
  update_instant_ = false;
}

bool LocationBarView::OnCommitSuggestedText(bool skip_inline_autocomplete) {
  if (!delegate_->GetInstant())
    return false;

  string16 suggestion;
#if defined(OS_WIN)
  if (HasValidSuggestText())
    suggestion = suggested_text_view_->GetText();
#else
  suggestion = location_entry_->GetInstantSuggestion();
#endif

  if (suggestion.empty())
    return false;

  location_entry_->model()->FinalizeInstantQuery(
      location_entry_->GetText(), suggestion, skip_inline_autocomplete);
  return true;
}

bool LocationBarView::AcceptCurrentInstantPreview() {
  return InstantController::CommitIfCurrent(delegate_->GetInstant());
}

void LocationBarView::OnPopupBoundsChanged(const gfx::Rect& bounds) {
  InstantController* instant = delegate_->GetInstant();
  if (instant)
    instant->SetOmniboxBounds(bounds);
}

void LocationBarView::OnAutocompleteAccept(
    const GURL& url,
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

  if (delegate_->GetInstant() &&
      !location_entry_->model()->popup_model()->IsOpen())
    delegate_->GetInstant()->DestroyPreviewContents();

  update_instant_ = true;
}

void LocationBarView::OnChanged() {
  location_icon_view_->SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          location_entry_->GetIcon()));
  location_icon_view_->ShowTooltip(!location_entry()->IsEditingOrEmpty());

  Layout();
  SchedulePaint();

  // TODO(sky): code for updating instant is nearly identical on all platforms.
  // It sould be pushed to a common place.
  InstantController* instant = delegate_->GetInstant();
  string16 suggested_text;
  if (update_instant_ && instant && GetTabContentsWrapper()) {
    if (location_entry_->model()->user_input_in_progress() &&
        location_entry_->model()->popup_model()->IsOpen()) {
      instant->Update(GetTabContentsWrapper(),
                      location_entry_->model()->CurrentMatch(),
                      location_entry_->GetText(),
                      location_entry_->model()->UseVerbatimInstant(),
                      &suggested_text);
      if (!instant->MightSupportInstant()) {
        location_entry_->model()->FinalizeInstantQuery(
            string16(), string16(), false);
      }
    } else {
      instant->DestroyPreviewContents();
      location_entry_->model()->FinalizeInstantQuery(
          string16(), string16(), false);
    }
  }

  SetSuggestedText(suggested_text);
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

SkBitmap LocationBarView::GetFavIcon() const {
  return GetTabContentsFromDelegate(delegate_)->GetFavIcon();
}

string16 LocationBarView::GetTitle() const {
  return GetTabContentsFromDelegate(delegate_)->GetTitle();
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
  if (view->IsVisible()) {
    view->SetBounds(
        leading ? bounds->x() : (bounds->right() - view_size.width()),
        view->y(), view_size.width(), view->height());
    bounds->set_width(bounds->width() - desired_width);
  }
}

void LocationBarView::RefreshContentSettingViews() {
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->UpdateFromTabContents(model_->input_in_progress() ? NULL :
                                GetTabContentsFromDelegate(delegate_));
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
    old_visibility[(*i)->image_view()->page_action()] = (*i)->IsVisible();

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  std::vector<ExtensionAction*> page_actions;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions.size() != page_action_views_.size()) {
    DeletePageActionViews();  // Delete the old views (if any).

    page_action_views_.resize(page_actions.size());

    // Add the page actions in reverse order, so that the child views are
    // inserted in left-to-right order for accessibility.
    for (int i = page_actions.size() - 1; i >= 0; --i) {
      page_action_views_[i] = new PageActionWithBadgeView(
          new PageActionImageView(this, profile_, page_actions[i]));
      page_action_views_[i]->SetVisible(false);
      AddChildViewAt(page_action_views_[i], GetIndexOf(star_view_));
    }
  }

  TabContents* contents = GetTabContentsFromDelegate(delegate_);
  if (!page_action_views_.empty() && contents) {
    GURL url = GURL(WideToUTF8(model_->GetText()));

    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(model_->input_in_progress() ? NULL : contents,
                             url);

      // Check if the visibility of the action changed and notify if it did.
      ExtensionAction* action = (*i)->image_view()->page_action();
      if (old_visibility.find(action) == old_visibility.end() ||
          old_visibility[action] != (*i)->IsVisible()) {
        NotificationService::current()->Notify(
            NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
            Source<ExtensionAction>(action),
            Details<TabContents>(contents));
      }
    }
  }
}

#if defined(OS_WIN)
void LocationBarView::OnMouseEvent(const views::MouseEvent& event, UINT msg) {
  UINT flags = 0;
  if (event.IsControlDown())
    flags |= MK_CONTROL;
  if (event.IsShiftDown())
    flags |= MK_SHIFT;
  if (event.IsLeftMouseButton())
    flags |= MK_LBUTTON;
  if (event.IsMiddleMouseButton())
    flags |= MK_MBUTTON;
  if (event.IsRightMouseButton())
    flags |= MK_RBUTTON;

  gfx::Point screen_point(event.location());
  ConvertPointToScreen(this, &screen_point);
  location_entry_->HandleExternalMsg(msg, flags, screen_point.ToPOINT());
}
#endif

void LocationBarView::ShowFirstRunBubbleInternal(
    FirstRun::BubbleType bubble_type) {
#if defined(OS_WIN)  // First run bubble doesn't make sense for Chrome OS.
  // If the browser is no longer active, let's not show the info bubble, as this
  // would make the browser the active window again.
  if (!location_entry_view_ || !location_entry_view_->GetWidget()->IsActive())
    return;

  // Point at the start of the edit control; adjust to look as good as possible.
  const int kXOffset = kNormalHorizontalEdgeThickness + kEdgeItemPadding +
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_OMNIBOX_HTTP)->width() + kItemPadding;
  const int kYOffset = -(kVerticalEdgeThickness + 2);
  gfx::Point origin(location_entry_view_->bounds().x() + kXOffset,
                    y() + height() + kYOffset);
  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (base::i18n::IsRTL())
    origin.set_x(width() - origin.x());
  views::View::ConvertPointToScreen(this, &origin);
  FirstRunBubble::Show(profile_, GetWidget(), gfx::Rect(origin, gfx::Size()),
                       BubbleBorder::TOP_LEFT, bubble_type);
#endif
}

std::string LocationBarView::GetClassName() const {
  return kViewClassName;
}

bool LocationBarView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
#if defined(OS_WIN)
  if (views::FocusManager::IsTabTraversalKeyEvent(e)) {
    if (HasValidSuggestText()) {
      // Return true so that the edit sees the tab and commits the suggestion.
      return true;
    }
    if (keyword_hint_view_->IsVisible() && !e.IsShiftDown()) {
      // Return true so the edit gets the tab event and enters keyword mode.
      return true;
    }

    // If the caret is not at the end, then tab moves the caret to the end.
    if (!location_entry_->IsCaretAtEnd())
      return true;

    // Tab while showing instant commits instant immediately.
    // Return true so that focus traversal isn't attempted. The edit ends
    // up doing nothing in this case.
    if (AcceptCurrentInstantPreview())
      return true;
  }

  return location_entry_->SkipDefaultKeyEventProcessing(e);
#else
  // This method is not used for Linux ports. See FocusManager::OnKeyEvent() in
  // src/views/focus/focus_manager.cc for details.
  return false;
#endif
}

AccessibilityTypes::Role LocationBarView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_GROUPING;
}

void LocationBarView::WriteDragData(views::View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) {
  DCHECK(GetDragOperations(sender, press_pt) != ui::DragDropTypes::DRAG_NONE);

  TabContents* tab_contents = GetTabContentsFromDelegate(delegate_);
  DCHECK(tab_contents);
  drag_utils::SetURLAndDragImage(tab_contents->GetURL(),
                                 UTF16ToWideHack(tab_contents->GetTitle()),
                                 tab_contents->GetFavIcon(), data);
}

int LocationBarView::GetDragOperations(views::View* sender,
                                       const gfx::Point& p) {
  DCHECK((sender == location_icon_view_) || (sender == ev_bubble_view_));
  TabContents* tab_contents = GetTabContentsFromDelegate(delegate_);
  return (tab_contents && tab_contents->GetURL().is_valid() &&
          !location_entry()->IsEditingOrEmpty()) ?
      (ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK) :
      ui::DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDrag(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {
  // Wait until search engines have loaded to show the first run bubble.
  if (!profile_->GetTemplateURLModel()->loaded()) {
    bubble_type_ = bubble_type;
    template_url_model_ = profile_->GetTemplateURLModel();
    template_url_model_->AddObserver(this);
    template_url_model_->Load();
    return;
  }
  ShowFirstRunBubbleInternal(bubble_type);
}

void LocationBarView::SetSuggestedText(const string16& input) {
  // This method is internally invoked to reset suggest text, so we only do
  // anything if the text isn't empty.
  // TODO: if we keep autocomplete, make it so this isn't invoked with empty
  // text.
  if (!input.empty()) {
    location_entry_->model()->FinalizeInstantQuery(location_entry_->GetText(),
                                                   input, false);
  }
}

std::wstring LocationBarView::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarView::GetPageTransition() const {
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

void LocationBarView::SaveStateToContents(TabContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarView::Revert() {
  location_entry_->RevertAll();
}

int LocationBarView::PageActionVisibleCount() {
  int result = 0;
  for (size_t i = 0; i < page_action_views_.size(); i++) {
    if (page_action_views_[i]->IsVisible())
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
    if (page_action_views_[i]->IsVisible()) {
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
    if (page_action_views_[i]->IsVisible()) {
      if (current == index) {
        const int kLeftMouseButton = 1;
        page_action_views_[i]->image_view()->ExecuteAction(kLeftMouseButton,
            false);  // inspect_with_devtools
        return;
      }
      ++current;
    }
  }

  NOTREACHED();
}

void LocationBarView::OnTemplateURLModelChanged() {
  template_url_model_->RemoveObserver(this);
  template_url_model_ = NULL;
  ShowFirstRunBubble(bubble_type_);
}

#if defined(OS_WIN)
bool LocationBarView::HasValidSuggestText() {
  return suggested_text_view_ && !suggested_text_view_->size().IsEmpty() &&
      !suggested_text_view_->GetText().empty();
}
#endif
