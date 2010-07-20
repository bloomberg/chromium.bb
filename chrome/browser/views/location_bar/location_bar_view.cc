// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar/location_bar_view.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "app/drag_drop_types.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/views/location_bar/ev_bubble_view.h"
#include "chrome/browser/views/location_bar/keyword_hint_view.h"
#include "chrome/browser/views/location_bar/location_icon_view.h"
#include "chrome/browser/views/location_bar/page_action_image_view.h"
#include "chrome/browser/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/views/location_bar/star_view.h"
#include "gfx/canvas_skia.h"
#include "gfx/color_utils.h"
#include "gfx/skia_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/drag_utils.h"

#if defined(OS_WIN)
#include "chrome/browser/views/first_run_bubble.h"
#endif

using views::View;

// static
const int LocationBarView::kVertMargin = 2;
const int LocationBarView::kEdgeThickness = 2;
const int LocationBarView::kItemPadding = 3;
const char LocationBarView::kViewClassName[] =
    "browser/views/location_bar/LocationBarView";

// Convenience: Total space at the edges of the bar.
const int kEdgePadding =
    LocationBarView::kEdgeThickness + LocationBarView::kItemPadding;

// Padding before the start of a bubble.
static const int kBubblePadding = kEdgePadding - 1;

// Padding between the location icon and the edit, if they're adjacent.
static const int kLocationIconEditPadding = LocationBarView::kItemPadding - 1;

// Padding after the star.
static const int kStarPadding = kEdgePadding + 1;

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
      location_icon_view_(NULL),
      ev_bubble_view_(NULL),
      location_entry_view_(NULL),
      selected_keyword_view_(NULL),
      keyword_hint_view_(NULL),
      star_view_(NULL),
      mode_(mode),
      show_focus_rect_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(first_run_bubble_(this)) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (mode_ == NORMAL)
    painter_.reset(new views::HorizontalPainter(kNormalModeBackgroundImages));
}

LocationBarView::~LocationBarView() {
}

void LocationBarView::Init() {
  if (mode_ == POPUP) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = font_.DeriveFont(3);
  }

  // If this makes the font too big, try to make it smaller so it will fit.
  const int height =
      std::max(GetPreferredSize().height() - TopMargin() - kVertMargin, 0);
  while ((font_.height() > height) && (font_.FontSize() > 1))
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
  location_entry_.reset(new AutocompleteEditViewGtk(this, model_, profile_,
      command_updater_, mode_ == POPUP, this));
  location_entry_->Init();
  // Make all the children of the widget visible. NOTE: this won't display
  // anything, it just toggles the visible flag.
  gtk_widget_show_all(location_entry_->GetNativeView());
  // Hide the widget. NativeViewHostGtk will make it visible again as
  // necessary.
  gtk_widget_hide(location_entry_->GetNativeView());

  // Associate an accessible name with the location entry.
  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      location_entry_->text_view(), profile_));
  accessible_widget_helper_->SetWidgetName(
      location_entry_->text_view(),
      l10n_util::GetStringUTF8(IDS_ACCNAME_LOCATION));
#endif
  location_entry_view_ = new views::NativeViewHost;
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->GetNativeView());
  location_entry_view_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_LOCATION));

  selected_keyword_view_ =
      new SelectedKeywordView(kSelectedKeywordBackgroundImages,
                              IDR_OMNIBOX_SEARCH, SK_ColorBLACK, profile_),
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
  if (mode_ == NORMAL) {
    star_view_ = new StarView(command_updater_);
    AddChildView(star_view_);
    star_view_->SetVisible(true);
  }

  // Notify us when any ancestor is resized.  In this case we want to tell the
  // AutocompleteEditView to close its popup.
  SetNotifyWhenVisibleBoundsInRootChanges(true);

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

void LocationBarView::Focus() {
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

TabContents* LocationBarView::GetTabContents() const {
  return delegate_->GetTabContents();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                                  bool preview_enabled) {
  if (mode_ != NORMAL)
    return;

  DCHECK(page_action);
  TabContents* contents = delegate_->GetTabContents();

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

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0, GetThemeProvider()->GetBitmapNamed(mode_ == POPUP ?
      IDR_LOCATIONBG_POPUPMODE_CENTER : IDR_LOCATIONBG_C)->height());
}

