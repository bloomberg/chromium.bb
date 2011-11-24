// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/nine_box.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/gtk/gtk_floating_container.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Used as the color of the text in the entry box and the text for the results
// label for failure searches.
const GdkColor& kEntryTextColor = ui::kGdkBlack;

// Used as the color of the background of the entry box and the background of
// the find label for successful searches.
const GdkColor& kEntryBackgroundColor = ui::kGdkWhite;
const GdkColor kFindFailureBackgroundColor = GDK_COLOR_RGB(255, 102, 102);
const GdkColor kFindSuccessTextColor = GDK_COLOR_RGB(178, 178, 178);

// Padding around the container.
const int kBarPaddingTopBottom = 4;
const int kEntryPaddingLeft = 6;
const int kCloseButtonPaddingLeft = 3;
const int kBarPaddingRight = 4;

// The height of the findbar dialog, as dictated by the size of the background
// images.
const int kFindBarHeight = 32;

// The default width of the findbar dialog. It may get smaller if the window
// is narrow.
const int kFindBarWidth = 303;

// The size of the "rounded" corners.
const int kCornerSize = 3;

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

// Returns a list of points that either form the outline of the status bubble
// (|type| == FRAME_MASK) or form the inner border around the inner edge
// (|type| == FRAME_STROKE).
std::vector<GdkPoint> MakeFramePolygonPoints(int width,
                                             int height,
                                             FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  bool ltr = !base::i18n::IsRTL();
  // If we have a stroke, we have to offset some of our points by 1 pixel.
  // We have to inset by 1 pixel when we draw horizontal lines that are on the
  // bottom or when we draw vertical lines that are closer to the end (end is
  // right for ltr).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for LTR.
  int x_off_l = ltr ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !ltr ? -y_off : 0;

  // Top left corner
  points.push_back(MakeBidiGdkPoint(x_off_r, 0, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r, kCornerSize, width, ltr));

  // Bottom left corner
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r, height - kCornerSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      (2 * kCornerSize) + x_off_l, height + y_off,
      width, ltr));

  // Bottom right corner
  points.push_back(MakeBidiGdkPoint(
      width - (2 * kCornerSize) + x_off_r, height + y_off,
      width, ltr));
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_l, height - kCornerSize, width, ltr));

  // Top right corner
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_l, kCornerSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(width + x_off_l, 0, width, ltr));

  return points;
}

// Give the findbar dialog its unique shape using images.
void SetDialogShape(GtkWidget* widget) {
  static NineBox* dialog_shape = NULL;
  if (!dialog_shape) {
    dialog_shape = new NineBox(
      IDR_FIND_DLG_LEFT_BACKGROUND,
      IDR_FIND_DLG_MIDDLE_BACKGROUND,
      IDR_FIND_DLG_RIGHT_BACKGROUND,
      0, 0, 0, 0, 0, 0);
  }

  dialog_shape->ContourWidget(widget);
}

// Return a ninebox that will paint the border of the findbar dialog. This is
// shared across all instances of the findbar. Do not free the returned pointer.
const NineBox* GetDialogBorder() {
  static NineBox* dialog_border = NULL;
  if (!dialog_border) {
    dialog_border = new NineBox(
      IDR_FIND_DIALOG_LEFT,
      IDR_FIND_DIALOG_MIDDLE,
      IDR_FIND_DIALOG_RIGHT,
      0, 0, 0, 0, 0, 0);
  }

  return dialog_border;
}

// Like gtk_util::CreateGtkBorderBin, but allows control over the alignment and
// returns both the event box and the alignment so we can modify it during its
// lifetime (i.e. during a theme change).
void BuildBorder(GtkWidget* child,
                 int padding_top, int padding_bottom, int padding_left,
                 int padding_right,
                 GtkWidget** ebox, GtkWidget** alignment) {
  *ebox = gtk_event_box_new();
  *alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(*alignment),
                            padding_top, padding_bottom, padding_left,
                            padding_right);
  gtk_container_add(GTK_CONTAINER(*alignment), child);
  gtk_container_add(GTK_CONTAINER(*ebox), *alignment);
}

}  // namespace

