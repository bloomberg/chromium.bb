// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bar_gtk.h"

#include <vector>

#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/ntp_background_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_menu_controller_gtk.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_tree_model.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/hover_controller_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/ui/gtk/tabstrip_origin_provider.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/gtk_util.h"

namespace {

// The showing height of the bar.
const int kBookmarkBarHeight = 29;

// Padding for when the bookmark bar is floating.
const int kTopBottomNTPPadding = 12;
const int kLeftRightNTPPadding = 8;

// Padding around the bar's content area when the bookmark bar is floating.
const int kNTPPadding = 2;

// The number of pixels of rounding on the corners of the bookmark bar content
// area when in floating mode.
const int kNTPRoundedness = 3;

// The height of the bar when it is "hidden". It is usually not completely
// hidden because even when it is closed it forms the bottom few pixels of
// the toolbar.
const int kBookmarkBarMinimumHeight = 3;

// Left-padding for the instructional text.
const int kInstructionsPadding = 6;

// Padding around the "Other Bookmarks" button.
const int kOtherBookmarksPaddingHorizontal = 2;
const int kOtherBookmarksPaddingVertical = 1;

// The targets accepted by the toolbar and folder buttons for DnD.
const int kDestTargetList[] = { ui::CHROME_BOOKMARK_ITEM,
                                ui::CHROME_NAMED_URL,
                                ui::TEXT_URI_LIST,
                                ui::NETSCAPE_URL,
                                ui::TEXT_PLAIN, -1 };

// Acceptable drag actions for the bookmark bar drag destinations.
const GdkDragAction kDragAction =
    GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY);

void SetToolBarStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-bookmark-toolbar\" {"
      "  xthickness = 0\n"
      "  ythickness = 0\n"
      "  GtkWidget::focus-padding = 0\n"
      "  GtkContainer::border-width = 0\n"
      "  GtkToolbar::internal-padding = 1\n"
      "  GtkToolbar::shadow-type = GTK_SHADOW_NONE\n"
      "}\n"
      "widget \"*chrome-bookmark-toolbar\" style \"chrome-bookmark-toolbar\"");
}

void RecordAppLaunch(Profile* profile, const GURL& url) {
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                            extension_misc::APP_LAUNCH_BOOKMARK_BAR,
                            extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
}

}  // namespace

const int BookmarkBarGtk::kBookmarkBarNTPHeight = 57;

BookmarkBarGtk::BookmarkBarGtk(BrowserWindowGtk* window,
                               Profile* profile, Browser* browser,
                               TabstripOriginProvider* tabstrip_origin_provider)
    : profile_(NULL),
      page_navigator_(NULL),
      browser_(browser),
      window_(window),
      tabstrip_origin_provider_(tabstrip_origin_provider),
      model_(NULL),
      instructions_(NULL),
      sync_service_(NULL),
      dragged_node_(NULL),
      drag_icon_(NULL),
      toolbar_drop_item_(NULL),
      theme_service_(GtkThemeService::GetFrom(profile)),
      show_instructions_(true),
      menu_bar_helper_(this),
      slide_animation_(this),
      floating_(false),
      last_allocation_width_(-1),
      throbbing_widget_(NULL),
      method_factory_(this) {
  if (profile->GetProfileSyncService()) {
    // Obtain a pointer to the profile sync service and add our instance as an
    // observer.
    sync_service_ = profile->GetProfileSyncService();
    sync_service_->AddObserver(this);
  }

  Init(profile);
  SetProfile(profile);
  // Force an update by simulating being in the wrong state.
  floating_ = !ShouldBeFloating();
  UpdateFloatingState();

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());

  edit_bookmarks_enabled_.Init(prefs::kEditBookmarksEnabled,
                               profile_->GetPrefs(), this);
  OnEditBookmarksEnabledChanged();
}

BookmarkBarGtk::~BookmarkBarGtk() {
  RemoveAllBookmarkButtons();
  bookmark_toolbar_.Destroy();
  event_box_.Destroy();
}

void BookmarkBarGtk::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  RemoveAllBookmarkButtons();

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  // TODO(erg): Handle extensions

  model_ = profile_->GetBookmarkModel();
  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  // else case: we'll receive notification back from the BookmarkModel when done
  // loading, then we'll populate the bar.
}

void BookmarkBarGtk::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
}

