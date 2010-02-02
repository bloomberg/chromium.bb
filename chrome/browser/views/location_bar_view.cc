// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar_view.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/stl_util-inl.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/content_blocked_bubble_contents.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "chrome/browser/views/first_run_bubble.h"
#endif

using views::View;

// static
const int LocationBarView::kVertMargin = 2;

// Padding on the right and left of the entry field.
static const int kEntryPadding = 3;

// Padding between the entry and the leading/trailing views.
static const int kInnerPadding = 3;

static const SkBitmap* kBackground = NULL;

static const SkBitmap* kPopupBackground = NULL;

// The delay the mouse has to be hovering over the lock/warning icon before the
// info bubble is shown.
static const int kInfoBubbleHoverDelayMs = 500;

// The tab key image.
static const SkBitmap* kTabButtonBitmap = NULL;

// Returns the short name for a keyword.
static std::wstring GetKeywordName(Profile* profile,
                                   const std::wstring& keyword) {
  // Make sure the TemplateURL still exists.
  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url)
    return template_url->AdjustedShortNameForLocaleDirection();
  return std::wstring();
}


// PageActionWithBadgeView ----------------------------------------------------

// A container for the PageActionImageView plus its badge.
class LocationBarView::PageActionWithBadgeView : public views::View {
 public:
  explicit PageActionWithBadgeView(PageActionImageView* image_view);

  PageActionImageView* image_view() { return image_view_; }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(Extension::kPageActionIconMaxSize,
                     Extension::kPageActionIconMaxSize);
  }

  void UpdateVisibility(TabContents* contents, const GURL& url);

 private:
  virtual void Layout();

  // The button this view contains.
  PageActionImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(PageActionWithBadgeView);
};

LocationBarView::PageActionWithBadgeView::PageActionWithBadgeView(
    PageActionImageView* image_view) {
  image_view_ = image_view;
  AddChildView(image_view_);
}

void LocationBarView::PageActionWithBadgeView::Layout() {
  // We have 25 pixels of vertical space in the Omnibox to play with, so even
  // sized icons (such as 16x16) have either a 5 or a 4 pixel whitespace
  // (padding) above and below. It looks better to have the extra pixel above
  // the icon than below it, so we add a pixel. http://crbug.com/25708.
  const SkBitmap& image = image_view()->GetImage();
  int y = (image.height() + 1) % 2;  // Even numbers: 1px padding. Odd: 0px.
  image_view_->SetBounds(0, y, width(), height());
}

void LocationBarView::PageActionWithBadgeView::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  image_view_->UpdateVisibility(contents, url);
  SetVisible(image_view_->IsVisible());
}


// LocationBarView -----------------------------------------------------------

LocationBarView::LocationBarView(Profile* profile,
                                 CommandUpdater* command_updater,
                                 ToolbarModel* model,
                                 Delegate* delegate,
                                 bool popup_window_mode,
                                 const BubblePositioner* bubble_positioner)
    : profile_(profile),
      command_updater_(command_updater),
      model_(model),
      delegate_(delegate),
      disposition_(CURRENT_TAB),
      location_entry_view_(NULL),
      selected_keyword_view_(profile),
      keyword_hint_view_(profile),
      type_to_search_view_(l10n_util::GetString(IDS_OMNIBOX_EMPTY_TEXT)),
      security_image_view_(profile, model, bubble_positioner),
      popup_window_mode_(popup_window_mode),
      first_run_bubble_(this),
      bubble_positioner_(bubble_positioner) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (!kBackground) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    kBackground = rb.GetBitmapNamed(IDR_LOCATIONBG);
    kPopupBackground = rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_CENTER);
  }
}

LocationBarView::~LocationBarView() {
}

void LocationBarView::Init() {
  if (popup_window_mode_) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = font_.DeriveFont(3);
  }

  // URL edit field.
  // View container for URL edit field.
#if defined(OS_WIN)
  views::Widget* widget = GetWidget();
  location_entry_.reset(new AutocompleteEditViewWin(font_, this, model_, this,
                                                    widget->GetNativeView(),
                                                    profile_, command_updater_,
                                                    popup_window_mode_,
                                                    bubble_positioner_));
#else
  location_entry_.reset(new AutocompleteEditViewGtk(this, model_, profile_,
                                                    command_updater_,
                                                    popup_window_mode_,
                                                    bubble_positioner_));
  location_entry_->Init();
  // Make all the children of the widget visible. NOTE: this won't display
  // anything, it just toggles the visible flag.
  gtk_widget_show_all(location_entry_->widget());
  // Hide the widget. NativeViewHostGtk will make it visible again as
  // necessary.
  gtk_widget_hide(location_entry_->widget());