FindBarGtk::FindBarGtk(BrowserWindowGtk* window)
    : browser_(window->browser()),
      window_(window),
      theme_service_(GtkThemeService::GetFrom(browser_->profile())),
      container_width_(-1),
      container_height_(-1),
      match_label_failure_(false),
      ignore_changed_signal_(false) {
  InitWidgets();
  ViewIDUtil::SetID(text_entry_, VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);

  // Insert the widget into the browser gtk hierarchy.
  window_->AddFindBar(this);

  // Hook up signals after the widget has been added to the hierarchy so the
  // widget will be realized.
  g_signal_connect(text_entry_, "changed",
                   G_CALLBACK(OnChanged), this);
  g_signal_connect_after(text_entry_, "key-press-event",
                         G_CALLBACK(OnKeyPressEvent), this);
  g_signal_connect_after(text_entry_, "key-release-event",
                         G_CALLBACK(OnKeyReleaseEvent), this);
  // When the user tabs to us or clicks on us, save where the focus used to
  // be.
  g_signal_connect(text_entry_, "focus",
                   G_CALLBACK(OnFocus), this);
  gtk_widget_add_events(text_entry_, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(text_entry_, "button-press-event",
                   G_CALLBACK(OnButtonPress), this);
  g_signal_connect(text_entry_, "move-cursor", G_CALLBACK(OnMoveCursor), this);
  g_signal_connect(text_entry_, "activate", G_CALLBACK(OnActivateThunk), this);
  g_signal_connect(text_entry_, "direction-changed",
                   G_CALLBACK(OnWidgetDirectionChanged), this);
  g_signal_connect(text_entry_, "focus-in-event",
                   G_CALLBACK(OnFocusInThunk), this);
  g_signal_connect(text_entry_, "focus-out-event",
                   G_CALLBACK(OnFocusOutThunk), this);
  g_signal_connect(container_, "expose-event",
                   G_CALLBACK(OnExpose), this);
}

FindBarGtk::~FindBarGtk() {
}

void FindBarGtk::InitWidgets() {
  // The find bar is basically an hbox with a gtkentry (text box) followed by 3
  // buttons (previous result, next result, close).  We wrap the hbox in a gtk
  // alignment and a gtk event box to get the padding and light blue
  // background. We put that event box in a fixed in order to control its
  // lateral position. We put that fixed in a SlideAnimatorGtk in order to get
  // the slide effect.
  GtkWidget* hbox = gtk_hbox_new(false, 0);
  container_ = gtk_util::CreateGtkBorderBin(hbox, NULL,
      kBarPaddingTopBottom, kBarPaddingTopBottom,
      kEntryPaddingLeft, kBarPaddingRight);
  gtk_widget_set_size_request(container_, kFindBarWidth, -1);
  ViewIDUtil::SetID(container_, VIEW_ID_FIND_IN_PAGE);
  gtk_widget_set_app_paintable(container_, TRUE);

  slide_widget_.reset(new SlideAnimatorGtk(container_,
                                           SlideAnimatorGtk::DOWN,
                                           0, false, true, NULL));

  close_button_.reset(CustomDrawButton::CloseButton(theme_service_));
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true,
                               kCloseButtonPaddingLeft);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnClickedThunk), this);
  gtk_widget_set_tooltip_text(close_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP).c_str());

  find_next_button_.reset(new CustomDrawButton(theme_service_,
      IDR_FINDINPAGE_NEXT, IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_H,
      IDR_FINDINPAGE_NEXT_P, GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU));
  g_signal_connect(find_next_button_->widget(), "clicked",
                   G_CALLBACK(OnClickedThunk), this);
  gtk_widget_set_tooltip_text(find_next_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_NEXT_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_next_button_->widget(),
                   FALSE, FALSE, 0);

  find_previous_button_.reset(new CustomDrawButton(theme_service_,
      IDR_FINDINPAGE_PREV, IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_H,
      IDR_FINDINPAGE_PREV_P, GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
  g_signal_connect(find_previous_button_->widget(), "clicked",
                   G_CALLBACK(OnClickedThunk), this);
  gtk_widget_set_tooltip_text(find_previous_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_previous_button_->widget(),
                   FALSE, FALSE, 0);

  // Make a box for the edit and match count widgets.
  GtkWidget* content_hbox = gtk_hbox_new(FALSE, 0);

  text_entry_ = gtk_entry_new();
  gtk_entry_set_has_frame(GTK_ENTRY(text_entry_), FALSE);

  match_count_label_ = gtk_label_new(NULL);
  // This line adds padding on the sides so that the label has even padding on
  // all edges.
  gtk_misc_set_padding(GTK_MISC(match_count_label_), 2, 0);
  match_count_event_box_ = gtk_event_box_new();
  GtkWidget* match_count_centerer = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(match_count_centerer), match_count_event_box_,
                     TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(match_count_centerer), 1);
  gtk_container_add(GTK_CONTAINER(match_count_event_box_), match_count_label_);

  gtk_box_pack_end(GTK_BOX(content_hbox), match_count_centerer,
                   FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(content_hbox), text_entry_, TRUE, TRUE, 0);

  // This event box is necessary to color in the area above and below the match
  // count label, and is where we draw the entry background onto in GTK mode.
  BuildBorder(content_hbox, 0, 0, 0, 0,
              &content_event_box_, &content_alignment_);
  gtk_widget_set_app_paintable(content_event_box_, TRUE);
  g_signal_connect(content_event_box_, "expose-event",
                   G_CALLBACK(OnContentEventBoxExpose), this);

  // This alignment isn't centered and is used for spacing in chrome theme
  // mode. (It's also used in GTK mode for padding because left padding doesn't
  // equal bottom padding naturally.)
  BuildBorder(content_event_box_, 2, 2, 2, 0,
              &border_bin_, &border_bin_alignment_);
  gtk_box_pack_end(GTK_BOX(hbox), border_bin_, TRUE, TRUE, 0);

  theme_service_->InitThemesFor(this);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));

  g_signal_connect(widget(), "parent-set", G_CALLBACK(OnParentSet), this);

  // We take care to avoid showing the slide animator widget.
  gtk_widget_show_all(container_);
  gtk_widget_show(widget());
}