void BookmarkBarGtk::Init(Profile* profile) {
  event_box_.Own(gtk_event_box_new());
  g_signal_connect(event_box_.get(), "destroy",
                   G_CALLBACK(&OnEventBoxDestroyThunk), this);
  g_signal_connect(event_box_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressedThunk), this);

  ntp_padding_box_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), ntp_padding_box_);

  paint_box_ = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(ntp_padding_box_), paint_box_);
  GdkColor paint_box_color =
      theme_service_->GetGdkColor(ThemeService::COLOR_TOOLBAR);
  gtk_widget_modify_bg(paint_box_, GTK_STATE_NORMAL, &paint_box_color);
  gtk_widget_add_events(paint_box_, GDK_POINTER_MOTION_MASK |
                                    GDK_BUTTON_PRESS_MASK);

  bookmark_hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(paint_box_), bookmark_hbox_);

  instructions_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(instructions_), 0, 0,
                            kInstructionsPadding, 0);
  instructions_gtk_.reset(new BookmarkBarInstructionsGtk(this, profile));
  gtk_container_add(GTK_CONTAINER(instructions_), instructions_gtk_->widget());
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), instructions_,
                     TRUE, TRUE, 0);

  gtk_drag_dest_set(instructions_,
      GtkDestDefaults(GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION),
      NULL, 0, kDragAction);
  ui::SetDestTargetList(instructions_, kDestTargetList);
  g_signal_connect(instructions_, "drag-data-received",
                   G_CALLBACK(&OnDragReceivedThunk), this);

  g_signal_connect(event_box_.get(), "expose-event",
                   G_CALLBACK(&OnEventBoxExposeThunk), this);
  UpdateEventBoxPaintability();

  bookmark_toolbar_.Own(gtk_toolbar_new());
  SetToolBarStyle();
  gtk_widget_set_name(bookmark_toolbar_.get(), "chrome-bookmark-toolbar");
  gtk_util::SuppressDefaultPainting(bookmark_toolbar_.get());
  g_signal_connect(bookmark_toolbar_.get(), "size-allocate",
                   G_CALLBACK(&OnToolbarSizeAllocateThunk), this);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), bookmark_toolbar_.get(),
                     TRUE, TRUE, 0);

  overflow_button_ = theme_service_->BuildChromeButton();
  g_object_set_data(G_OBJECT(overflow_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  SetOverflowButtonAppearance();
  ConnectFolderButtonEvents(overflow_button_, false);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), overflow_button_,
                     FALSE, FALSE, 0);

  gtk_drag_dest_set(bookmark_toolbar_.get(), GTK_DEST_DEFAULT_DROP,
                    NULL, 0, kDragAction);
  ui::SetDestTargetList(bookmark_toolbar_.get(), kDestTargetList);
  g_signal_connect(bookmark_toolbar_.get(), "drag-motion",
                   G_CALLBACK(&OnToolbarDragMotionThunk), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-leave",
                   G_CALLBACK(&OnDragLeaveThunk), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-data-received",
                   G_CALLBACK(&OnDragReceivedThunk), this);

  GtkWidget* vseparator = theme_service_->CreateToolbarSeparator();
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), vseparator,
                     FALSE, FALSE, 0);

  // We pack the button manually (rather than using gtk_button_set_*) so that
  // we can have finer control over its label.
  other_bookmarks_button_ = theme_service_->BuildChromeButton();
  ConnectFolderButtonEvents(other_bookmarks_button_, false);
  GtkWidget* other_padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(other_padding),
                            kOtherBookmarksPaddingVertical,
                            kOtherBookmarksPaddingVertical,
                            kOtherBookmarksPaddingHorizontal,
                            kOtherBookmarksPaddingHorizontal);
  gtk_container_add(GTK_CONTAINER(other_padding), other_bookmarks_button_);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), other_padding,
                     FALSE, FALSE, 0);

  sync_error_button_ = theme_service_->BuildChromeButton();
  gtk_button_set_image(
      GTK_BUTTON(sync_error_button_),
      gtk_image_new_from_pixbuf(
          ResourceBundle::GetSharedInstance().GetPixbufNamed(IDR_WARNING)));
  g_signal_connect(sync_error_button_, "button-press-event",
                   G_CALLBACK(OnSyncErrorButtonPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), sync_error_button_,
                     FALSE, FALSE, 0);

  gtk_widget_set_size_request(event_box_.get(), -1, kBookmarkBarMinimumHeight);

  ViewIDUtil::SetID(other_bookmarks_button_, VIEW_ID_OTHER_BOOKMARKS);
  ViewIDUtil::SetID(widget(), VIEW_ID_BOOKMARK_BAR);

  gtk_widget_show_all(widget());
  gtk_widget_hide(widget());
}

void BookmarkBarGtk::Show(bool animate) {
  gtk_widget_show_all(widget());
  bool old_floating = floating_;
  UpdateFloatingState();
  // TODO(estade): animate the transition between floating and non.
  animate = animate && (old_floating == floating_);
  if (animate) {
    slide_animation_.Show();
  } else {
    slide_animation_.Reset(1);
    AnimationProgressed(&slide_animation_);
  }

  // Hide out behind the findbar. This is rather fragile code, it could
  // probably be improved.
  if (floating_) {
    if (theme_service_->UseGtkTheme()) {
      if (GTK_WIDGET_REALIZED(event_box_->parent))
        gdk_window_lower(event_box_->parent->window);
      if (GTK_WIDGET_REALIZED(event_box_.get()))
        gdk_window_lower(event_box_->window);
    } else {  // Chromium theme mode.
      if (GTK_WIDGET_REALIZED(paint_box_)) {
        gdk_window_lower(paint_box_->window);
        // The event box won't stay below its children's GdkWindows unless we
        // toggle the above-child property here. If the event box doesn't stay
        // below its children then events will be routed to it rather than the
        // children.
        gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box_.get()), TRUE);
        gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box_.get()), FALSE);
      }
    }
  }

  if (sync_ui_util::ShouldShowSyncErrorButton(sync_service_)) {
    gtk_widget_show(sync_error_button_);
  } else {
    gtk_widget_hide(sync_error_button_);
  }

  // Maybe show the instructions
  if (show_instructions_) {
    gtk_widget_hide(bookmark_toolbar_.get());
    gtk_widget_show(instructions_);
  } else {
    gtk_widget_hide(instructions_);
    gtk_widget_show(bookmark_toolbar_.get());
  }

  SetChevronState();
}

void BookmarkBarGtk::Hide(bool animate) {
  UpdateFloatingState();

  // After coming out of fullscreen, the browser window sets the bookmark bar
  // to the "hidden" state, which means we need to show our minimum height.
  gtk_widget_show(widget());
  // Sometimes we get called without a matching call to open. If that happens
  // then force hide.
  if (slide_animation_.IsShowing() && animate) {
    slide_animation_.Hide();
  } else {
    gtk_widget_hide(bookmark_hbox_);
    slide_animation_.Reset(0);
    AnimationProgressed(&slide_animation_);
  }
}

void BookmarkBarGtk::OnStateChanged() {
  if (sync_ui_util::ShouldShowSyncErrorButton(sync_service_)) {
    gtk_widget_show(sync_error_button_);
  } else {
    gtk_widget_hide(sync_error_button_);
  }
}

void BookmarkBarGtk::ShowImportDialog() {
  browser_->OpenImportSettingsDialog();
}

void BookmarkBarGtk::EnterFullscreen() {
  if (ShouldBeFloating())
    Show(false);
  else
    gtk_widget_hide(widget());
}

int BookmarkBarGtk::GetHeight() {
  return event_box_->allocation.height - kBookmarkBarMinimumHeight;
}

bool BookmarkBarGtk::IsAnimating() {
  return slide_animation_.is_animating();
}

bool BookmarkBarGtk::OnNewTabPage() {
  return (browser_ && browser_->GetSelectedTabContents() &&
          browser_->GetSelectedTabContents()->ShouldShowBookmarkBar());
}

void BookmarkBarGtk::Loaded(BookmarkModel* model) {
  // If |instructions_| has been nulled, we are in the middle of browser
  // shutdown. Do nothing.
  if (!instructions_)
    return;

  RemoveAllBookmarkButtons();
  CreateAllBookmarkButtons();
}

void BookmarkBarGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // The bookmark model should never be deleted before us. This code exists
  // to check for regressions in shutdown code and not crash.
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers())
    NOTREACHED();

  // Do minimal cleanup, presumably we'll be deleted shortly.
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkBarGtk::BookmarkNodeMoved(BookmarkModel* model,
                                       const BookmarkNode* old_parent,
                                       int old_index,
                                       const BookmarkNode* new_parent,
                                       int new_index) {
  const BookmarkNode* node = new_parent->GetChild(new_index);
  BookmarkNodeRemoved(model, old_parent, old_index, node);
  BookmarkNodeAdded(model, new_parent, new_index);
}

void BookmarkBarGtk::BookmarkNodeAdded(BookmarkModel* model,
                                       const BookmarkNode* parent,
                                       int index) {
  const BookmarkNode* node = parent->GetChild(index);
  if (parent != model_->GetBookmarkBarNode()) {
    StartThrobbing(node);
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());

  GtkToolItem* item = CreateBookmarkToolItem(node);
  gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()),
                     item, index);
  if (node->is_folder())
    menu_bar_helper_.Add(gtk_bin_get_child(GTK_BIN(item)));

  SetInstructionState();
  SetChevronState();

  StartThrobbingAfterAllocation(GTK_WIDGET(item));
}

void BookmarkBarGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                         const BookmarkNode* parent,
                                         int old_index,
                                         const BookmarkNode* node) {
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(old_index >= 0 && old_index < GetBookmarkButtonCount());

  GtkWidget* to_remove = GTK_WIDGET(gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), old_index));
  if (node->is_folder())
    menu_bar_helper_.Remove(gtk_bin_get_child(GTK_BIN(to_remove)));
  gtk_container_remove(GTK_CONTAINER(bookmark_toolbar_.get()),
                       to_remove);

  SetInstructionState();
  SetChevronState();
}

void BookmarkBarGtk::BookmarkNodeChanged(BookmarkModel* model,
                                         const BookmarkNode* node) {
  if (node->parent() != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->GetBookmarkBarNode()->GetIndexOf(node);
  DCHECK(index != -1);

  GtkToolItem* item = gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), index);
  GtkWidget* button = gtk_bin_get_child(GTK_BIN(item));
  bookmark_utils::ConfigureButtonForNode(node, model, button, theme_service_);
  SetChevronState();
}

void BookmarkBarGtk::BookmarkNodeFaviconLoaded(BookmarkModel* model,
                                               const BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void BookmarkBarGtk::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  if (node != model_->GetBookmarkBarNode())
    return;  // We only care about reordering of the bookmark bar node.

  // Purge and rebuild the bar.
  RemoveAllBookmarkButtons();
  CreateAllBookmarkButtons();
}

void BookmarkBarGtk::CreateAllBookmarkButtons() {
  const BookmarkNode* bar = model_->GetBookmarkBarNode();
  DCHECK(bar && model_->other_node());

  // Create a button for each of the children on the bookmark bar.
  for (int i = 0; i < bar->child_count(); ++i) {
    const BookmarkNode* node = bar->GetChild(i);
    GtkToolItem* item = CreateBookmarkToolItem(node);
    gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()), item, -1);
    if (node->is_folder())
      menu_bar_helper_.Add(gtk_bin_get_child(GTK_BIN(item)));
  }

  bookmark_utils::ConfigureButtonForNode(model_->other_node(),
      model_, other_bookmarks_button_, theme_service_);

  SetInstructionState();
  SetChevronState();
}

void BookmarkBarGtk::SetInstructionState() {
  show_instructions_ = (model_->GetBookmarkBarNode()->child_count() == 0);
  if (show_instructions_) {
    gtk_widget_hide(bookmark_toolbar_.get());
    gtk_widget_show_all(instructions_);
  } else {
    gtk_widget_hide(instructions_);
    gtk_widget_show(bookmark_toolbar_.get());
  }
}

void BookmarkBarGtk::SetChevronState() {
  if (!GTK_WIDGET_VISIBLE(bookmark_hbox_))
    return;

  if (show_instructions_) {
    gtk_widget_hide(overflow_button_);
    return;
  }

  int extra_space = 0;
  if (GTK_WIDGET_VISIBLE(overflow_button_))
    extra_space = overflow_button_->allocation.width;

  int overflow_idx = GetFirstHiddenBookmark(extra_space, NULL);
  if (overflow_idx == -1)
    gtk_widget_hide(overflow_button_);
  else
    gtk_widget_show_all(overflow_button_);
}

void BookmarkBarGtk::RemoveAllBookmarkButtons() {
  gtk_util::RemoveAllChildren(bookmark_toolbar_.get());
  menu_bar_helper_.Clear();
  menu_bar_helper_.Add(other_bookmarks_button_);
  menu_bar_helper_.Add(overflow_button_);
}

int BookmarkBarGtk::GetBookmarkButtonCount() {
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_toolbar_.get()));
  int count = g_list_length(children);
  g_list_free(children);
  return count;
}

void BookmarkBarGtk::SetOverflowButtonAppearance() {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(overflow_button_));
  if (former_child)
    gtk_widget_destroy(former_child);

  GtkWidget* new_child = theme_service_->UseGtkTheme() ?
      gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE) :
      gtk_image_new_from_pixbuf(ResourceBundle::GetSharedInstance().
          GetRTLEnabledPixbufNamed(IDR_BOOKMARK_BAR_CHEVRONS));

  gtk_container_add(GTK_CONTAINER(overflow_button_), new_child);
  SetChevronState();
}

int BookmarkBarGtk::GetFirstHiddenBookmark(
    int extra_space, std::vector<GtkWidget*>* showing_folders) {
  int rv = 0;
  bool overflow = false;
  GList* toolbar_items =
      gtk_container_get_children(GTK_CONTAINER(bookmark_toolbar_.get()));
  for (GList* iter = toolbar_items; iter; iter = g_list_next(iter)) {
    GtkWidget* tool_item = reinterpret_cast<GtkWidget*>(iter->data);
    if (gtk_widget_get_direction(tool_item) == GTK_TEXT_DIR_RTL) {
      overflow = (tool_item->allocation.x + tool_item->style->xthickness <
                  bookmark_toolbar_.get()->allocation.x - extra_space);
    } else {
      overflow =
        (tool_item->allocation.x + tool_item->allocation.width +
         tool_item->style->xthickness >
         bookmark_toolbar_.get()->allocation.width +
         bookmark_toolbar_.get()->allocation.x + extra_space);
    }
    overflow = overflow || tool_item->allocation.x == -1;

    if (overflow)
      break;

    if (showing_folders &&
        model_->GetBookmarkBarNode()->GetChild(rv)->is_folder()) {
      showing_folders->push_back(gtk_bin_get_child(GTK_BIN(tool_item)));
    }
    rv++;
  }

  g_list_free(toolbar_items);

  if (!overflow)
    return -1;

  return rv;
}