#endif
  location_entry_view_ = new views::NativeViewHost;
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(
#if defined(OS_WIN)
                               location_entry_->m_hWnd
#else
                               location_entry_->widget()
#endif
                               );  // NOLINT

  AddChildView(&selected_keyword_view_);
  selected_keyword_view_.SetFont(font_);
  selected_keyword_view_.SetVisible(false);
  selected_keyword_view_.set_parent_owned(false);

  SkColor dimmed_text = GetColor(false, DEEMPHASIZED_TEXT);

  AddChildView(&type_to_search_view_);
  type_to_search_view_.SetVisible(false);
  type_to_search_view_.SetFont(font_);
  type_to_search_view_.SetColor(dimmed_text);
  type_to_search_view_.set_parent_owned(false);

  AddChildView(&keyword_hint_view_);
  keyword_hint_view_.SetVisible(false);
  keyword_hint_view_.SetFont(font_);
  keyword_hint_view_.SetColor(dimmed_text);
  keyword_hint_view_.set_parent_owned(false);

  AddChildView(&security_image_view_);
  security_image_view_.SetVisible(false);
  security_image_view_.set_parent_owned(false);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentBlockedImageView* content_blocked_view =
        new ContentBlockedImageView(static_cast<ContentSettingsType>(i), this,
                                    profile_, bubble_positioner_);
    content_blocked_views_.push_back(content_blocked_view);
    AddChildView(content_blocked_view);
    content_blocked_view->SetVisible(false);
  }

  AddChildView(&info_label_);
  info_label_.SetVisible(false);
  info_label_.set_parent_owned(false);

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
SkColor LocationBarView::GetColor(bool is_secure, ColorKind kind) {
  enum SecurityState {
    NOT_SECURE = 0,
    SECURE,
    NUM_STATES
  };

  static bool initialized = false;
  static SkColor colors[NUM_STATES][NUM_KINDS];
  if (!initialized) {
#if defined(OS_WIN)
    colors[NOT_SECURE][BACKGROUND] = color_utils::GetSysSkColor(COLOR_WINDOW);
    colors[NOT_SECURE][TEXT] = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    colors[NOT_SECURE][SELECTED_TEXT] =
        color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
#else
    // TODO(beng): source from theme provider.
    colors[NOT_SECURE][BACKGROUND] = SK_ColorWHITE;
    colors[NOT_SECURE][TEXT] = SK_ColorBLACK;
    colors[NOT_SECURE][SELECTED_TEXT] = SK_ColorWHITE;
#endif
    colors[SECURE][BACKGROUND] = SkColorSetRGB(255, 245, 195);
    colors[SECURE][TEXT] = SK_ColorBLACK;
    colors[SECURE][SELECTED_TEXT] = 0;  // Unused
    colors[NOT_SECURE][DEEMPHASIZED_TEXT] =
        color_utils::AlphaBlend(colors[NOT_SECURE][TEXT],
                                colors[NOT_SECURE][BACKGROUND], 128);
    colors[SECURE][DEEMPHASIZED_TEXT] =
        color_utils::AlphaBlend(colors[SECURE][TEXT],
                                colors[SECURE][BACKGROUND], 128);
    colors[NOT_SECURE][SECURITY_TEXT] = color_utils::GetReadableColor(
        SkColorSetRGB(200, 0, 0), colors[NOT_SECURE][BACKGROUND]);
    colors[SECURE][SECURITY_TEXT] = SkColorSetRGB(0, 150, 20);
    colors[NOT_SECURE][SECURITY_INFO_BUBBLE_TEXT] =
        colors[NOT_SECURE][SECURITY_TEXT];
    colors[SECURE][SECURITY_INFO_BUBBLE_TEXT] = color_utils::GetReadableColor(
        SkColorSetRGB(0, 153, 51), colors[NOT_SECURE][BACKGROUND]);
    colors[NOT_SECURE][SCHEME_STRIKEOUT] = color_utils::GetReadableColor(
        SkColorSetRGB(210, 0, 0), colors[NOT_SECURE][BACKGROUND]);
    colors[SECURE][SCHEME_STRIKEOUT] = 0;  // Unused
    initialized = true;
  }

  return colors[is_secure ? SECURE : NOT_SECURE][kind];
}

void LocationBarView::Update(const TabContents* tab_for_state_restoring) {
  SetSecurityIcon(model_->GetIcon());
  RefreshContentBlockedViews();
  RefreshPageActionViews();
  std::wstring info_text, info_tooltip;
  ToolbarModel::InfoTextType info_text_type =
      model_->GetInfoText(&info_text, &info_tooltip);
  SetInfoText(info_text, info_text_type, info_tooltip);
  location_entry_->Update(tab_for_state_restoring);
  Layout();
  SchedulePaint();
}

void LocationBarView::UpdateContentBlockedIcons() {
  RefreshContentBlockedViews();

  Layout();
  SchedulePaint();
}

void LocationBarView::UpdatePageActions() {
  RefreshPageActionViews();

  Layout();
  SchedulePaint();
}

void LocationBarView::InvalidatePageActions() {
  DeletePageActionViews();
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
    selected_keyword_view_.set_profile(profile);
    keyword_hint_view_.set_profile(profile);
    for (ContentBlockedViews::const_iterator i(content_blocked_views_.begin());
         i != content_blocked_views_.end(); ++i)
      (*i)->set_profile(profile);
    security_image_view_.set_profile(profile);
  }
}

TabContents* LocationBarView::GetTabContents() const {
  return delegate_->GetTabContents();
}

void LocationBarView::SetPreviewEnabledPageAction(ExtensionAction *page_action,
                                                  bool preview_enabled) {
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

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0,
      (popup_window_mode_ ? kPopupBackground : kBackground)->height());
}

void LocationBarView::Layout() {
  DoLayout(true);
}

void LocationBarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

  const SkBitmap* background =
      popup_window_mode_ ?
          kPopupBackground :
          GetThemeProvider()->GetBitmapNamed(IDR_LOCATIONBG);

  canvas->TileImageInt(*background, 0, 0, 0, 0, width(), height());
  int top_margin = TopMargin();
  canvas->FillRectInt(
      GetColor(model_->GetSchemeSecurityLevel() == ToolbarModel::SECURE,
               BACKGROUND),
      0, top_margin, width(), std::max(height() - top_margin - kVertMargin, 0));
}

void LocationBarView::VisibleBoundsInRootChanged() {
  location_entry_->ClosePopup();
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
}

void LocationBarView::OnChanged() {
  DoLayout(false);
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

void LocationBarView::DoLayout(const bool force_layout) {
  if (!location_entry_.get())
    return;

  int entry_width = width() - (kEntryPadding * 2);

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kInnerPadding;
  }
  for (ContentBlockedViews::const_iterator i(content_blocked_views_.begin());
       i != content_blocked_views_.end(); ++i) {
    if ((*i)->IsVisible())
      entry_width -= (*i)->GetPreferredSize().width() + kInnerPadding;
  }
  gfx::Size security_image_size;
  if (security_image_view_.IsVisible()) {
    security_image_size = security_image_view_.GetPreferredSize();
    entry_width -= security_image_size.width() + kInnerPadding;
  }
  gfx::Size info_label_size;
  if (info_label_.IsVisible()) {
    info_label_size = info_label_.GetPreferredSize();
    entry_width -= (info_label_size.width() + kInnerPadding);
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
  bool needs_layout = force_layout;
  needs_layout |= AdjustHints(available_width);

  if (!needs_layout)
    return;

  // TODO(sky): baseline layout.
  int location_y = TopMargin();
  int location_height = std::max(height() - location_y - kVertMargin, 0);

  // First set the bounds for the label that appears to the right of the
  // security icon.
  int offset = width() - kEntryPadding;
  if (info_label_.IsVisible()) {
    offset -= info_label_size.width();
    info_label_.SetBounds(offset, location_y,
                          info_label_size.width(), location_height);
    offset -= kInnerPadding;
  }
  if (security_image_view_.IsVisible()) {
    offset -= security_image_size.width();
    security_image_view_.SetBounds(offset, location_y,
                                   security_image_size.width(),
                                   location_height);
    offset -= kInnerPadding;
  }

  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i) {
    if ((*i)->IsVisible()) {
      int page_action_width = (*i)->GetPreferredSize().width();
      offset -= page_action_width;
      (*i)->SetBounds(offset, location_y, page_action_width, location_height);
      offset -= kInnerPadding;
    }
  }
  // We use a reverse_iterator here because we're laying out the views from
  // right to left but in the vector they're ordered left to right.
  for (ContentBlockedViews::const_reverse_iterator
       i(content_blocked_views_.rbegin()); i != content_blocked_views_.rend();
       ++i) {
    if ((*i)->IsVisible()) {
      int content_blocked_width = (*i)->GetPreferredSize().width();
      offset -= content_blocked_width;
      (*i)->SetBounds(offset, location_y, content_blocked_width,
                      location_height);
      offset -= kInnerPadding;
    }
  }
  gfx::Rect location_bounds(kEntryPadding, location_y, entry_width,
                            location_height);
  if (selected_keyword_view_.IsVisible()) {
    LayoutView(true, &selected_keyword_view_, available_width,
               &location_bounds);
  } else if (keyword_hint_view_.IsVisible()) {
    LayoutView(false, &keyword_hint_view_, available_width,
               &location_bounds);
  } else if (type_to_search_view_.IsVisible()) {
    LayoutView(false, &type_to_search_view_, available_width,
               &location_bounds);
  }

  location_entry_view_->SetBounds(location_bounds);
  if (!force_layout) {
    // If force_layout is false and we got this far it means one of the views
    // was added/removed or changed in size. We need to paint ourselves.
    SchedulePaint();
  }
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
  return (pref_width + kInnerPadding <= available_width);
}

bool LocationBarView::NeedsResize(View* view, int available_width) {
  gfx::Size size = view->GetPreferredSize();
  if (!UsePref(size.width(), available_width))
    size = view->GetMinimumSize();
  return (view->width() != size.width());
}

bool LocationBarView::AdjustHints(int available_width) {
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;
  bool show_search_hint(location_entry_->model()->show_search_hint());
  DCHECK(keyword.empty() || !show_search_hint);

  if (show_search_hint) {
    // Only show type to search if all the text fits.
    gfx::Size view_pref = type_to_search_view_.GetPreferredSize();
    show_search_hint = UsePref(view_pref.width(), available_width);
  }

  // NOTE: This isn't just one big || statement as ToggleVisibility MUST be
  // invoked for each view.
  bool needs_layout = false;
  needs_layout |= ToggleVisibility(show_selected_keyword,
                                   &selected_keyword_view_);
  needs_layout |= ToggleVisibility(show_keyword_hint, &keyword_hint_view_);
  needs_layout |= ToggleVisibility(show_search_hint, &type_to_search_view_);
  if (show_selected_keyword) {
    if (selected_keyword_view_.keyword() != keyword) {
      needs_layout = true;
      selected_keyword_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&selected_keyword_view_, available_width);
  } else if (show_keyword_hint) {
    if (keyword_hint_view_.keyword() != keyword) {
      needs_layout = true;
      keyword_hint_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&keyword_hint_view_, available_width);
  }

  return needs_layout;
}