FindBarController* FindBarGtk::GetFindBarController() const {
  return find_bar_controller_;
}

void FindBarGtk::SetFindBarController(FindBarController* find_bar_controller) {
  find_bar_controller_ = find_bar_controller;
}

void FindBarGtk::Show(bool animate) {
  if (animate) {
    slide_widget_->Open();
    selection_rect_ = gfx::Rect();
    Reposition();
    if (container_->window)
      gdk_window_raise(container_->window);
  } else {
    slide_widget_->OpenWithoutAnimation();
  }
}

void FindBarGtk::Hide(bool animate) {
  if (animate)
    slide_widget_->Close();
  else
    slide_widget_->CloseWithoutAnimation();
}

void FindBarGtk::SetFocusAndSelection() {
  StoreOutsideFocus();
  gtk_widget_grab_focus(text_entry_);
  // Select all the text.
  gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
}

void FindBarGtk::ClearResults(const FindNotificationDetails& results) {
  UpdateUIForFindResult(results, string16());
}

void FindBarGtk::StopAnimation() {
  slide_widget_->End();
}

void FindBarGtk::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // Not moving the window on demand, so do nothing.
}

void FindBarGtk::SetFindText(const string16& find_text) {
  std::string find_text_utf8 = UTF16ToUTF8(find_text);

  // Ignore the "changed" signal handler because programatically setting the
  // text should not fire a "changed" event.
  ignore_changed_signal_ = true;
  gtk_entry_set_text(GTK_ENTRY(text_entry_), find_text_utf8.c_str());
  ignore_changed_signal_ = false;
}

void FindBarGtk::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  if (!result.selection_rect().IsEmpty()) {
    selection_rect_ = result.selection_rect();
    int xposition = GetDialogPosition(result.selection_rect()).x();
    if (xposition != widget()->allocation.x)
      Reposition();
  }

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_store_.Store(NULL);

  std::string find_text_utf8 = UTF16ToUTF8(find_text);
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  std::string entry_text(gtk_entry_get_text(GTK_ENTRY(text_entry_)));
  if (entry_text != find_text_utf8) {
    SetFindText(find_text);
    gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
  }

  if (!find_text.empty() && have_valid_range) {
    gtk_label_set_text(GTK_LABEL(match_count_label_),
        l10n_util::GetStringFUTF8(IDS_FIND_IN_PAGE_COUNT,
            base::IntToString16(result.active_match_ordinal()),
            base::IntToString16(result.number_of_matches())).c_str());
    UpdateMatchLabelAppearance(result.number_of_matches() == 0 &&
                               result.final_update());
  } else {
    // If there was no text entered, we don't show anything in the result count
    // area.
    gtk_label_set_text(GTK_LABEL(match_count_label_), "");
    UpdateMatchLabelAppearance(false);
  }
}