bool BookmarkBarGtk::ShouldBeFloating() {
  // NTP4 never floats the bookmark bar.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewTabPage4))
    return false;

  return (!IsAlwaysShown() || (window_ && window_->IsFullscreen())) &&
      window_ && window_->GetDisplayedTabContents() &&
      window_->GetDisplayedTabContents()->ShouldShowBookmarkBar();
}

void BookmarkBarGtk::UpdateFloatingState() {
  bool old_floating = floating_;
  floating_ = ShouldBeFloating();
  if (floating_ == old_floating)
    return;

  if (floating_) {
#if !defined(OS_CHROMEOS)
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(paint_box_), TRUE);
#endif
    GdkColor stroke_color = theme_service_->UseGtkTheme() ?
        theme_service_->GetBorderColor() :
        theme_service_->GetGdkColor(ThemeService::COLOR_NTP_HEADER);
    gtk_util::ActAsRoundedWindow(paint_box_, stroke_color, kNTPRoundedness,
                                 gtk_util::ROUNDED_ALL, gtk_util::BORDER_ALL);

    gtk_alignment_set_padding(GTK_ALIGNMENT(ntp_padding_box_),
        kTopBottomNTPPadding, kTopBottomNTPPadding,
        kLeftRightNTPPadding, kLeftRightNTPPadding);
    gtk_container_set_border_width(GTK_CONTAINER(bookmark_hbox_), kNTPPadding);
  } else {
    gtk_util::StopActingAsRoundedWindow(paint_box_);
#if !defined(OS_CHROMEOS)
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(paint_box_), FALSE);
#endif
    gtk_alignment_set_padding(GTK_ALIGNMENT(ntp_padding_box_), 0, 0, 0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(bookmark_hbox_), 0);
  }

  UpdateEventBoxPaintability();
  // |window_| can be NULL during testing.
  if (window_) {
    window_->BookmarkBarIsFloating(floating_);

    // Listen for parent size allocations.
    if (floating_ && widget()->parent) {
      // Only connect once.
      if (g_signal_handler_find(widget()->parent, G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, reinterpret_cast<gpointer>(OnParentSizeAllocateThunk),
          NULL) == 0) {
        g_signal_connect(widget()->parent, "size-allocate",
                         G_CALLBACK(OnParentSizeAllocateThunk), this);
      }
    }
  }
}

void BookmarkBarGtk::UpdateEventBoxPaintability() {
  gtk_widget_set_app_paintable(event_box_.get(),
                               !theme_service_->UseGtkTheme() || floating_);
  // When using the GTK+ theme, we need to have the event box be visible so
  // buttons don't get a halo color from the background.  When using Chromium
  // themes, we want to let the background show through the toolbar.

#if !defined(OS_CHROMEOS)
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()),
                                   theme_service_->UseGtkTheme());
#endif
}

void BookmarkBarGtk::PaintEventBox() {
  gfx::Size tab_contents_size;
  if (GetTabContentsSize(&tab_contents_size) &&
      tab_contents_size != last_tab_contents_size_) {
    last_tab_contents_size_ = tab_contents_size;
    gtk_widget_queue_draw(event_box_.get());
  }
}

bool BookmarkBarGtk::GetTabContentsSize(gfx::Size* size) {
  Browser* browser = browser_;
  if (!browser) {
    NOTREACHED();
    return false;
  }
  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents) {
    // It is possible to have a browser but no TabContents while under testing,
    // so don't NOTREACHED() and error the program.
    return false;
  }
  if (!tab_contents->view()) {
    NOTREACHED();
    return false;
  }
  *size = tab_contents->view()->GetContainerSize();
  return true;
}

void BookmarkBarGtk::StartThrobbing(const BookmarkNode* node) {
  const BookmarkNode* parent_on_bb = NULL;
  for (const BookmarkNode* parent = node; parent;
       parent = parent->parent()) {
    if (parent->parent() == model_->GetBookmarkBarNode()) {
      parent_on_bb = parent;
      break;
    }
  }

  GtkWidget* widget_to_throb = NULL;

  if (!parent_on_bb) {
    // Descendant of "Other Bookmarks".
    widget_to_throb = other_bookmarks_button_;
  } else {
    int hidden = GetFirstHiddenBookmark(0, NULL);
    int idx = model_->GetBookmarkBarNode()->GetIndexOf(parent_on_bb);

    if (hidden >= 0 && hidden <= idx) {
      widget_to_throb = overflow_button_;
    } else {
      widget_to_throb = gtk_bin_get_child(GTK_BIN(gtk_toolbar_get_nth_item(
          GTK_TOOLBAR(bookmark_toolbar_.get()), idx)));
    }
  }

  SetThrobbingWidget(widget_to_throb);
}

void BookmarkBarGtk::SetThrobbingWidget(GtkWidget* widget) {
  if (throbbing_widget_) {
    HoverControllerGtk* hover_controller =
        HoverControllerGtk::GetHoverControllerGtk(throbbing_widget_);
    if (hover_controller)
      hover_controller->StartThrobbing(0);

    g_signal_handlers_disconnect_by_func(
        throbbing_widget_,
        reinterpret_cast<gpointer>(OnThrobbingWidgetDestroyThunk),
        this);
    g_object_unref(throbbing_widget_);
    throbbing_widget_ = NULL;
  }

  if (widget) {
    throbbing_widget_ = widget;
    g_object_ref(throbbing_widget_);
    g_signal_connect(throbbing_widget_, "destroy",
                     G_CALLBACK(OnThrobbingWidgetDestroyThunk), this);

    HoverControllerGtk* hover_controller =
        HoverControllerGtk::GetHoverControllerGtk(throbbing_widget_);
    if (hover_controller)
      hover_controller->StartThrobbing(4);
  }
}

void BookmarkBarGtk::OnItemAllocate(GtkWidget* item,
                                    GtkAllocation* allocation) {
  // We only want to fire on the item's first allocation.
  g_signal_handlers_disconnect_by_func(
      item, reinterpret_cast<gpointer>(&OnItemAllocateThunk), this);

  GtkWidget* button = gtk_bin_get_child(GTK_BIN(item));
  const BookmarkNode* node = GetNodeForToolButton(button);
  if (node)
    StartThrobbing(node);
}