void LocationBarView::Layout() {
  if (!location_entry_.get())
    return;

  int entry_width = width() - (star_view_ ? kStarPadding : kEdgePadding);

  // |location_icon_view_| is visible except when |ev_bubble_view_| or
  // |selected_keyword_view_| are visible.
  int location_icon_width = 0;
  int ev_bubble_width = 0;
  location_icon_view_->SetVisible(false);
  ev_bubble_view_->SetVisible(false);
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  if (show_selected_keyword) {
    entry_width -= kEdgePadding;  // Assume the keyword might be hidden.
  } else if (model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    ev_bubble_view_->SetVisible(true);
    ev_bubble_view_->SetLabel(model_->GetEVCertName());
    ev_bubble_width = ev_bubble_view_->GetPreferredSize().width();
    // We'll adjust this width and take it out of |entry_width| below.
  } else {
    location_icon_view_->SetVisible(true);
    location_icon_width = location_icon_view_->GetPreferredSize().width();
    entry_width -=
        kEdgePadding + location_icon_width + kLocationIconEditPadding;
  }

  if (star_view_)
    entry_width -= star_view_->GetPreferredSize().width() + kItemPadding;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kItemPadding;
  }
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kItemPadding;
  }

  // Size the EV bubble.  We do this after taking the star/page actions/content
  // settings out of |entry_width| so we won't take too much space.
  if (ev_bubble_width) {
    // Try to elide the bubble to be no larger than half the total available
    // space, but never elide it any smaller than 150 px.
    static const int kMinElidedBubbleWidth = 150;
    static const double kMaxBubbleFraction = 0.5;
    ev_bubble_width = std::min(ev_bubble_width, std::max(kMinElidedBubbleWidth,
        static_cast<int>((entry_width - kBubblePadding - kItemPadding) *
        kMaxBubbleFraction)));

    entry_width -= kBubblePadding + ev_bubble_width + kItemPadding;
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
        const SkBitmap& bitmap = profile_->GetExtensionsService()->
            GetOmniboxIcon(template_url->GetExtensionId());
        selected_keyword_view_->SetImage(bitmap);
      } else {
        selected_keyword_view_->SetImage(*ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_OMNIBOX_SEARCH));
      }
    }
  } else if (show_keyword_hint) {
    if (keyword_hint_view_->keyword() != keyword)
      keyword_hint_view_->SetKeyword(keyword);
  }

  // TODO(sky): baseline layout.
  int location_y = TopMargin();
  int location_height = std::max(height() - location_y - kVertMargin, 0);

  // Lay out items to the right of the edit field.
  int offset = width();
  if (star_view_) {
    offset -= kStarPadding;
    int star_width = star_view_->GetPreferredSize().width();
    offset -= star_width;
    star_view_->SetBounds(offset, location_y, star_width, location_height);
    offset -= kItemPadding;
  } else {
    offset -= kEdgePadding;
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
    location_icon_view_->SetBounds(kEdgePadding, location_y,
                                   location_icon_width, location_height);
    offset = location_icon_view_->bounds().right() + kLocationIconEditPadding;
  } else if (ev_bubble_view_->IsVisible()) {
    ev_bubble_view_->SetBounds(kBubblePadding, location_y, ev_bubble_width,
                               location_height);
    offset = ev_bubble_view_->bounds().right() + kItemPadding;
  } else {
    offset = show_selected_keyword ? kBubblePadding : kEdgePadding;
  }

  // Now lay out the edit field and views that autocollapse to give it more
  // room.
  gfx::Rect location_bounds(offset, location_y, entry_width, location_height);
  if (show_selected_keyword) {
    LayoutView(true, selected_keyword_view_, available_width, &location_bounds);
    if (!selected_keyword_view_->IsVisible()) {
      location_bounds.set_x(
          location_bounds.x() + kEdgePadding - kBubblePadding);
    }
  } else if (show_keyword_hint) {
    LayoutView(false, keyword_hint_view_, available_width, &location_bounds);
  }

  location_entry_view_->SetBounds(location_bounds);
}

void LocationBarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

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
  // AutocompletePopupContentsView::Paint()).
  gfx::Rect bounds(GetLocalBounds(false));
  bounds.Inset(0, TopMargin(), 0, kVertMargin);
  SkColor color(GetColor(ToolbarModel::NONE, BACKGROUND));
  if (mode_ == NORMAL) {
    SkPaint paint;
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    // The round corners of the omnibox match the round corners of the dropdown
    // below, and all our other bubbles.
    const SkScalar radius(SkIntToScalar(BubbleBorder::GetCornerRadius()));
    bounds.Inset(kEdgeThickness, 0);
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

void LocationBarView::VisibleBoundsInRootChanged() {
  location_entry_->ClosePopup();
}

void LocationBarView::SetShowFocusRect(bool show) {
  show_focus_rect_ = show;
  SchedulePaint();
}

#if defined(OS_WIN)
bool LocationBarView::OnMousePressed(const views::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
  } else if (event.IsMiddleMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_MBUTTONDBLCLK : WM_MBUTTONDOWN;
  } else if (event.IsRightMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
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

void LocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (command_updater_) {
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
}

void LocationBarView::OnChanged() {
  location_icon_view_->SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          location_entry_->GetIcon()));
  Layout();
  SchedulePaint();
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
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return delegate_->GetTabContents()->GetFavIcon();
}