void FindBarGtk::AudibleAlert() {
  // This call causes a lot of weird bugs, especially when using the custom
  // frame. TODO(estade): if people complain, re-enable it. See
  // http://crbug.com/27635 and others.
  //
  //   gtk_widget_error_bell(widget());
}

gfx::Rect FindBarGtk::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  bool ltr = !base::i18n::IsRTL();
  // 15 is the size of the scrollbar, copied from ScrollbarThemeChromium.
  // The height is not used.
  // At very low browser widths we can wind up with a negative |dialog_bounds|
  // width, so clamp it to 0.
  gfx::Rect dialog_bounds = gfx::Rect(ltr ? 0 : 15, 0,
      std::max(0, widget()->parent->allocation.width - (ltr ? 15 : 0)), 0);

  GtkRequisition req;
  gtk_widget_size_request(container_, &req);
  gfx::Size prefsize(req.width, req.height);

  gfx::Rect view_location(
      ltr ? dialog_bounds.width() - prefsize.width() : dialog_bounds.x(),
      dialog_bounds.y(), prefsize.width(), prefsize.height());
  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_location, dialog_bounds, avoid_overlapping_rect);

  return new_pos;
}

bool FindBarGtk::IsFindBarVisible() {
  return gtk_widget_get_visible(widget());
}

void FindBarGtk::RestoreSavedFocus() {
  // This function sometimes gets called when we don't have focus. We should do
  // nothing in this case.
  if (!gtk_widget_is_focus(text_entry_))
    return;

  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    find_bar_controller_->tab_contents()->tab_contents()->Focus();
}

FindBarTesting* FindBarGtk::GetFindBarTesting() {
  return this;
}

void FindBarGtk::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  // Force reshapings of the find bar window.
  container_width_ = -1;
  container_height_ = -1;

  if (theme_service_->UsingNativeTheme()) {
    gtk_widget_modify_cursor(text_entry_, NULL, NULL);
    gtk_widget_modify_base(text_entry_, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_text(text_entry_, GTK_STATE_NORMAL, NULL);

    // Prevent forced font sizes because it causes the jump up and down
    // character movement (http://crbug.com/22614), and because it will
    // prevent centering of the text entry.
    gtk_util::UndoForceFontSize(text_entry_);
    gtk_util::UndoForceFontSize(match_count_label_);

    gtk_widget_set_size_request(content_event_box_, -1, -1);
    gtk_widget_modify_bg(content_event_box_, GTK_STATE_NORMAL, NULL);

    // Replicate the normal GtkEntry behaviour by drawing the entry
    // background. We set the fake alignment to be the frame thickness.
    GtkStyle* style = gtk_rc_get_style(text_entry_);
    gint xborder = style->xthickness;
    gint yborder = style->ythickness;
    gtk_alignment_set_padding(GTK_ALIGNMENT(content_alignment_),
                              yborder, yborder, xborder, xborder);

    // We leave left padding on the left, even in GTK mode, as it's required
    // for the left margin to be equivalent to the bottom margin.
    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_alignment_),
                              0, 0, 1, 0);

    // We need this event box to have its own window in GTK mode for doing the
    // hacky widget rendering.
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(border_bin_), TRUE);
    gtk_widget_set_app_paintable(border_bin_, TRUE);

    gtk_misc_set_alignment(GTK_MISC(match_count_label_), 0.5, 0.5);
  } else {
    GdkColor gray = GDK_COLOR_RGB(0x7f, 0x7f, 0x7f);
    gtk_widget_modify_cursor(text_entry_, &ui::kGdkBlack, &gray);
    gtk_widget_modify_base(text_entry_, GTK_STATE_NORMAL,
                           &kEntryBackgroundColor);
    gtk_widget_modify_text(text_entry_, GTK_STATE_NORMAL, &kEntryTextColor);

    // Until we switch to vector graphics, force the font size.
    gtk_util::ForceFontSizePixels(text_entry_, 13.4);  // 13.4px == 10pt @ 96dpi
    gtk_util::ForceFontSizePixels(match_count_label_, 13.4);

    // Force the text widget height so it lines up with the buttons regardless
    // of font size.
    gtk_widget_set_size_request(content_event_box_, -1, 20);
    gtk_widget_modify_bg(content_event_box_, GTK_STATE_NORMAL,
                         &kEntryBackgroundColor);

    gtk_alignment_set_padding(GTK_ALIGNMENT(content_alignment_),
                              0.0, 0.0, 0.0, 0.0);

    gtk_alignment_set_padding(GTK_ALIGNMENT(border_bin_alignment_),
                              2, 2, 3, 0);

    // We need this event box to be invisible because we're only going to draw
    // on the background (but we can't take it out of the heiarchy entirely
    // because we also need it to take up space).
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(border_bin_), FALSE);
    gtk_widget_set_app_paintable(border_bin_, FALSE);

    gtk_misc_set_alignment(GTK_MISC(match_count_label_), 0.5, 1.0);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    close_button_->SetBackground(
        theme_service_->GetColor(ThemeService::COLOR_TAB_TEXT),
        rb.GetBitmapNamed(IDR_CLOSE_BAR),
        rb.GetBitmapNamed(IDR_CLOSE_BAR_MASK));
  }

  UpdateMatchLabelAppearance(match_label_failure_);
}