void BookmarkBarGtk::StartThrobbingAfterAllocation(GtkWidget* item) {
  g_signal_connect_after(
      item, "size-allocate", G_CALLBACK(OnItemAllocateThunk), this);
}

bool BookmarkBarGtk::IsAlwaysShown() {
  return (profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) &&
          profile_->GetPrefs()->GetBoolean(prefs::kEnableBookmarkBar));
}

void BookmarkBarGtk::AnimationProgressed(const ui::Animation* animation) {
  DCHECK_EQ(animation, &slide_animation_);

  int max_height = ShouldBeFloating() ?
                   kBookmarkBarNTPHeight : kBookmarkBarHeight;
  gint height =
      static_cast<gint>(animation->GetCurrentValue() *
                        (max_height - kBookmarkBarMinimumHeight)) +
      kBookmarkBarMinimumHeight;
  gtk_widget_set_size_request(event_box_.get(), -1, height);
}

void BookmarkBarGtk::AnimationEnded(const ui::Animation* animation) {
  DCHECK_EQ(animation, &slide_animation_);

  if (!slide_animation_.IsShowing()) {
    gtk_widget_hide(bookmark_hbox_);

    // We can be windowless during unit tests.
    if (window_) {
      // Because of our constant resizing and our toolbar/bookmark bar overlap
      // shenanigans, gtk+ gets confused, partially draws parts of the bookmark
      // bar into the toolbar and than doesn't queue a redraw to fix it. So do
      // it manually by telling the toolbar area to redraw itself.
      window_->QueueToolbarRedraw();
    }
  }
}

void BookmarkBarGtk::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED) {
    if (model_ && model_->IsLoaded()) {
      // Regenerate the bookmark bar with all new objects with their theme
      // properties set correctly for the new theme.
      RemoveAllBookmarkButtons();
      CreateAllBookmarkButtons();
    }

    UpdateEventBoxPaintability();

    GdkColor paint_box_color =
        theme_service_->GetGdkColor(ThemeService::COLOR_TOOLBAR);
    gtk_widget_modify_bg(paint_box_, GTK_STATE_NORMAL, &paint_box_color);

    if (floating_) {
      GdkColor stroke_color = theme_service_->UseGtkTheme() ?
          theme_service_->GetBorderColor() :
          theme_service_->GetGdkColor(ThemeService::COLOR_NTP_HEADER);
      gtk_util::SetRoundedWindowBorderColor(paint_box_, stroke_color);
    }

    SetOverflowButtonAppearance();
  } else if (type == NotificationType::PREF_CHANGED) {
    const std::string& pref_name = *Details<std::string>(details).ptr();
    if (pref_name == prefs::kEditBookmarksEnabled)
      OnEditBookmarksEnabledChanged();
  }
}

GtkWidget* BookmarkBarGtk::CreateBookmarkButton(const BookmarkNode* node) {
  GtkWidget* button = theme_service_->BuildChromeButton();
  bookmark_utils::ConfigureButtonForNode(node, model_, button, theme_service_);

  // The tool item is also a source for dragging
  gtk_drag_source_set(button, GDK_BUTTON1_MASK, NULL, 0,
      static_cast<GdkDragAction>(GDK_ACTION_MOVE | GDK_ACTION_COPY));
  int target_mask = bookmark_utils::GetCodeMask(node->is_folder());
  ui::SetSourceTargetListFromCodeMask(button, target_mask);
  g_signal_connect(button, "drag-begin",
                   G_CALLBACK(&OnButtonDragBeginThunk), this);
  g_signal_connect(button, "drag-end",
                   G_CALLBACK(&OnButtonDragEndThunk), this);
  g_signal_connect(button, "drag-data-get",
                   G_CALLBACK(&OnButtonDragGetThunk), this);
  // We deliberately don't connect to "drag-data-delete" because the action of
  // moving a button will regenerate all the contents of the bookmarks bar
  // anyway.

  if (node->is_url()) {
    // Connect to 'button-release-event' instead of 'clicked' because we need
    // access to the modifier keys and we do different things on each
    // button.
    g_signal_connect(button, "button-press-event",
                     G_CALLBACK(OnButtonPressedThunk), this);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(OnClickedThunk), this);
    gtk_util::SetButtonTriggersNavigation(button);
  } else {
    ConnectFolderButtonEvents(button, true);
  }

  return button;
}

GtkToolItem* BookmarkBarGtk::CreateBookmarkToolItem(const BookmarkNode* node) {
  GtkWidget* button = CreateBookmarkButton(node);
  g_object_set_data(G_OBJECT(button), "left-align-popup",
                    reinterpret_cast<void*>(true));

  GtkToolItem* item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(item), button);
  gtk_widget_show_all(GTK_WIDGET(item));

  return item;
}

void BookmarkBarGtk::ConnectFolderButtonEvents(GtkWidget* widget,
                                               bool is_tool_item) {
  // For toolbar items (i.e. not the overflow button or other bookmarks
  // button), we handle motion and highlighting manually.
  gtk_drag_dest_set(widget,
                    is_tool_item ? GTK_DEST_DEFAULT_DROP :
                                   GTK_DEST_DEFAULT_ALL,
                    NULL,
                    0,
                    kDragAction);
  ui::SetDestTargetList(widget, kDestTargetList);
  g_signal_connect(widget, "drag-data-received",
                   G_CALLBACK(&OnDragReceivedThunk), this);
  if (is_tool_item) {
    g_signal_connect(widget, "drag-motion",
                     G_CALLBACK(&OnFolderDragMotionThunk), this);
    g_signal_connect(widget, "drag-leave",
                     G_CALLBACK(&OnDragLeaveThunk), this);
  }

  g_signal_connect(widget, "button-press-event",
                   G_CALLBACK(OnButtonPressedThunk), this);
  g_signal_connect(widget, "clicked",
                   G_CALLBACK(OnFolderClickedThunk), this);

  // Accept middle mouse clicking (which opens all). This must be called after
  // connecting to "button-press-event" because the handler it attaches stops
  // the propagation of that signal.
  gtk_util::SetButtonClickableByMouseButtons(widget, true, true, false);
}