void LocationBarView::LayoutView(bool leading,
                                 views::View* view,
                                 int available_width,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  gfx::Size view_size = view->GetPreferredSize();
  if (!UsePref(view_size.width(), available_width))
    view_size = view->GetMinimumSize();
  if (view_size.width() + kInnerPadding < bounds->width()) {
    view->SetVisible(true);
    if (leading) {
      view->SetBounds(bounds->x(), bounds->y(), view_size.width(),
                      bounds->height());
      bounds->Offset(view_size.width() + kInnerPadding, 0);
    } else {
      view->SetBounds(bounds->right() - view_size.width(), bounds->y(),
                      view_size.width(), bounds->height());
    }
    bounds->set_width(bounds->width() - view_size.width() - kInnerPadding);
  } else {
    view->SetVisible(false);
  }
}

void LocationBarView::SetSecurityIcon(ToolbarModel::Icon icon) {
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      security_image_view_.SetImageShown(SecurityImageView::LOCK);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::WARNING_ICON:
      security_image_view_.SetImageShown(SecurityImageView::WARNING);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::NO_ICON:
      security_image_view_.SetVisible(false);
      break;
    default:
      NOTREACHED();
      security_image_view_.SetVisible(false);
      break;
  }
}

void LocationBarView::RefreshContentBlockedViews() {
  const TabContents* tab_contents = delegate_->GetTabContents();
  for (ContentBlockedViews::const_iterator i(content_blocked_views_.begin());
       i != content_blocked_views_.end(); ++i) {
    (*i)->SetVisible((!model_->input_in_progress() && tab_contents) ?
        tab_contents->IsContentBlocked((*i)->content_type()) : false);
  }
}

void LocationBarView::DeletePageActionViews() {
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    RemoveChildView(*i);
  STLDeleteElements(&page_action_views_);
}

void LocationBarView::RefreshPageActionViews() {
  std::vector<ExtensionAction*> page_actions;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;

  std::map<ExtensionAction*, bool> old_visibility;
  for (PageActionViews::const_iterator i(page_action_views_.begin());
       i != page_action_views_.end(); ++i)
    old_visibility[(*i)->image_view()->page_action()] = (*i)->IsVisible();

  // Remember the previous visibility of the page actions so that we can
  // notify when this changes.
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->extensions()->at(i)->page_action())
      page_actions.push_back(service->extensions()->at(i)->page_action());
  }

  // On startup we sometimes haven't loaded any extensions. This makes sure
  // we catch up when the extensions (and any page actions) load.
  if (page_actions.size() != page_action_views_.size()) {
    DeletePageActionViews();  // Delete the old views (if any).

    page_action_views_.resize(page_actions.size());

    for (size_t i = 0; i < page_actions.size(); ++i) {
      page_action_views_[i] = new PageActionWithBadgeView(
          new PageActionImageView(this, profile_,
                                  page_actions[i], bubble_positioner_));
      page_action_views_[i]->SetVisible(false);
      AddChildView(page_action_views_[i]);
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

void LocationBarView::SetInfoText(const std::wstring& text,
                                  ToolbarModel::InfoTextType text_type,
                                  const std::wstring& tooltip_text) {
  info_label_.SetVisible(!text.empty());
  info_label_.SetText(text);
  if (text_type == ToolbarModel::INFO_EV_TEXT)
    info_label_.SetColor(GetColor(true, SECURITY_TEXT));
  info_label_.SetTooltipText(tooltip_text);
}

bool LocationBarView::ToggleVisibility(bool new_vis, View* view) {
  DCHECK(view);
  if (view->IsVisible() != new_vis) {
    view->SetVisible(new_vis);
    return true;
  }
  return false;
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

bool LocationBarView::GetAccessibleName(std::wstring* name) {
  DCHECK(name);

  if (!accessible_name_.empty()) {
    name->assign(accessible_name_);
    return true;
  }
  return false;
}

bool LocationBarView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}

void LocationBarView::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

// SelectedKeywordView -------------------------------------------------------

// The background is drawn using HorizontalPainter. This is the
// left/center/right image names.
static const int kBorderImages[] = {
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_L,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_C,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_R };

// Insets around the label.
static const int kTopInset = 0;
static const int kBottomInset = 0;
static const int kLeftInset = 4;
static const int kRightInset = 4;

// Offset from the top the background is drawn at.
static const int kBackgroundYOffset = 2;

LocationBarView::SelectedKeywordView::SelectedKeywordView(Profile* profile)
    : background_painter_(kBorderImages),
      profile_(profile) {
  AddChildView(&full_label_);
  AddChildView(&partial_label_);
  // Full_label and partial_label are deleted by us, make sure View doesn't
  // delete them too.
  full_label_.set_parent_owned(false);
  partial_label_.set_parent_owned(false);
  full_label_.SetVisible(false);
  partial_label_.SetVisible(false);
  full_label_.set_border(
      views::Border::CreateEmptyBorder(kTopInset, kLeftInset, kBottomInset,
                                       kRightInset));
  partial_label_.set_border(
      views::Border::CreateEmptyBorder(kTopInset, kLeftInset, kBottomInset,
                                       kRightInset));
  full_label_.SetColor(SK_ColorBLACK);
  partial_label_.SetColor(SK_ColorBLACK);
}

LocationBarView::SelectedKeywordView::~SelectedKeywordView() {
}

void LocationBarView::SelectedKeywordView::SetFont(const gfx::Font& font) {
  full_label_.SetFont(font);
  partial_label_.SetFont(font);
}

void LocationBarView::SelectedKeywordView::Paint(gfx::Canvas* canvas) {
  canvas->TranslateInt(0, kBackgroundYOffset);
  background_painter_.Paint(width(), height() - kTopInset, canvas);
  canvas->TranslateInt(0, -kBackgroundYOffset);
}

gfx::Size LocationBarView::SelectedKeywordView::GetPreferredSize() {
  return full_label_.GetPreferredSize();
}

gfx::Size LocationBarView::SelectedKeywordView::GetMinimumSize() {
  return partial_label_.GetMinimumSize();
}

void LocationBarView::SelectedKeywordView::Layout() {
  gfx::Size pref = GetPreferredSize();
  bool at_pref = (width() == pref.width());
  if (at_pref)
    full_label_.SetBounds(0, 0, width(), height());
  else
    partial_label_.SetBounds(0, 0, width(), height());
  full_label_.SetVisible(at_pref);
  partial_label_.SetVisible(!at_pref);
}

void LocationBarView::SelectedKeywordView::SetKeyword(
    const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring short_name = GetKeywordName(profile_, keyword);
  full_label_.SetText(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                            short_name));
  const std::wstring min_string = CalculateMinString(short_name);
  if (!min_string.empty()) {
    partial_label_.SetText(
        l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, min_string));
  } else {
    partial_label_.SetText(full_label_.GetText());
  }
}