bool FindBarGtk::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  if (position)
    *position = GetPosition();

  if (fully_visible) {
    *fully_visible = !slide_widget_->IsAnimating() &&
                     slide_widget_->IsShowing();
  }
  return true;
}

string16 FindBarGtk::GetFindText() {
  std::string contents(gtk_entry_get_text(GTK_ENTRY(text_entry_)));
  return UTF8ToUTF16(contents);
}

string16 FindBarGtk::GetFindSelectedText() {
  gint cursor_pos;
  gint selection_bound;
  g_object_get(G_OBJECT(text_entry_), "cursor-position", &cursor_pos,
               NULL);
  g_object_get(G_OBJECT(text_entry_), "selection-bound", &selection_bound,
               NULL);
  std::string contents(gtk_entry_get_text(GTK_ENTRY(text_entry_)));
  return UTF8ToUTF16(contents.substr(cursor_pos, selection_bound));
}

string16 FindBarGtk::GetMatchCountText() {
  std::string contents(gtk_label_get_text(GTK_LABEL(match_count_label_)));
  return UTF8ToUTF16(contents);
}

int FindBarGtk::GetWidth() {
  return container_->allocation.width;
}

void FindBarGtk::FindEntryTextInContents(bool forward_search) {
  TabContentsWrapper* tab_contents = find_bar_controller_->tab_contents();
  if (!tab_contents)
    return;
  FindTabHelper* find_tab_helper = tab_contents->find_tab_helper();

  std::string new_contents(gtk_entry_get_text(GTK_ENTRY(text_entry_)));

  if (new_contents.length() > 0) {
    find_tab_helper->StartFinding(UTF8ToUTF16(new_contents), forward_search,
                               false);  // Not case sensitive.
  } else {
    // The textbox is empty so we reset.
    find_tab_helper->StopFinding(FindBarController::kClearSelection);
    UpdateUIForFindResult(find_tab_helper->find_result(), string16());

    // Clearing the text box should also clear the prepopulate state so that
    // when we close and reopen the Find box it doesn't show the search we
    // just deleted.
    FindBarState* find_bar_state = browser_->profile()->GetFindBarState();
    find_bar_state->set_last_prepopulate_text(string16());
  }
}