const BookmarkNode* BookmarkBarGtk::GetNodeForToolButton(GtkWidget* widget) {
  // First check to see if |button| is special cased.
  if (widget == other_bookmarks_button_)
    return model_->other_node();
  else if (widget == event_box_.get() || widget == overflow_button_)
    return model_->GetBookmarkBarNode();

  // Search the contents of |bookmark_toolbar_| for the corresponding widget
  // and find its index.
  GtkWidget* item_to_find = gtk_widget_get_parent(widget);
  int index_to_use = -1;
  int index = 0;
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_toolbar_.get()));
  for (GList* item = children; item; item = item->next, index++) {
    if (item->data == item_to_find) {
      index_to_use = index;
      break;
    }
  }
  g_list_free(children);

  if (index_to_use != -1)
    return model_->GetBookmarkBarNode()->GetChild(index_to_use);

  return NULL;
}

void BookmarkBarGtk::PopupMenuForNode(GtkWidget* sender,
                                      const BookmarkNode* node,
                                      GdkEventButton* event) {
  if (!model_->IsLoaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = NULL;
  std::vector<const BookmarkNode*> nodes;
  if (sender == other_bookmarks_button_) {
    nodes.push_back(node);
    parent = model_->GetBookmarkBarNode();
  } else if (sender != bookmark_toolbar_.get()) {
    nodes.push_back(node);
    parent = node->parent();
  } else {
    parent = model_->GetBookmarkBarNode();
    nodes.push_back(parent);
  }

  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(sender));
  current_context_menu_controller_.reset(
      new BookmarkContextMenuController(
          window, this, profile_, page_navigator_, parent, nodes));
  current_context_menu_.reset(
      new MenuGtk(NULL, current_context_menu_controller_->menu_model()));
  current_context_menu_->PopupAsContext(
      gfx::Point(event->x_root, event->y_root),
      event->time);
}

gboolean BookmarkBarGtk::OnButtonPressed(GtkWidget* sender,
                                         GdkEventButton* event) {
  last_pressed_coordinates_ = gfx::Point(event->x, event->y);

  if (event->button == 3 && GTK_WIDGET_VISIBLE(bookmark_hbox_)) {
    const BookmarkNode* node = GetNodeForToolButton(sender);
    DCHECK(node);
    DCHECK(page_navigator_);
    PopupMenuForNode(sender, node, event);
  }

  return FALSE;
}

gboolean BookmarkBarGtk::OnSyncErrorButtonPressed(GtkWidget* sender,
                                                  GdkEventButton* event) {
  if (sender == sync_error_button_) {
    DCHECK(sync_service_ && !sync_service_->IsManaged());
    sync_service_->ShowErrorUI(NULL);
  }

  return FALSE;
}

void BookmarkBarGtk::OnClicked(GtkWidget* sender) {
  const BookmarkNode* node = GetNodeForToolButton(sender);
  DCHECK(node);
  DCHECK(node->is_url());
  DCHECK(page_navigator_);

  RecordAppLaunch(profile_, node->GetURL());
  page_navigator_->OpenURL(
      node->GetURL(), GURL(),
      gtk_util::DispositionForCurrentButtonPressEvent(),
      PageTransition::AUTO_BOOKMARK);

  UserMetrics::RecordAction(UserMetricsAction("ClickedBookmarkBarURLButton"),
                            profile_);
}

void BookmarkBarGtk::OnButtonDragBegin(GtkWidget* button,
                                       GdkDragContext* drag_context) {
  // The parent tool item might be removed during the drag. Ref it so |button|
  // won't get destroyed.
  g_object_ref(button->parent);

  const BookmarkNode* node = GetNodeForToolButton(button);
  DCHECK(!dragged_node_);
  dragged_node_ = node;
  DCHECK(dragged_node_);

  drag_icon_ = bookmark_utils::GetDragRepresentationForNode(
      node, model_, theme_service_);

  // We have to jump through some hoops to get the drag icon to line up because
  // it is a different size than the button.
  GtkRequisition req;
  gtk_widget_size_request(drag_icon_, &req);
  gfx::Rect button_rect = gtk_util::WidgetBounds(button);
  gfx::Point drag_icon_relative =
      gfx::Rect(req.width, req.height).CenterPoint().Add(
          (last_pressed_coordinates_.Subtract(button_rect.CenterPoint())));
  gtk_drag_set_icon_widget(drag_context, drag_icon_,
                           drag_icon_relative.x(),
                           drag_icon_relative.y());

  // Hide our node, but reserve space for it on the toolbar.
  int index = gtk_toolbar_get_item_index(GTK_TOOLBAR(bookmark_toolbar_.get()),
                                         GTK_TOOL_ITEM(button->parent));
  gtk_widget_hide(button);
  toolbar_drop_item_ = CreateBookmarkToolItem(dragged_node_);
  g_object_ref_sink(GTK_OBJECT(toolbar_drop_item_));
  gtk_toolbar_set_drop_highlight_item(GTK_TOOLBAR(bookmark_toolbar_.get()),
                                      GTK_TOOL_ITEM(toolbar_drop_item_), index);
  // Make sure it stays hidden for the duration of the drag.
  gtk_widget_set_no_show_all(button, TRUE);
}

void BookmarkBarGtk::OnButtonDragEnd(GtkWidget* button,
                                     GdkDragContext* drag_context) {
  gtk_widget_show(button);
  gtk_widget_set_no_show_all(button, FALSE);

  ClearToolbarDropHighlighting();

  DCHECK(dragged_node_);
  dragged_node_ = NULL;

  DCHECK(drag_icon_);
  gtk_widget_destroy(drag_icon_);
  drag_icon_ = NULL;

  g_object_unref(button->parent);
}

void BookmarkBarGtk::OnButtonDragGet(GtkWidget* widget, GdkDragContext* context,
                                     GtkSelectionData* selection_data,
                                     guint target_type, guint time) {
  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(widget);
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           profile_);
}

void BookmarkBarGtk::OnFolderClicked(GtkWidget* sender) {
  // Stop its throbbing, if any.
  HoverControllerGtk* hover_controller =
      HoverControllerGtk::GetHoverControllerGtk(sender);
  if (hover_controller)
    hover_controller->StartThrobbing(0);

  GdkEvent* event = gtk_get_current_event();
  if (event->button.button == 1) {
    PopupForButton(sender);
  } else if (event->button.button == 2) {
    const BookmarkNode* node = GetNodeForToolButton(sender);
    bookmark_utils::OpenAll(window_->GetNativeHandle(),
                            profile_, page_navigator_,
                            node, NEW_BACKGROUND_TAB);
  }
}