std::wstring LocationBarView::SelectedKeywordView::CalculateMinString(
    const std::wstring& description) {
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

// KeywordHintView -------------------------------------------------------------

// Amount of space to offset the tab image from the top of the view by.
static const int kTabImageYOffset = 4;

LocationBarView::KeywordHintView::KeywordHintView(Profile* profile)
    : profile_(profile) {
  AddChildView(&leading_label_);
  AddChildView(&trailing_label_);

  if (!kTabButtonBitmap) {
    kTabButtonBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
  }
}

LocationBarView::KeywordHintView::~KeywordHintView() {
  // Labels are freed by us. Remove them so that View doesn't
  // try to free them too.
  RemoveChildView(&leading_label_);
  RemoveChildView(&trailing_label_);
}

void LocationBarView::KeywordHintView::SetFont(const gfx::Font& font) {
  leading_label_.SetFont(font);
  trailing_label_.SetFont(font);
}

void LocationBarView::KeywordHintView::SetColor(const SkColor& color) {
  leading_label_.SetColor(color);
  trailing_label_.SetColor(color);
}

void LocationBarView::KeywordHintView::SetKeyword(const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  const std::wstring keyword_hint(l10n_util::GetStringF(
      IDS_OMNIBOX_KEYWORD_HINT, std::wstring(),
      GetKeywordName(profile_, keyword), &content_param_offsets));
  if (content_param_offsets.size() == 2) {
    leading_label_.SetText(keyword_hint.substr(0,
                                               content_param_offsets.front()));
    trailing_label_.SetText(keyword_hint.substr(content_param_offsets.front()));
  } else {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
  }
}

void LocationBarView::KeywordHintView::Paint(gfx::Canvas* canvas) {
  int image_x = leading_label_.IsVisible() ? leading_label_.width() : 0;

  // Since we paint the button image directly on the canvas (instead of using a
  // child view), we must mirror the button's position manually if the locale
  // is right-to-left.
  gfx::Rect tab_button_bounds(image_x,
                              kTabImageYOffset,
                              kTabButtonBitmap->width(),
                              kTabButtonBitmap->height());
  tab_button_bounds.set_x(MirroredLeftPointForRect(tab_button_bounds));
  canvas->DrawBitmapInt(*kTabButtonBitmap,
                        tab_button_bounds.x(),
                        tab_button_bounds.y());
}

gfx::Size LocationBarView::KeywordHintView::GetPreferredSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  gfx::Size prefsize = leading_label_.GetPreferredSize();
  int width = prefsize.width();
  width += kTabButtonBitmap->width();
  prefsize = trailing_label_.GetPreferredSize();
  width += prefsize.width();
  return gfx::Size(width, prefsize.height());
}

gfx::Size LocationBarView::KeywordHintView::GetMinimumSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  return gfx::Size(kTabButtonBitmap->width(), 0);
}

void LocationBarView::KeywordHintView::Layout() {
  // TODO(sky): baseline layout.
  bool show_labels = (width() != kTabButtonBitmap->width());

  leading_label_.SetVisible(show_labels);
  trailing_label_.SetVisible(show_labels);
  int x = 0;
  gfx::Size pref;

  if (show_labels) {
    pref = leading_label_.GetPreferredSize();
    leading_label_.SetBounds(x, 0, pref.width(), height());

    x += pref.width() + kTabButtonBitmap->width();
    pref = trailing_label_.GetPreferredSize();
    trailing_label_.SetBounds(x, 0, pref.width(), height());
  }
}

bool LocationBarView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  if (keyword_hint_view_.IsVisible() &&
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
  // For now, we just override back-space as it is the accelerator for back
  // navigation.
  if (e.GetKeyCode() == base::VKEY_BACK)
    return true;
  return false;
#endif
}

// ShowInfoBubbleTask-----------------------------------------------------------