void FindBarGtk::UpdateMatchLabelAppearance(bool failure) {
  match_label_failure_ = failure;
  bool use_gtk = theme_service_->UsingNativeTheme();

  if (use_gtk) {
    GtkStyle* style = gtk_rc_get_style(text_entry_);
    GdkColor normal_bg = style->base[GTK_STATE_NORMAL];
    GdkColor normal_text = gtk_util::AverageColors(
        style->text[GTK_STATE_NORMAL], style->base[GTK_STATE_NORMAL]);

    gtk_widget_modify_bg(match_count_event_box_, GTK_STATE_NORMAL,
                         failure ? &kFindFailureBackgroundColor :
                         &normal_bg);
    gtk_widget_modify_fg(match_count_label_, GTK_STATE_NORMAL,
                         failure ? &kEntryTextColor : &normal_text);
  } else {
    gtk_widget_modify_bg(match_count_event_box_, GTK_STATE_NORMAL,
                         failure ? &kFindFailureBackgroundColor :
                         &kEntryBackgroundColor);
    gtk_widget_modify_fg(match_count_label_, GTK_STATE_NORMAL,
                         failure ? &kEntryTextColor : &kFindSuccessTextColor);
  }
}

void FindBarGtk::Reposition() {
  if (!IsFindBarVisible())
    return;

  // This will trigger an allocate, which allows us to reposition.
  if (widget()->parent)
    gtk_widget_queue_resize(widget()->parent);
}

void FindBarGtk::StoreOutsideFocus() {
  // |text_entry_| is the only widget in the find bar that can be focused,
  // so it's the only one we have to check.
  // TODO(estade): when we make the find bar buttons focusable, we'll have
  // to change this (same above in RestoreSavedFocus).
  if (!gtk_widget_is_focus(text_entry_))
    focus_store_.Store(text_entry_);
}

bool FindBarGtk::MaybeForwardKeyEventToRenderer(GdkEventKey* event) {
  switch (event->keyval) {
    case GDK_Down:
    case GDK_Up:
    case GDK_Page_Up:
    case GDK_Page_Down:
      break;
    case GDK_Home:
    case GDK_End:
      if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
          GDK_CONTROL_MASK) {
        break;
      }
    // Fall through.
    default:
      return false;
  }

  TabContentsWrapper* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();

  NativeWebKeyboardEvent wke(reinterpret_cast<GdkEvent*>(event));
  render_view_host->ForwardKeyboardEvent(wke);
  return true;
}

void FindBarGtk::AdjustTextAlignment() {
  PangoDirection content_dir =
      pango_find_base_dir(gtk_entry_get_text(GTK_ENTRY(text_entry_)), -1);

  GtkTextDirection widget_dir = gtk_widget_get_direction(text_entry_);

  // Use keymap or widget direction if content does not have strong direction.
  // It matches the behavior of GtkEntry.
  if (content_dir == PANGO_DIRECTION_NEUTRAL) {
    if (gtk_widget_has_focus(text_entry_)) {
      content_dir = gdk_keymap_get_direction(
        gdk_keymap_get_for_display(gtk_widget_get_display(text_entry_)));
    } else {
      if (widget_dir == GTK_TEXT_DIR_RTL)
        content_dir = PANGO_DIRECTION_RTL;
      else
        content_dir = PANGO_DIRECTION_LTR;
    }
  }

  if ((widget_dir == GTK_TEXT_DIR_RTL && content_dir == PANGO_DIRECTION_LTR) ||
      (widget_dir == GTK_TEXT_DIR_LTR && content_dir == PANGO_DIRECTION_RTL)) {
    gtk_entry_set_alignment(GTK_ENTRY(text_entry_), 1.0);
  } else {
    gtk_entry_set_alignment(GTK_ENTRY(text_entry_), 0.0);
  }
}

gfx::Point FindBarGtk::GetPosition() {
  gfx::Point point;

  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  gtk_container_child_get_property(GTK_CONTAINER(widget()->parent),
                                   widget(), "x", &value);
  point.set_x(g_value_get_int(&value));

  gtk_container_child_get_property(GTK_CONTAINER(widget()->parent),
                                   widget(), "y", &value);
  point.set_y(g_value_get_int(&value));

  g_value_unset(&value);

  return point;
}

// static
void FindBarGtk::OnParentSet(GtkWidget* widget, GtkObject* old_parent,
                             FindBarGtk* find_bar) {
  if (!widget->parent)
    return;

  g_signal_connect(widget->parent, "set-floating-position",
                   G_CALLBACK(OnSetFloatingPosition), find_bar);
}