gboolean BookmarkBarGtk::ItemDraggedOverToolbar(GdkDragContext* context,
                                                int index,
                                                guint time) {
  if (!edit_bookmarks_enabled_.GetValue())
    return FALSE;
  GdkAtom target_type =
      gtk_drag_dest_find_target(bookmark_toolbar_.get(), context, NULL);
  if (target_type == GDK_NONE) {
    // We shouldn't act like a drop target when something that we can't deal
    // with is dragged over the toolbar.
    return FALSE;
  }

  if (!toolbar_drop_item_) {
    if (dragged_node_) {
      toolbar_drop_item_ = CreateBookmarkToolItem(dragged_node_);
      g_object_ref_sink(GTK_OBJECT(toolbar_drop_item_));
    } else {
      // Create a fake item the size of other_node().
      //
      // TODO(erg): Maybe somehow figure out the real size for the drop target?
      toolbar_drop_item_ =
          CreateBookmarkToolItem(model_->other_node());
      g_object_ref_sink(GTK_OBJECT(toolbar_drop_item_));
    }
  }

  gtk_toolbar_set_drop_highlight_item(GTK_TOOLBAR(bookmark_toolbar_.get()),
                                      GTK_TOOL_ITEM(toolbar_drop_item_),
                                      index);
  if (target_type == ui::GetAtomForTarget(ui::CHROME_BOOKMARK_ITEM)) {
    gdk_drag_status(context, GDK_ACTION_MOVE, time);
  } else {
    gdk_drag_status(context, GDK_ACTION_COPY, time);
  }

  return TRUE;
}

gboolean BookmarkBarGtk::OnToolbarDragMotion(GtkWidget* toolbar,
                                             GdkDragContext* context,
                                             gint x,
                                             gint y,
                                             guint time) {
  gint index = gtk_toolbar_get_drop_index(GTK_TOOLBAR(toolbar), x, y);
  return ItemDraggedOverToolbar(context, index, time);
}

int BookmarkBarGtk::GetToolbarIndexForDragOverFolder(
    GtkWidget* button, gint x) {
  int margin = std::min(15, static_cast<int>(0.3 * button->allocation.width));
  if (x > margin && x < (button->allocation.width - margin))
    return -1;

  gint index = gtk_toolbar_get_item_index(GTK_TOOLBAR(bookmark_toolbar_.get()),
                                          GTK_TOOL_ITEM(button->parent));
  if (x > margin)
    index++;
  return index;
}

gboolean BookmarkBarGtk::OnFolderDragMotion(GtkWidget* button,
                                            GdkDragContext* context,
                                            gint x,
                                            gint y,
                                            guint time) {
  if (!edit_bookmarks_enabled_.GetValue())
    return FALSE;
  GdkAtom target_type = gtk_drag_dest_find_target(button, context, NULL);
  if (target_type == GDK_NONE)
    return FALSE;

  int index = GetToolbarIndexForDragOverFolder(button, x);
  if (index < 0) {
    ClearToolbarDropHighlighting();

    // Drag is over middle of folder.
    gtk_drag_highlight(button);
    if (target_type == ui::GetAtomForTarget(ui::CHROME_BOOKMARK_ITEM)) {
      gdk_drag_status(context, GDK_ACTION_MOVE, time);
    } else {
      gdk_drag_status(context, GDK_ACTION_COPY, time);
    }

    return TRUE;
  }

  // Remove previous highlighting.
  gtk_drag_unhighlight(button);
  return ItemDraggedOverToolbar(context, index, time);
}

void BookmarkBarGtk::OnEditBookmarksEnabledChanged() {
  GtkDestDefaults dest_defaults =
      *edit_bookmarks_enabled_ ? GTK_DEST_DEFAULT_ALL :
                                 GTK_DEST_DEFAULT_DROP;
  gtk_drag_dest_set(overflow_button_, dest_defaults, NULL, 0, kDragAction);
  gtk_drag_dest_set(other_bookmarks_button_, dest_defaults,
                    NULL, 0, kDragAction);
  ui::SetDestTargetList(overflow_button_, kDestTargetList);
  ui::SetDestTargetList(other_bookmarks_button_, kDestTargetList);
}

void BookmarkBarGtk::ClearToolbarDropHighlighting() {
  if (toolbar_drop_item_) {
    g_object_unref(toolbar_drop_item_);
    toolbar_drop_item_ = NULL;
  }

  gtk_toolbar_set_drop_highlight_item(GTK_TOOLBAR(bookmark_toolbar_.get()),
                                      NULL, 0);
}

void BookmarkBarGtk::OnDragLeave(GtkWidget* sender,
                                 GdkDragContext* context,
                                 guint time) {
  if (GTK_IS_BUTTON(sender))
    gtk_drag_unhighlight(sender);

  ClearToolbarDropHighlighting();
}

void BookmarkBarGtk::OnToolbarSizeAllocate(GtkWidget* widget,
                                           GtkAllocation* allocation) {
  if (bookmark_toolbar_.get()->allocation.width ==
      last_allocation_width_) {
    // If the width hasn't changed, then the visibility of the chevron
    // doesn't need to change. This check prevents us from getting stuck in a
    // loop where allocates are queued indefinitely while the visibility of
    // overflow chevron toggles without actual resizes of the toolbar.
    return;
  }
  last_allocation_width_ = bookmark_toolbar_.get()->allocation.width;

  SetChevronState();
}