class LocationBarView::ShowInfoBubbleTask : public Task {
 public:
  explicit ShowInfoBubbleTask(
      LocationBarView::LocationBarImageView* image_view);
  virtual void Run();
  void Cancel();

 private:
  LocationBarView::LocationBarImageView* image_view_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(ShowInfoBubbleTask);
};

LocationBarView::ShowInfoBubbleTask::ShowInfoBubbleTask(
    LocationBarView::LocationBarImageView* image_view)
    : image_view_(image_view),
      cancelled_(false) {
}

void LocationBarView::ShowInfoBubbleTask::Run() {
  if (cancelled_)
    return;

  if (!image_view_->GetWidget()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.  Also makes sure we NULL
    // show_info_bubble_task_ to prevent the SecurityImageView from keeping a
    // dangling pointer.
    image_view_->show_info_bubble_task_ = NULL;
    return;
  }

  image_view_->ShowInfoBubble();
}

void LocationBarView::ShowInfoBubbleTask::Cancel() {
  cancelled_ = true;
}

// -----------------------------------------------------------------------------

void LocationBarView::ShowFirstRunBubbleInternal(bool use_OEM_bubble) {
  if (!location_entry_view_)
    return;
  if (!location_entry_view_->GetWidget()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.
    return;
  }

  gfx::Point location;

  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (UILayoutIsRightToLeft())
    location.Offset(width(), 0);
  views::View::ConvertPointToScreen(this, &location);

  // We try to guess that 20 pixels offset is a good place for the first
  // letter in the OmniBox.
  gfx::Rect bounds(location.x(), location.y(), 20, height());

  // Moving the bounds "backwards" so that it appears within the location bar
  // if the UI layout is RTL.
  if (UILayoutIsRightToLeft())
    bounds.set_x(location.x() - 20);

#if defined(OS_WIN)
  FirstRunBubble::Show(profile_, GetWindow(), bounds, use_OEM_bubble);
#else
  // First run bubble doesn't make sense for Chrome OS.
#endif
}

// LocationBarImageView---------------------------------------------------------

LocationBarView::LocationBarImageView::LocationBarImageView(
    const BubblePositioner* bubble_positioner)
    : info_bubble_(NULL),
      show_info_bubble_task_(NULL),
      bubble_positioner_(bubble_positioner) {
}

LocationBarView::LocationBarImageView::~LocationBarImageView() {
  if (show_info_bubble_task_)
    show_info_bubble_task_->Cancel();

  if (info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::LocationBarImageView::OnMouseMoved(
    const views::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_) {
    // If an info bubble is currently showing, nothing to do.
    return;
  }

  show_info_bubble_task_ = new ShowInfoBubbleTask(this);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, show_info_bubble_task_,
      kInfoBubbleHoverDelayMs);
}

void LocationBarView::LocationBarImageView::OnMouseExited(
    const views::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::LocationBarImageView::InfoBubbleClosing(
    InfoBubble* info_bubble, bool closed_by_escape) {
  info_bubble_ = NULL;
}

void LocationBarView::LocationBarImageView::ShowInfoBubbleImpl(
    const std::wstring& text, SkColor text_color) {
  gfx::Rect bounds(bubble_positioner_->GetLocationStackBounds());
  gfx::Point location;
  views::View::ConvertPointToScreen(this, &location);
  bounds.set_x(location.x());
  bounds.set_width(width());

  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetColor(text_color);
  label->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(2));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SizeToFit(0);
  DCHECK(info_bubble_ == NULL);
  info_bubble_ = InfoBubble::Show(GetWindow(), bounds, label, this);
  show_info_bubble_task_ = NULL;
}

// SecurityImageView------------------------------------------------------------

// static
SkBitmap* LocationBarView::SecurityImageView::lock_icon_ = NULL;
SkBitmap* LocationBarView::SecurityImageView::warning_icon_ = NULL;
SkBitmap* LocationBarView::
    ContentBlockedImageView::icons_[CONTENT_SETTINGS_NUM_TYPES] = { NULL };

LocationBarView::SecurityImageView::SecurityImageView(
    Profile* profile,
    ToolbarModel* model,
    const BubblePositioner* bubble_positioner)
    : LocationBarImageView(bubble_positioner),
      profile_(profile),
      model_(model) {
  if (!lock_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    lock_icon_ = rb.GetBitmapNamed(IDR_LOCK);
    warning_icon_ = rb.GetBitmapNamed(IDR_WARNING);
  }
  SetImageShown(LOCK);
}

LocationBarView::SecurityImageView::~SecurityImageView() {
}