std::wstring LocationBarView::GetTitle() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return UTF16ToWideHack(delegate_->GetTabContents()->GetTitle());
}

int LocationBarView::TopMargin() const {
  return std::min(kVertMargin, height());
}

int LocationBarView::AvailableWidth(int location_bar_width) {
#if defined(OS_WIN)
  // Use font_.GetStringWidth() instead of
  // PosFromChar(location_entry_->GetTextLength()) because PosFromChar() is
  // apparently buggy. In both LTR UI and RTL UI with left-to-right layout,
  // PosFromChar(i) might return 0 when i is greater than 1.
  return std::max(
      location_bar_width - font_.GetStringWidth(location_entry_->GetText()), 0);
#else
  return location_bar_width - location_entry_->TextWidth();
#endif
}

bool LocationBarView::UsePref(int pref_width, int available_width) {
  return (pref_width + kItemPadding <= available_width);
}

void LocationBarView::LayoutView(bool leading,
                                 views::View* view,
                                 int available_width,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  gfx::Size view_size = view->GetPreferredSize();
  if (!UsePref(view_size.width(), available_width))
    view_size = view->GetMinimumSize();
  if (view_size.width() + kItemPadding >= bounds->width()) {
    view->SetVisible(false);
    return;
  }
  if (leading) {
    view->SetBounds(bounds->x(), bounds->y(), view_size.width(),
                    bounds->height());
    bounds->Offset(view_size.width() + kItemPadding, 0);
  } else {
    view->SetBounds(bounds->right() - view_size.width(), bounds->y(),
                    view_size.width(), bounds->height());
  }
  bounds->set_width(bounds->width() - view_size.width() - kItemPadding);
  view->SetVisible(true);
}

void LocationBarView::RefreshContentSettingViews() {
  const TabContents* tab_contents = delegate_->GetTabContents();
  for (ContentSettingViews::const_iterator i(content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->UpdateFromTabContents(
        model_->input_in_progress() ? NULL : tab_contents);
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

  ExtensionsService* service = profile_->GetExtensionsService();
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
      AddChildView(GetChildIndex(star_view_), page_action_views_[i]);
    }
  }

  TabContents* contents = delegate_->GetTabContents();
  if (!page_action_views_.empty() && contents) {
    GURL url = GURL(WideToUTF8(model_->GetText()));

    for (PageActionViews::const_iterator i(page_action_views_.begin());
         i != page_action_views_.end(); ++i) {
      (*i)->UpdateVisibility(contents, url);

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
  const int kXOffset = 6;   // Text looks like it actually starts 6 px in.
  const int kYOffset = -4;  // Point into the omnibox, not just at its edge.
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
  if (keyword_hint_view_->IsVisible() &&
      views::FocusManager::IsTabTraversalKeyEvent(e)) {
    // We want to receive tab key events when the hint is showing.
    return true;
  }

#if defined(OS_WIN)
  return location_entry_->SkipDefaultKeyEventProcessing(e);
#else
  // TODO(jcampan): We need to refactor the code of
  // AutocompleteEditViewWin::SkipDefaultKeyEventProcessing into this class so
  // it can be shared between Windows and Linux.
  // For now, we just override back-space and tab keys, as back-space is the
  // accelerator for back navigation and tab key is used by some input methods.
  if (e.GetKeyCode() == base::VKEY_BACK ||
      views::FocusManager::IsTabTraversalKeyEvent(e))
    return true;
  return false;
#endif
}

bool LocationBarView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}


void LocationBarView::WriteDragData(views::View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) {
  DCHECK(GetDragOperations(sender, press_pt) != DragDropTypes::DRAG_NONE);

  TabContents* tab_contents = delegate_->GetTabContents();
  DCHECK(tab_contents);
  drag_utils::SetURLAndDragImage(tab_contents->GetURL(),
                                 UTF16ToWideHack(tab_contents->GetTitle()),
                                 tab_contents->GetFavIcon(), data);
}

int LocationBarView::GetDragOperations(views::View* sender,
                                       const gfx::Point& p) {
  DCHECK((sender == location_icon_view_) || (sender == ev_bubble_view_));
  TabContents* tab_contents = delegate_->GetTabContents();
  return (tab_contents && tab_contents->GetURL().is_valid() &&
          !location_entry()->IsEditingOrEmpty()) ?
      (DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK) :
      DragDropTypes::DRAG_NONE;
}

bool LocationBarView::CanStartDrag(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {
  // We wait 30 milliseconds to open. It allows less flicker.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarView::ShowFirstRunBubbleInternal, bubble_type);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 30);
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