// static
void FindBarGtk::OnSetFloatingPosition(GtkFloatingContainer* floating_container,
                                       GtkAllocation* allocation,
                                       FindBarGtk* find_bar) {
  GtkWidget* findbar = find_bar->widget();

  int xposition = find_bar->GetDialogPosition(find_bar->selection_rect_).x();

  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, xposition);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   findbar, "x", &value);

  g_value_set_int(&value, 0);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   findbar, "y", &value);
  g_value_unset(&value);
}

// static
gboolean FindBarGtk::OnChanged(GtkWindow* window, FindBarGtk* find_bar) {
  find_bar->AdjustTextAlignment();

  if (!find_bar->ignore_changed_signal_)
    find_bar->FindEntryTextInContents(true);

  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyPressEvent(GtkWidget* widget, GdkEventKey* event,
                                     FindBarGtk* find_bar) {
  if (find_bar->MaybeForwardKeyEventToRenderer(event)) {
    return TRUE;
  } else if (GDK_Escape == event->keyval) {
    find_bar->find_bar_controller_->EndFindSession(
        FindBarController::kKeepSelection);
    return TRUE;
  } else if (GDK_Return == event->keyval ||
             GDK_KP_Enter == event->keyval) {
    if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
        GDK_CONTROL_MASK) {
      find_bar->find_bar_controller_->EndFindSession(
          FindBarController::kActivateSelection);
      return TRUE;
    }

    bool forward = (event->state & gtk_accelerator_get_default_mod_mask()) !=
                   GDK_SHIFT_MASK;
    find_bar->FindEntryTextInContents(forward);
    return TRUE;
  }
  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       FindBarGtk* find_bar) {
  return find_bar->MaybeForwardKeyEventToRenderer(event);
}

void FindBarGtk::OnClicked(GtkWidget* button) {
  if (button == close_button_->widget()) {
    find_bar_controller_->EndFindSession(FindBarController::kKeepSelection);
  } else if (button == find_previous_button_->widget() ||
             button == find_next_button_->widget()) {
    FindEntryTextInContents(button == find_next_button_->widget());
  } else {
    NOTREACHED();
  }
}

// static
gboolean FindBarGtk::OnContentEventBoxExpose(GtkWidget* widget,
                                             GdkEventExpose* event,
                                             FindBarGtk* bar) {
  if (bar->theme_service_->UsingNativeTheme()) {
    // Draw the text entry background around where we input stuff. Note the
    // decrement to |width|. We do this because some theme engines
    // (*cough*Clearlooks*cough*) don't do any blending and use thickness to
    // make sure that widgets never overlap.
    int padding = gtk_widget_get_style(widget)->xthickness;
    GdkRectangle rec = {
      widget->allocation.x,
      widget->allocation.y,
      widget->allocation.width - padding,
      widget->allocation.height
    };

    gtk_util::DrawTextEntryBackground(bar->text_entry_, widget,
                                      &event->area, &rec);
  }

  return FALSE;
}