void LocationBarView::SecurityImageView::SetImageShown(Image image) {
  switch (image) {
    case LOCK:
      SetImage(lock_icon_);
      break;
    case WARNING:
      SetImage(warning_icon_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool LocationBarView::SecurityImageView::OnMousePressed(
    const views::MouseEvent& event) {
  TabContents* tab = BrowserList::GetLastActive()->GetSelectedTabContents();
  NavigationEntry* nav_entry = tab->controller().GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return true;
  }
  tab->ShowPageInfo(nav_entry->url(), nav_entry->ssl(), true);
  return true;
}

void LocationBarView::SecurityImageView::ShowInfoBubble() {
  std::wstring text;
  model_->GetIconHoverText(&text);
  ShowInfoBubbleImpl(text, GetColor(
      model_->GetSecurityLevel() == ToolbarModel::SECURE,
      SECURITY_INFO_BUBBLE_TEXT));
}

// ContentBlockedImageView------------------------------------------------------

LocationBarView::ContentBlockedImageView::ContentBlockedImageView(
    ContentSettingsType content_type,
    const LocationBarView* parent,
    Profile* profile,
    const BubblePositioner* bubble_positioner)
    : content_type_(content_type),
      parent_(parent),
      profile_(profile),
      info_bubble_(NULL),
      bubble_positioner_(bubble_positioner) {
  if (!icons_[CONTENT_SETTINGS_TYPE_COOKIES]) {
    static const int kIconIDs[CONTENT_SETTINGS_NUM_TYPES] = {
      IDR_BLOCKED_COOKIES,
      IDR_BLOCKED_IMAGES,
      IDR_BLOCKED_JAVASCRIPT,
      IDR_BLOCKED_PLUGINS,
      IDR_BLOCKED_POPUPS,
    };
    DCHECK_EQ(arraysize(kIconIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      icons_[i] = rb.GetBitmapNamed(kIconIDs[i]);
  }
  SetImage(icons_[content_type_]);

  static const int kTooltipIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    IDS_BLOCKED_COOKIES_TITLE,
    IDS_BLOCKED_IMAGES_TITLE,
    IDS_BLOCKED_JAVASCRIPT_TITLE,
    IDS_BLOCKED_PLUGINS_TITLE,
    IDS_BLOCKED_POPUPS_TOOLTIP,
  };
  DCHECK_EQ(arraysize(kTooltipIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  SetTooltipText(l10n_util::GetString(kTooltipIDs[content_type_]));
}

LocationBarView::ContentBlockedImageView::~ContentBlockedImageView() {
  if (info_bubble_)
    info_bubble_->Close();
}

bool LocationBarView::ContentBlockedImageView::OnMousePressed(
    const views::MouseEvent& event) {
  gfx::Rect bounds(bubble_positioner_->GetLocationStackBounds());
  gfx::Point location;
  views::View::ConvertPointToScreen(this, &location);
  bounds.set_x(location.x());
  bounds.set_width(width());

  TabContents* tab_contents = parent_->GetTabContents();
  if (!tab_contents)
    return true;
  GURL url = tab_contents->GetURL();
  std::wstring display_host;
  net::AppendFormattedHost(url,
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages), &display_host,
      NULL, NULL);
  ContentBlockedBubbleContents* bubble_contents =
      new ContentBlockedBubbleContents(content_type_, url.host(), display_host,
                                       profile_, tab_contents);
  DCHECK(!info_bubble_);
  info_bubble_ = InfoBubble::Show(GetWindow(), bounds, bubble_contents, this);
  bubble_contents->set_info_bubble(info_bubble_);
  return true;
}

void LocationBarView::ContentBlockedImageView::VisibilityChanged(
    View* starting_from,
    bool is_visible) {
  if (!is_visible && info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::ContentBlockedImageView::InfoBubbleClosing(
    InfoBubble* info_bubble,
    bool closed_by_escape) {
  info_bubble_ = NULL;
}

bool LocationBarView::ContentBlockedImageView::CloseOnEscape() {
  return true;
}

// PageActionImageView----------------------------------------------------------

LocationBarView::PageActionImageView::PageActionImageView(
    LocationBarView* owner,
    Profile* profile,
    ExtensionAction* page_action,
    const BubblePositioner* bubble_positioner)
    : LocationBarImageView(bubble_positioner),
      owner_(owner),
      profile_(profile),
      page_action_(page_action),
      current_tab_id_(-1),
      preview_enabled_(false),
      popup_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
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
    tracker_->PostLoadImageTask(
        extension->GetResource(*iter),
        gfx::Size(Extension::kPageActionIconMaxSize,
                  Extension::kPageActionIconMaxSize));
  }

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(profile_));
}

LocationBarView::PageActionImageView::~PageActionImageView() {
  if (tracker_)
    tracker_->StopTrackingImageLoad();

  if (popup_)
    HidePopup();
}

void LocationBarView::PageActionImageView::ExecuteAction(int button) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    // In tests, GetLastActive could return NULL, so we need to have
    // a fallback.
    // TODO(erikkay): Find a better way to get the Browser that this
    // button is in.
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (!browser)
      browser = BrowserList::FindBrowserWithProfile(profile_);
    DCHECK(browser);

    bool popup_showing = popup_ != NULL;

    // Always hide the current popup. Only one popup at a time.
    HidePopup();

    // If we were already showing, then treat this click as a dismiss.
    if (popup_showing)
      return;

    View* parent = GetParent();
    gfx::Point origin;
    View::ConvertPointToScreen(parent, &origin);
    gfx::Rect rect = parent->bounds();
    rect.set_x(origin.x());
    rect.set_y(origin.y());

    popup_ = ExtensionPopup::Show(page_action_->GetPopupUrl(current_tab_id_),
                                  browser,
                                  browser->profile(),
                                  browser->window()->GetNativeHandle(),
                                  rect,
                                  BubbleBorder::TOP_RIGHT,
                                  true);  // Activate the popup window.
    popup_->set_delegate(this);
  } else {
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        profile_, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(), button);
  }
}

void LocationBarView::PageActionImageView::OnMouseMoved(
    const views::MouseEvent& event) {
  // PageActionImageView uses normal tooltips rather than the info bubble,
  // so just do nothing here rather than letting LocationBarImageView start
  // its hover timer.
}