void BookmarkBarGtk::OnDragReceived(GtkWidget* widget,
                                    GdkDragContext* context,
                                    gint x, gint y,
                                    GtkSelectionData* selection_data,
                                    guint target_type, guint time) {
  if (!edit_bookmarks_enabled_.GetValue()) {
    gtk_drag_finish(context, FALSE, FALSE, time);
    return;
  }

  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  const BookmarkNode* dest_node = model_->GetBookmarkBarNode();
  gint index;
  if (widget == bookmark_toolbar_.get()) {
    index = gtk_toolbar_get_drop_index(
      GTK_TOOLBAR(bookmark_toolbar_.get()), x, y);
  } else if (widget == instructions_) {
    dest_node = model_->GetBookmarkBarNode();
    index = 0;
  } else {
    index = GetToolbarIndexForDragOverFolder(widget, x);
    if (index < 0) {
      dest_node = GetNodeForToolButton(widget);
      index = dest_node->child_count();
    }
  }

  switch (target_type) {
    case ui::CHROME_BOOKMARK_ITEM: {
      std::vector<const BookmarkNode*> nodes =
          bookmark_utils::GetNodesFromSelection(context, selection_data,
                                                target_type,
                                                profile_,
                                                &delete_selection_data,
                                                &dnd_success);
      DCHECK(!nodes.empty());
      for (std::vector<const BookmarkNode*>::iterator it = nodes.begin();
           it != nodes.end(); ++it) {
        model_->Move(*it, dest_node, index);
        index = dest_node->GetIndexOf(*it) + 1;
      }
      break;
    }

    case ui::CHROME_NAMED_URL: {
      dnd_success = bookmark_utils::CreateNewBookmarkFromNamedUrl(
          selection_data, model_, dest_node, index);
      break;
    }

    case ui::TEXT_URI_LIST: {
      dnd_success = bookmark_utils::CreateNewBookmarksFromURIList(
          selection_data, model_, dest_node, index);
      break;
    }

    case ui::NETSCAPE_URL: {
      dnd_success = bookmark_utils::CreateNewBookmarkFromNetscapeURL(
          selection_data, model_, dest_node, index);
      break;
    }

    case ui::TEXT_PLAIN: {
      guchar* text = gtk_selection_data_get_text(selection_data);
      if (!text)
        break;
      GURL url(reinterpret_cast<char*>(text));
      g_free(text);
      // TODO(estade): It would be nice to head this case off at drag motion,
      // so that it doesn't look like we can drag onto the bookmark bar.
      if (!url.is_valid())
        break;
      string16 title = bookmark_utils::GetNameForURL(url);
      model_->AddURL(dest_node, index, title, url);
      dnd_success = TRUE;
      break;
    }
  }

  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

gboolean BookmarkBarGtk::OnEventBoxExpose(GtkWidget* widget,
                                          GdkEventExpose* event) {
  GtkThemeService* theme_provider = theme_service_;

  // We don't need to render the toolbar image in GTK mode, except when
  // detached.
  if (theme_provider->UseGtkTheme() && !floating_)
    return FALSE;

  if (!floating_) {
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    gdk_cairo_rectangle(cr, &event->area);
    cairo_clip(cr);

    // Paint the background theme image.
    gfx::Point tabstrip_origin =
        tabstrip_origin_provider_->GetTabStripOriginForWidget(widget);
    gtk_util::DrawThemedToolbarBackground(widget, cr, event, tabstrip_origin,
                                          theme_provider);

    cairo_destroy(cr);
  } else {
    gfx::Size tab_contents_size;
    if (!GetTabContentsSize(&tab_contents_size))
      return FALSE;
    gfx::CanvasSkiaPaint canvas(event, true);

    gfx::Rect area = GTK_WIDGET_NO_WINDOW(widget) ?
        gfx::Rect(widget->allocation) :
        gfx::Rect(0, 0, widget->allocation.width, widget->allocation.height);
    NtpBackgroundUtil::PaintBackgroundDetachedMode(theme_provider, &canvas,
        area, tab_contents_size.height());
  }

  return FALSE;  // Propagate expose to children.
}

void BookmarkBarGtk::OnEventBoxDestroy(GtkWidget* widget) {
  if (model_)
    model_->RemoveObserver(this);

  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void BookmarkBarGtk::OnParentSizeAllocate(GtkWidget* widget,
                                          GtkAllocation* allocation) {
  // In floating mode, our layout depends on the size of the tab contents.
  // We get the size-allocate signal before the tab contents does, hence we
  // need to post a delayed task so we will paint correctly. Note that
  // gtk_widget_queue_draw by itself does not work, despite that it claims to
  // be asynchronous.
  if (floating_) {
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &BookmarkBarGtk::PaintEventBox));
  }
}

void BookmarkBarGtk::OnThrobbingWidgetDestroy(GtkWidget* widget) {
  SetThrobbingWidget(NULL);
}

// MenuBarHelper::Delegate implementation --------------------------------------
void BookmarkBarGtk::PopupForButton(GtkWidget* button) {
  const BookmarkNode* node = GetNodeForToolButton(button);
  DCHECK(node);
  DCHECK(page_navigator_);

  int first_hidden = GetFirstHiddenBookmark(0, NULL);
  if (first_hidden == -1) {
    // No overflow exists: don't show anything for the overflow button.
    if (button == overflow_button_)
      return;
  } else {
    // Overflow exists: don't show anything for an overflowed folder button.
    if (button != overflow_button_ && button != other_bookmarks_button_ &&
        node->parent()->GetIndexOf(node) >= first_hidden) {
      return;
    }
  }

  current_menu_.reset(
      new BookmarkMenuController(browser_, profile_, page_navigator_,
                                 GTK_WINDOW(gtk_widget_get_toplevel(button)),
                                 node,
                                 button == overflow_button_ ?
                                     first_hidden : 0));
  menu_bar_helper_.MenuStartedShowing(button, current_menu_->widget());
  GdkEvent* event = gtk_get_current_event();
  current_menu_->Popup(button, event->button.button, event->button.time);
  gdk_event_free(event);
}

void BookmarkBarGtk::PopupForButtonNextTo(GtkWidget* button,
                                          GtkMenuDirectionType dir) {
  const BookmarkNode* relative_node = GetNodeForToolButton(button);
  DCHECK(relative_node);

  // Find out the order of the buttons.
  std::vector<GtkWidget*> folder_list;
  const int first_hidden = GetFirstHiddenBookmark(0, &folder_list);
  if (first_hidden != -1)
    folder_list.push_back(overflow_button_);
  folder_list.push_back(other_bookmarks_button_);

  // Find the position of |button|.
  int button_idx = -1;
  for (size_t i = 0; i < folder_list.size(); ++i) {
    if (folder_list[i] == button) {
      button_idx = i;
      break;
    }
  }
  DCHECK_NE(button_idx, -1);

  // Find the GtkWidget* for the actual target button.
  int shift = dir == GTK_MENU_DIR_PARENT ? -1 : 1;
  button_idx = (button_idx + shift + folder_list.size()) % folder_list.size();
  PopupForButton(folder_list[button_idx]);
}

void BookmarkBarGtk::CloseMenu() {
  current_context_menu_->Cancel();
}