// Used to handle custom painting of |container_|.
gboolean FindBarGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                              FindBarGtk* bar) {
  if (bar->theme_service_->UsingNativeTheme()) {
    if (bar->container_width_ != widget->allocation.width ||
        bar->container_height_ != widget->allocation.height) {
      std::vector<GdkPoint> mask_points = MakeFramePolygonPoints(
          widget->allocation.width, widget->allocation.height, FRAME_MASK);
      GdkRegion* mask_region = gdk_region_polygon(&mask_points[0],
                                                  mask_points.size(),
                                                  GDK_EVEN_ODD_RULE);
      // Reset the shape.
      gdk_window_shape_combine_region(widget->window, NULL, 0, 0);
      gdk_window_shape_combine_region(widget->window, mask_region, 0, 0);
      gdk_region_destroy(mask_region);

      bar->container_width_ = widget->allocation.width;
      bar->container_height_ = widget->allocation.height;
    }

    GdkDrawable* drawable = GDK_DRAWABLE(e->window);
    GdkGC* gc = gdk_gc_new(drawable);
    gdk_gc_set_clip_rectangle(gc, &e->area);
    GdkColor color = bar->theme_service_->GetBorderColor();
    gdk_gc_set_rgb_fg_color(gc, &color);

    // Stroke the frame border.
    std::vector<GdkPoint> points = MakeFramePolygonPoints(
        widget->allocation.width, widget->allocation.height, FRAME_STROKE);
    gdk_draw_lines(drawable, gc, &points[0], points.size());

    g_object_unref(gc);
  } else {
    if (bar->container_width_ != widget->allocation.width ||
        bar->container_height_ != widget->allocation.height) {
      // Reset the shape.
      gdk_window_shape_combine_region(widget->window, NULL, 0, 0);
      SetDialogShape(bar->container_);

      bar->container_width_ = widget->allocation.width;
      bar->container_height_ = widget->allocation.height;
    }

    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    gdk_cairo_rectangle(cr, &e->area);
    cairo_clip(cr);

    gfx::Point tabstrip_origin =
        bar->window_->tabstrip()->GetTabStripOriginForWidget(widget);

    gtk_util::DrawThemedToolbarBackground(widget, cr, e, tabstrip_origin,
                                          bar->theme_service_);

    // During chrome theme mode, we need to draw the border around content_hbox
    // now instead of when we render |border_bin_|. We don't use stacked event
    // boxes to simulate the effect because we need to blend them with this
    // background.
    GtkAllocation border_allocation = bar->border_bin_->allocation;

    // Blit the left part of the background image once on the left.
    CairoCachedSurface* background_left =
        bar->theme_service_->GetRTLEnabledSurfaceNamed(
        IDR_FIND_BOX_BACKGROUND_LEFT, widget);
    background_left->SetSource(cr, border_allocation.x, border_allocation.y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, border_allocation.x, border_allocation.y,
                    background_left->Width(), background_left->Height());
    cairo_fill(cr);

    // Blit the center part of the background image in all the space between.
    CairoCachedSurface* background = bar->theme_service_->GetSurfaceNamed(
        IDR_FIND_BOX_BACKGROUND, widget);
    background->SetSource(cr,
                          border_allocation.x + background_left->Width(),
                          border_allocation.y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr,
                    border_allocation.x + background_left->Width(),
                    border_allocation.y,
                    border_allocation.width - background_left->Width(),
                    background->Height());
    cairo_fill(cr);

    cairo_destroy(cr);

    // Draw the border.
    GetDialogBorder()->RenderToWidget(widget);
  }

  // Propagate to the container's child.
  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);
  return TRUE;
}

// static
gboolean FindBarGtk::OnFocus(GtkWidget* text_entry, GtkDirectionType focus,
                             FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}

// static
gboolean FindBarGtk::OnButtonPress(GtkWidget* text_entry, GdkEventButton* e,
                                   FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}

// static
void FindBarGtk::OnMoveCursor(GtkEntry* entry, GtkMovementStep step, gint count,
                              gboolean selection, FindBarGtk* bar) {
  static guint signal_id = g_signal_lookup("move-cursor", GTK_TYPE_ENTRY);

  GdkEvent* event = gtk_get_current_event();
  if (event) {
    if ((event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE) &&
        bar->MaybeForwardKeyEventToRenderer(&(event->key))) {
      g_signal_stop_emission(entry, signal_id, 0);
    }

    gdk_event_free(event);
  }
}

void FindBarGtk::OnActivate(GtkWidget* entry) {
  FindEntryTextInContents(true);
}

gboolean FindBarGtk::OnFocusIn(GtkWidget* entry, GdkEventFocus* event) {
  g_signal_connect(gdk_keymap_get_for_display(gtk_widget_get_display(entry)),
                   "direction-changed",
                   G_CALLBACK(&OnKeymapDirectionChanged), this);

  AdjustTextAlignment();

  return FALSE;  // Continue propagation.
}

gboolean FindBarGtk::OnFocusOut(GtkWidget* entry, GdkEventFocus* event) {
  g_signal_handlers_disconnect_by_func(
      gdk_keymap_get_for_display(gtk_widget_get_display(entry)),
      reinterpret_cast<gpointer>(&OnKeymapDirectionChanged), this);

  return FALSE;  // Continue propagation.
}