bool LocationBarView::PageActionImageView::OnMousePressed(
    const views::MouseEvent& event) {
  int button = -1;
  if (event.IsLeftMouseButton()) {
    button = 1;
  } else if (event.IsMiddleMouseButton()) {
    button = 2;
  } else if (event.IsRightMouseButton()) {
    // Get the top left point of this button in screen coordinates.
    gfx::Point point = gfx::Point(0, 0);
    ConvertPointToScreen(this, &point);

    // Make the menu appear below the button.
    point.Offset(0, height());

    Extension* extension = profile_->GetExtensionsService()->GetExtensionById(
        page_action()->extension_id(), false);

    if (!context_menu_.get())
      context_menu_.reset(new ExtensionActionContextMenu());
    context_menu_->Run(extension, point);
    return false;
  }

  ExecuteAction(button);
  return true;
}

void LocationBarView::PageActionImageView::ShowInfoBubble() {
  ShowInfoBubbleImpl(ASCIIToWide(tooltip_), GetColor(false, TEXT));
}

void LocationBarView::PageActionImageView::OnImageLoaded(SkBitmap* image,
                                                         size_t index) {
  // We loaded icons()->size() icons, plus one extra if the page action had
  // a default icon.
  size_t total_icons = page_action_->icon_paths()->size();
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    if (index < page_action_->icon_paths()->size())
      page_action_icons_[page_action_->icon_paths()->at(index)] = *image;
    else
      page_action_icons_[page_action_->default_icon_path()] = *image;
  }

  // If we are done, release the tracker.
  if (index == total_icons - 1)
    tracker_ = NULL;

  owner_->UpdatePageActions();
}

void LocationBarView::PageActionImageView::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = ExtensionTabUtil::GetTabId(contents);
  current_url_ = url;

  bool visible = preview_enabled_ ||
                 page_action_->GetIsVisible(current_tab_id_);
  if (visible) {
    // Set the tooltip.
    tooltip_ = page_action_->GetTitle(current_tab_id_);
    SetTooltipText(UTF8ToWide(tooltip_));

    // Set the image.
    // It can come from three places. In descending order of priority:
    // - The developer can set it dynamically by path or bitmap. It will be in
    //   page_action_->GetIcon().
    // - The developer can set it dynamically by index. It will be in
    //   page_action_->GetIconIndex().
    // - It can be set in the manifest by path. It will be in page_action_->
    //   default_icon_path().

    // First look for a dynamically set bitmap.
    SkBitmap icon = page_action_->GetIcon(current_tab_id_);
    if (icon.isNull()) {
      int icon_index = page_action_->GetIconIndex(current_tab_id_);
      std::string icon_path;
      if (icon_index >= 0)
        icon_path = page_action_->icon_paths()->at(icon_index);
      else
        icon_path = page_action_->default_icon_path();

      if (!icon_path.empty()) {
        PageActionMap::iterator iter = page_action_icons_.find(icon_path);
        if (iter != page_action_icons_.end())
          icon = iter->second;
      }
    }

    if (!icon.isNull())
      SetImage(&icon);
  }
  SetVisible(visible);
}

void LocationBarView::PageActionImageView::BubbleBrowserWindowClosing(
    BrowserBubble* bubble) {
  HidePopup();
}

void LocationBarView::PageActionImageView::BubbleLostFocus(
    BrowserBubble* bubble,
    gfx::NativeView focused_view) {
  if (!popup_)
    return;

  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &LocationBarView::PageActionImageView::HidePopup));
}

void LocationBarView::PageActionImageView::HidePopup() {
  if (!popup_)
    return;

  // This sometimes gets called via a timer (See BubbleLostFocus), so clear
  // the method factory. in case one is pending.
  method_factory_.RevokeAll();

  // Save the popup in a local since destroying it calls BubbleLostFocus,
  // which will try to call HidePopup() again.
  ExtensionPopup* closing_popup = popup_;
  popup_ = NULL;

  closing_popup->DetachFromBrowser();
  delete closing_popup;
}

void LocationBarView::PageActionImageView::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (!popup_ || Details<ExtensionHost>(popup_->host()) != details)
        return;

      HidePopup();
      break;
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble(bool use_OEM_bubble) {
  // We wait 30 milliseconds to open. It allows less flicker.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarView::ShowFirstRunBubbleInternal, use_OEM_bubble);
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

void LocationBarView::AcceptInputWithDisposition(WindowOpenDisposition disp) {
  location_entry_->model()->AcceptInput(disp, false);
}

void LocationBarView::FocusLocation() {
  location_entry_->SetFocus();
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
  if (index >= 0) {
    size_t current = 0;
    for (size_t i = 0; i < page_action_views_.size(); ++i) {
      if (page_action_views_[i]->IsVisible()) {
        if (current == index)
          return page_action_views_[i]->image_view()->page_action();

        ++current;
      }
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarView::TestPageActionPressed(size_t index) {
  if (index >= 0) {
    size_t current = 0;
    for (size_t i = 0; i < page_action_views_.size(); ++i) {
      if (page_action_views_[i]->IsVisible()) {
        if (current == index) {
          const int kLeftMouseButton = 1;
          page_action_views_[i]->image_view()->ExecuteAction(kLeftMouseButton);
          return;
        }
        ++current;
      }
    }
  }

  NOTREACHED();
}
