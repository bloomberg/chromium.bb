// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/gtk/action_box_button_gtk.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_bubble_gtk.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/chrome_to_mobile_bubble_gtk.h"
#include "chrome/browser/ui/gtk/content_setting_bubble_gtk.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "chrome/browser/ui/gtk/first_run_bubble.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/nine_box.h"
#include "chrome/browser/ui/gtk/omnibox/omnibox_view_gtk.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "chrome/browser/ui/gtk/script_bubble_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/gtk/zoom_bubble_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/omnibox/alternate_nav_url_fetcher.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/badge_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/accelerators/platform_accelerator_gtk.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

using content::NavigationEntry;
using content::OpenURLParams;
using content::WebContents;
using extensions::LocationBarController;
using extensions::Extension;

namespace {

// We are positioned with a little bit of extra space that we don't use now.
const int kTopMargin = 1;
const int kBottomMargin = 1;
const int kLeftMargin = 1;
const int kRightMargin = 1;
// We draw a border on the top and bottom (but not on left or right).
const int kBorderThickness = 1;

// Spacing needed to align the bubble with the left side of the omnibox.
const int kFirstRunBubbleLeftSpacing = 4;

// The padding around the top, bottom, and sides of the location bar hbox.
// We don't want to edit control's text to be right against the edge,
// as well the tab to search box and other widgets need to have the padding on
// top and bottom to avoid drawing larger than the location bar space.
const int kHboxBorder = 2;

// Padding between the elements in the bar.
const int kInnerPadding = 2;
const int kScriptBadgeInnerPadding = 9;

// Colors used to draw the EV certificate rounded bubble.
const GdkColor kEvSecureTextColor = GDK_COLOR_RGB(0x07, 0x95, 0x00);
const GdkColor kEvSecureBackgroundColor = GDK_COLOR_RGB(0xef, 0xfc, 0xef);
const GdkColor kEvSecureBorderColor = GDK_COLOR_RGB(0x90, 0xc3, 0x90);

// Colors used to draw the Tab to Search rounded bubble.
const GdkColor kKeywordBackgroundColor = GDK_COLOR_RGB(0xf0, 0xf4, 0xfa);
const GdkColor kKeywordBorderColor = GDK_COLOR_RGB(0xcb, 0xde, 0xf7);

// Use weak gray for showing search and keyword hint text.
const GdkColor kHintTextColor = GDK_COLOR_RGB(0x75, 0x75, 0x75);

// Size of the rounding of the "Search site for:" box.
const int kCornerSize = 3;

// Default page tool animation time (open and close). In ms.
const int kPageToolAnimationTime = 150;

// The time, in ms, that the content setting label is fully displayed, for the
// cases where we animate it into and out of view.
const int kContentSettingImageDisplayTime = 3200;
// The time, in ms, of the animation (open and close).
const int kContentSettingImageAnimationTime = 150;

// Animation opening time for web intents button (in ms).
const int kWebIntentsButtonAnimationTime = 150;

// Color of border of content setting area (icon/label).
const GdkColor kContentSettingBorderColor = GDK_COLOR_RGB(0xe9, 0xb9, 0x66);
// Colors for the background gradient.
const GdkColor kContentSettingTopColor = GDK_COLOR_RGB(0xff, 0xf8, 0xd4);
const GdkColor kContentSettingBottomColor = GDK_COLOR_RGB(0xff, 0xe6, 0xaf);

// Styling for gray button.
const GdkColor kGrayBorderColor = GDK_COLOR_RGB(0xa0, 0xa0, 0xa0);
const GdkColor kTopColorGray = GDK_COLOR_RGB(0xe5, 0xe5, 0xe5);
const GdkColor kBottomColorGray = GDK_COLOR_RGB(0xd0, 0xd0, 0xd0);

inline int InnerPadding() {
  return extensions::FeatureSwitch::script_badges()->IsEnabled() ?
      kScriptBadgeInnerPadding : kInnerPadding;
}

// If widget is visible, increment the int pointed to by count.
// Suitible for use with gtk_container_foreach.
void CountVisibleWidgets(GtkWidget* widget, gpointer count) {
  if (gtk_widget_get_visible(widget))
    *static_cast<int*>(count) += 1;
}

class ContentSettingImageViewGtk : public LocationBarViewGtk::PageToolViewGtk,
                                   public BubbleDelegateGtk {
 public:
  ContentSettingImageViewGtk(ContentSettingsType content_type,
                             const LocationBarViewGtk* parent);
  virtual ~ContentSettingImageViewGtk();

  // PageToolViewGtk
  virtual void Update(WebContents* web_contents) OVERRIDE;

  // ui::AnimationDelegate
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

 private:
  // PageToolViewGtk
  virtual GdkColor button_border_color() const OVERRIDE;
  virtual GdkColor gradient_top_color() const OVERRIDE;
  virtual GdkColor gradient_bottom_color() const OVERRIDE;
  virtual void OnClick(GtkWidget* sender) OVERRIDE;

  // BubbleDelegateGtk
  virtual void BubbleClosing(BubbleGtk* bubble,
                             bool closed_by_escape) OVERRIDE;

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  // The currently shown bubble if any.
  ContentSettingBubbleGtk* content_setting_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageViewGtk);
};

ContentSettingImageViewGtk::ContentSettingImageViewGtk(
    ContentSettingsType content_type,
    const LocationBarViewGtk* parent)
    : PageToolViewGtk(parent),
      content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      content_setting_bubble_(NULL) {
  animation_.SetSlideDuration(kContentSettingImageAnimationTime);
}

ContentSettingImageViewGtk::~ContentSettingImageViewGtk() {
  if (content_setting_bubble_)
    content_setting_bubble_->Close();
}

void ContentSettingImageViewGtk::Update(WebContents* web_contents) {
  if (web_contents)
    content_setting_image_model_->UpdateFromWebContents(web_contents);

  if (!content_setting_image_model_->is_visible()) {
    gtk_widget_hide(widget());
    return;
  }

  gtk_image_set_from_pixbuf(GTK_IMAGE(image_.get()),
      GtkThemeService::GetFrom(parent_->browser()->profile())->GetImageNamed(
          content_setting_image_model_->get_icon()).ToGdkPixbuf());

  gtk_widget_set_tooltip_text(widget(),
      content_setting_image_model_->get_tooltip().c_str());
  gtk_widget_show_all(widget());

  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings || content_settings->IsBlockageIndicated(
      content_setting_image_model_->get_content_settings_type()))
    return;

  // The content blockage was not yet indicated to the user. Start indication
  // animation and clear "not yet shown" flag.
  content_settings->SetBlockageHasBeenIndicated(
      content_setting_image_model_->get_content_settings_type());

  int label_string_id =
      content_setting_image_model_->explanatory_string_id();
  // If there's no string for the content type, we don't animate.
  if (!label_string_id)
    return;

  gtk_label_set_text(GTK_LABEL(label_.get()),
      l10n_util::GetStringUTF8(label_string_id).c_str());
  StartAnimating();
}

void ContentSettingImageViewGtk::AnimationEnded(
    const ui::Animation* animation) {
  if (animation_.IsShowing()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ContentSettingImageViewGtk::CloseAnimation,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kContentSettingImageDisplayTime));
  } else {
    gtk_widget_hide(label_.get());
    gtk_util::StopActingAsRoundedWindow(event_box_.get());
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  }
}

GdkColor ContentSettingImageViewGtk::
    button_border_color() const {
  return kContentSettingBorderColor;
}

GdkColor ContentSettingImageViewGtk::
    gradient_top_color() const {
  return kContentSettingTopColor;
}

GdkColor ContentSettingImageViewGtk::
    gradient_bottom_color() const {
  return kContentSettingBottomColor;
}

void ContentSettingImageViewGtk::OnClick(
    GtkWidget* sender) {
  WebContents* web_contents = parent_->GetWebContents();
  if (!web_contents)
    return;
  Profile* profile = parent_->browser()->profile();
  content_setting_bubble_ = new ContentSettingBubbleGtk(
      sender, this,
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          parent_->browser()->content_setting_bubble_model_delegate(),
          web_contents,
          profile,
          content_setting_image_model_->get_content_settings_type()),
      profile, web_contents);
  return;
}

void ContentSettingImageViewGtk::BubbleClosing(
    BubbleGtk* bubble,
    bool closed_by_escape) {
  content_setting_bubble_ = NULL;
}

class WebIntentsButtonViewGtk : public LocationBarViewGtk::PageToolViewGtk {
 public:
  explicit WebIntentsButtonViewGtk(const LocationBarViewGtk* parent)
      : LocationBarViewGtk::PageToolViewGtk(parent) {
    animation_.SetSlideDuration(kWebIntentsButtonAnimationTime);
  }
  virtual ~WebIntentsButtonViewGtk() {}

  // PageToolViewGtk
  virtual void Update(WebContents* web_contents) OVERRIDE;

 private:
  // PageToolViewGtk
  virtual GdkColor button_border_color() const OVERRIDE;
  virtual GdkColor gradient_top_color() const OVERRIDE;
  virtual GdkColor gradient_bottom_color() const OVERRIDE;
  virtual void OnClick(GtkWidget* sender) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsButtonViewGtk);
};

void WebIntentsButtonViewGtk::Update(WebContents* web_contents) {
  WebIntentPickerController* web_intent_picker_controller =
      web_contents ? WebIntentPickerController::FromWebContents(web_contents)
                   : NULL;
  if (!web_intent_picker_controller ||
      !web_intent_picker_controller->ShowLocationBarPickerButton()) {
    gtk_widget_hide(widget());
    return;
  }

  gtk_widget_set_tooltip_text(widget(),
      l10n_util::GetStringUTF8(IDS_INTENT_PICKER_USE_ANOTHER_SERVICE).c_str());
  gtk_widget_show_all(widget());

  gtk_label_set_text(GTK_LABEL(label_.get()),
      l10n_util::GetStringUTF8(IDS_INTENT_PICKER_USE_ANOTHER_SERVICE).c_str());

  StartAnimating();
}

void WebIntentsButtonViewGtk::OnClick(GtkWidget* sender) {
  WebContents* web_contents = parent_->GetWebContents();
  if (!web_contents)
    return;

  WebIntentPickerController::FromWebContents(web_contents)->
      LocationBarPickerButtonClicked();
}

GdkColor WebIntentsButtonViewGtk::button_border_color() const {
  return kGrayBorderColor;
}

GdkColor WebIntentsButtonViewGtk::gradient_top_color() const {
  return kTopColorGray;
}

GdkColor WebIntentsButtonViewGtk::gradient_bottom_color() const {
  return kBottomColorGray;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LocationBarViewGtk

// static
const GdkColor LocationBarViewGtk::kBackgroundColor =
    GDK_COLOR_RGB(255, 255, 255);

LocationBarViewGtk::LocationBarViewGtk(Browser* browser)
    : zoom_image_(NULL),
      script_bubble_button_image_(NULL),
      num_running_scripts_(0u),
      star_image_(NULL),
      starred_(false),
      star_sized_(false),
      site_type_alignment_(NULL),
      site_type_event_box_(NULL),
      location_icon_image_(NULL),
      drag_icon_(NULL),
      enable_location_drag_(false),
      security_info_label_(NULL),
      web_intents_button_view_(new WebIntentsButtonViewGtk(this)),
      tab_to_search_alignment_(NULL),
      tab_to_search_box_(NULL),
      tab_to_search_full_label_(NULL),
      tab_to_search_partial_label_(NULL),
      tab_to_search_hint_(NULL),
      tab_to_search_hint_leading_label_(NULL),
      tab_to_search_hint_icon_(NULL),
      tab_to_search_hint_trailing_label_(NULL),
      command_updater_(browser->command_controller()->command_updater()),
      toolbar_model_(browser->toolbar_model()),
      browser_(browser),
      disposition_(CURRENT_TAB),
      transition_(content::PageTransitionFromInt(
          content::PAGE_TRANSITION_TYPED |
          content::PAGE_TRANSITION_FROM_ADDRESS_BAR)),
      weak_ptr_factory_(this),
      popup_window_mode_(false),
      theme_service_(NULL),
      hbox_width_(0),
      entry_box_width_(0),
      show_selected_keyword_(false),
      show_keyword_hint_(false) {
}

LocationBarViewGtk::~LocationBarViewGtk() {
  // All of our widgets should be children of / owned by the alignment.
  zoom_.Destroy();
  script_bubble_button_.Destroy();
  star_.Destroy();
  hbox_.Destroy();
  content_setting_hbox_.Destroy();
  page_action_hbox_.Destroy();
  web_intents_hbox_.Destroy();
}

void LocationBarViewGtk::Init(bool popup_window_mode) {
  popup_window_mode_ = popup_window_mode;

  Profile* profile = browser_->profile();
  theme_service_ = GtkThemeService::GetFrom(profile);

  // Create the widget first, so we can pass it to the OmniboxViewGtk.
  hbox_.Own(gtk_hbox_new(FALSE, InnerPadding()));
  gtk_container_set_border_width(GTK_CONTAINER(hbox_.get()), kHboxBorder);
  // We will paint for the alignment, to paint the background and border.
  gtk_widget_set_app_paintable(hbox_.get(), TRUE);
  // Redraw the whole location bar when it changes size (e.g., when toggling
  // the home button on/off.
  gtk_widget_set_redraw_on_allocate(hbox_.get(), TRUE);

  // Now initialize the OmniboxViewGtk.
  location_entry_.reset(new OmniboxViewGtk(this, toolbar_model_, browser_,
      command_updater_, popup_window_mode_, hbox_.get()));
  location_entry_->Init();

  g_signal_connect(hbox_.get(), "expose-event",
                   G_CALLBACK(&HandleExposeThunk), this);

  BuildSiteTypeArea();

  // Put |tab_to_search_box_|, |location_entry_|, and |tab_to_search_hint_| into
  // a sub hbox, so that we can make this part horizontally shrinkable without
  // affecting other elements in the location bar.
  entry_box_ = gtk_hbox_new(FALSE, InnerPadding());
  gtk_widget_show(entry_box_);
  gtk_widget_set_size_request(entry_box_, 0, -1);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), entry_box_, TRUE, TRUE, 0);

  // We need to adjust the visibility of the search hint widgets according to
  // the horizontal space in the |entry_box_|.
  g_signal_connect(entry_box_, "size-allocate",
                   G_CALLBACK(&OnEntryBoxSizeAllocateThunk), this);

  // Tab to search (the keyword box on the left hand side).
  tab_to_search_full_label_ =
      theme_service_->BuildLabel("", ui::kGdkBlack);
  tab_to_search_partial_label_ =
      theme_service_->BuildLabel("", ui::kGdkBlack);
  GtkWidget* tab_to_search_label_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_label_hbox),
                     tab_to_search_full_label_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_label_hbox),
                     tab_to_search_partial_label_, FALSE, FALSE, 0);
  GtkWidget* tab_to_search_hbox = gtk_hbox_new(FALSE, 0);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  tab_to_search_magnifier_ = gtk_image_new_from_pixbuf(
      rb.GetNativeImageNamed(IDR_KEYWORD_SEARCH_MAGNIFIER).ToGdkPixbuf());
  gtk_box_pack_start(GTK_BOX(tab_to_search_hbox), tab_to_search_magnifier_,
                     FALSE, FALSE, 0);
  gtk_util::CenterWidgetInHBox(tab_to_search_hbox, tab_to_search_label_hbox,
                               false, 0);

  // This creates a box around the keyword text with a border, background color,
  // and padding around the text.
  tab_to_search_box_ = gtk_util::CreateGtkBorderBin(
      tab_to_search_hbox, NULL, 1, 1, 1, 3);
  gtk_widget_set_name(tab_to_search_box_, "chrome-tab-to-search-box");
  gtk_util::ActAsRoundedWindow(tab_to_search_box_, kKeywordBorderColor,
                               kCornerSize,
                               gtk_util::ROUNDED_ALL, gtk_util::BORDER_ALL);

  // Put the event box in an alignment to get the padding correct.
  tab_to_search_alignment_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_container_add(GTK_CONTAINER(tab_to_search_alignment_),
                    tab_to_search_box_);
  gtk_box_pack_start(GTK_BOX(entry_box_), tab_to_search_alignment_,
                     FALSE, FALSE, 0);

  // Show all children widgets of |tab_to_search_box_| initially, except
  // |tab_to_search_partial_label_|.
  gtk_widget_show_all(tab_to_search_box_);
  gtk_widget_hide(tab_to_search_partial_label_);

  location_entry_alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(location_entry_alignment_),
                    location_entry_->GetNativeView());
  gtk_box_pack_start(GTK_BOX(entry_box_), location_entry_alignment_,
                     TRUE, TRUE, 0);

  // Tab to search notification (the hint on the right hand side).
  tab_to_search_hint_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_name(tab_to_search_hint_, "chrome-tab-to-search-hint");
  tab_to_search_hint_leading_label_ =
      theme_service_->BuildLabel("", kHintTextColor);
  gtk_widget_set_sensitive(tab_to_search_hint_leading_label_, FALSE);
  tab_to_search_hint_icon_ = gtk_image_new_from_pixbuf(
      rb.GetNativeImageNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB).ToGdkPixbuf());
  tab_to_search_hint_trailing_label_ =
      theme_service_->BuildLabel("", kHintTextColor);
  gtk_widget_set_sensitive(tab_to_search_hint_trailing_label_, FALSE);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_leading_label_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_icon_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_trailing_label_,
                     FALSE, FALSE, 0);
  // Show all children widgets of |tab_to_search_hint_| initially.
  gtk_widget_show_all(tab_to_search_hint_);
  gtk_widget_hide(tab_to_search_hint_);
  // tab_to_search_hint_ gets hidden initially in OnChanged.  Hiding it here
  // doesn't work, someone is probably calling show_all on our parent box.
  gtk_box_pack_end(GTK_BOX(entry_box_), tab_to_search_hint_, FALSE, FALSE, 0);

  if (extensions::FeatureSwitch::action_box()->IsEnabled()) {
    // TODO(mpcomplete): should we hide this if ShouldOnlyShowLocation()==true?
    action_box_button_.reset(new ActionBoxButtonGtk(browser_));

    // TODO(mpcomplete): Figure out why CustomDrawButton is offset 3 pixels.
    // This offset corrects the strange offset of CustomDrawButton.
    const int kMagicActionBoxYOffset = 3;
    GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
                              0, kMagicActionBoxYOffset,
                              0, InnerPadding());
    gtk_container_add(GTK_CONTAINER(alignment), action_box_button_->widget());

    gtk_box_pack_end(GTK_BOX(hbox_.get()), alignment,
                     FALSE, FALSE, 0);
  }

  if (browser_defaults::bookmarks_enabled && !ShouldOnlyShowLocation()) {
    // Hide the star icon in popups, app windows, etc.
    CreateStarButton();
    gtk_box_pack_end(GTK_BOX(hbox_.get()), star_.get(), FALSE, FALSE, 0);
  }

  CreateScriptBubbleButton();
  gtk_box_pack_end(GTK_BOX(hbox_.get()), script_bubble_button_.get(), FALSE,
                   FALSE, 0);

  CreateZoomButton();
  gtk_box_pack_end(GTK_BOX(hbox_.get()), zoom_.get(), FALSE, FALSE, 0);

  content_setting_hbox_.Own(gtk_hbox_new(FALSE, InnerPadding() + 1));
  gtk_widget_set_name(content_setting_hbox_.get(),
                      "chrome-content-setting-hbox");
  gtk_box_pack_end(GTK_BOX(hbox_.get()), content_setting_hbox_.get(),
                   FALSE, FALSE, 1);

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingImageViewGtk* content_setting_view =
        new ContentSettingImageViewGtk(
            static_cast<ContentSettingsType>(i), this);
    content_setting_views_.push_back(content_setting_view);
    gtk_box_pack_end(GTK_BOX(content_setting_hbox_.get()),
                     content_setting_view->widget(), FALSE, FALSE, 0);
  }

  page_action_hbox_.Own(gtk_hbox_new(FALSE, InnerPadding()));
  gtk_widget_set_name(page_action_hbox_.get(),
                      "chrome-page-action-hbox");
  gtk_box_pack_end(GTK_BOX(hbox_.get()), page_action_hbox_.get(),
                   FALSE, FALSE, 0);

  web_intents_hbox_.Own(gtk_hbox_new(FALSE, InnerPadding()));
  gtk_widget_set_name(web_intents_hbox_.get(),
                      "chrome-web-intents-hbox");
  gtk_box_pack_end(GTK_BOX(hbox_.get()), web_intents_hbox_.get(),
                   FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(web_intents_hbox_.get()),
                   web_intents_button_view_->widget(), FALSE, FALSE, 0);

  // Now that we've created the widget hierarchy, connect to the main |hbox_|'s
  // size-allocate so we can do proper resizing and eliding on
  // |security_info_label_|.
  g_signal_connect(hbox_.get(), "size-allocate",
                   G_CALLBACK(&OnHboxSizeAllocateThunk), this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
                 content::Source<Profile>(browser()->profile()));
  edit_bookmarks_enabled_.Init(prefs::kEditBookmarksEnabled,
                               profile->GetPrefs(),
                               base::Bind(&LocationBarViewGtk::UpdateStarIcon,
                                          base::Unretained(this)));

  theme_service_->InitThemesFor(this);
}

void LocationBarViewGtk::BuildSiteTypeArea() {
  location_icon_image_ = gtk_image_new();
  gtk_widget_set_name(location_icon_image_, "chrome-location-icon");

  GtkWidget* icon_alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(icon_alignment), 0, 0, 2, 0);
  gtk_container_add(GTK_CONTAINER(icon_alignment), location_icon_image_);
  gtk_widget_show_all(icon_alignment);

  security_info_label_ = gtk_label_new(NULL);
  gtk_label_set_ellipsize(GTK_LABEL(security_info_label_),
                          PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_modify_fg(GTK_WIDGET(security_info_label_), GTK_STATE_NORMAL,
                       &kEvSecureTextColor);
  gtk_widget_set_name(security_info_label_,
                      "chrome-location-bar-security-info-label");

  GtkWidget* site_type_hbox = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(site_type_hbox), icon_alignment,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(site_type_hbox), security_info_label_,
                     FALSE, FALSE, 2);

  site_type_event_box_ = gtk_event_box_new();
  gtk_widget_modify_bg(site_type_event_box_, GTK_STATE_NORMAL,
                       &kEvSecureBackgroundColor);
  g_signal_connect(site_type_event_box_, "drag-data-get",
                   G_CALLBACK(&OnIconDragDataThunk), this);
  g_signal_connect(site_type_event_box_, "drag-begin",
                   G_CALLBACK(&OnIconDragBeginThunk), this);
  g_signal_connect(site_type_event_box_, "drag-end",
                   G_CALLBACK(&OnIconDragEndThunk), this);

  // Make the event box not visible so it does not paint a background.
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(site_type_event_box_),
                                   FALSE);
  gtk_widget_set_name(site_type_event_box_,
                      "chrome-location-icon-eventbox");
  gtk_container_add(GTK_CONTAINER(site_type_event_box_),
                    site_type_hbox);

  // Put the event box in an alignment to get the padding correct.
  site_type_alignment_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_container_add(GTK_CONTAINER(site_type_alignment_),
                    site_type_event_box_);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), site_type_alignment_,
                     FALSE, FALSE, 0);

  gtk_widget_set_tooltip_text(location_icon_image_,
      l10n_util::GetStringUTF8(IDS_TOOLTIP_LOCATION_ICON).c_str());

  g_signal_connect(site_type_event_box_, "button-release-event",
                   G_CALLBACK(&OnIconReleasedThunk), this);
}

void LocationBarViewGtk::SetSiteTypeDragSource() {
  bool enable = !GetLocationEntry()->IsEditingOrEmpty();
  if (enable_location_drag_ == enable)
    return;
  enable_location_drag_ = enable;

  if (!enable) {
    gtk_drag_source_unset(site_type_event_box_);
    return;
  }

  gtk_drag_source_set(site_type_event_box_, GDK_BUTTON1_MASK,
                      NULL, 0, GDK_ACTION_COPY);
  ui::SetSourceTargetListFromCodeMask(site_type_event_box_,
                                      ui::TEXT_PLAIN |
                                      ui::TEXT_URI_LIST |
                                      ui::CHROME_NAMED_URL);
}

WebContents* LocationBarViewGtk::GetWebContents() const {
  return chrome::GetActiveWebContents(browser_);
}

void LocationBarViewGtk::SetPreviewEnabledPageAction(
    ExtensionAction* page_action,
    bool preview_enabled) {
  DCHECK(page_action);
  for (ScopedVector<PageActionViewGtk>::iterator iter =
       page_action_views_.begin(); iter != page_action_views_.end();
       ++iter) {
    if ((*iter)->page_action() == page_action) {
      (*iter)->set_preview_enabled(preview_enabled);
      UpdatePageActions();
      return;
    }
  }
}

GtkWidget* LocationBarViewGtk::GetPageActionWidget(
    ExtensionAction* page_action) {
  DCHECK(page_action);
  for (ScopedVector<PageActionViewGtk>::iterator iter =
           page_action_views_.begin();
       iter != page_action_views_.end();
       ++iter) {
    if ((*iter)->page_action() == page_action)
      return (*iter)->widget();
  }
  return NULL;
}

void LocationBarViewGtk::Update(const WebContents* contents) {
  UpdateZoomIcon();
  UpdateScriptBubbleIcon();
  UpdateStarIcon();
  UpdateSiteTypeArea();
  UpdateContentSettingsIcons();
  UpdatePageActions();
  UpdateWebIntentsButton();
  location_entry_->Update(contents);
  // The security level (background color) could have changed, etc.
  if (theme_service_->UsingNativeTheme()) {
    // In GTK mode, we need our parent to redraw, as it draws the text entry
    // border.
    gtk_widget_queue_draw(gtk_widget_get_parent(widget()));
  } else {
    gtk_widget_queue_draw(widget());
  }
  ZoomBubbleGtk::CloseBubble();
}

void LocationBarViewGtk::OnAutocompleteAccept(const GURL& url,
    WindowOpenDisposition disposition,
    content::PageTransition transition,
    const GURL& alternate_nav_url) {
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

void LocationBarViewGtk::OnChanged() {
  UpdateSiteTypeArea();

  const string16 keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint = location_entry_->model()->is_keyword_hint();
  show_selected_keyword_ = !keyword.empty() && !is_keyword_hint;
  show_keyword_hint_ = !keyword.empty() && is_keyword_hint;

  if (show_selected_keyword_)
    SetKeywordLabel(keyword);

  if (show_keyword_hint_)
    SetKeywordHintLabel(keyword);

  AdjustChildrenVisibility();
}

void LocationBarViewGtk::OnSelectionBoundsChanged() {
  NOTIMPLEMENTED();
}

GtkWidget* LocationBarViewGtk::CreateIconButton(
    GtkWidget** image,
    int image_id,
    ViewID debug_id,
    int tooltip_id,
    gboolean (click_callback)(GtkWidget*, GdkEventButton*, gpointer)) {
  *image = image_id ?
      gtk_image_new_from_pixbuf(
          theme_service_->GetImageNamed(image_id).ToGdkPixbuf()) :
      gtk_image_new();

  GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            0, InnerPadding());
  gtk_container_add(GTK_CONTAINER(alignment), *image);

  GtkWidget* result = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(result), FALSE);
  gtk_container_add(GTK_CONTAINER(result), alignment);
  gtk_widget_show_all(result);

  if (debug_id != VIEW_ID_NONE)
    ViewIDUtil::SetID(result, debug_id);

  if (tooltip_id) {
    gtk_widget_set_tooltip_text(result,
                                l10n_util::GetStringUTF8(tooltip_id).c_str());
  }

  g_signal_connect(result, "button-press-event",
                     G_CALLBACK(click_callback), this);

  return result;
}

void LocationBarViewGtk::CreateZoomButton() {
  zoom_.Own(CreateIconButton(&zoom_image_,
                             0,
                             VIEW_ID_ZOOM_BUTTON,
                             0,
                             OnZoomButtonPressThunk));
}

void LocationBarViewGtk::CreateScriptBubbleButton() {
  script_bubble_button_.Own(CreateIconButton(&script_bubble_button_image_,
                                             0,
                                             VIEW_ID_SCRIPT_BUBBLE,
                                             IDS_TOOLTIP_SCRIPT_BUBBLE,
                                             OnScriptBubbleButtonPressThunk));
  gtk_image_set_from_pixbuf(
      GTK_IMAGE(script_bubble_button_image_),
      theme_service_->GetImageNamed(
          IDR_EXTENSIONS_SCRIPT_BUBBLE).ToGdkPixbuf());
  g_signal_connect_after(script_bubble_button_image_, "expose-event",
                         G_CALLBACK(&OnScriptBubbleButtonExposeThunk), this);
}

void LocationBarViewGtk::CreateStarButton() {
  star_.Own(CreateIconButton(&star_image_,
                             0,
                             VIEW_ID_STAR_BUTTON,
                             IDS_TOOLTIP_STAR,
                             OnStarButtonPressThunk));
  // We need to track when the star button is resized to show any bubble
  // attached to it at this time.
  g_signal_connect(star_image_, "size-allocate",
                   G_CALLBACK(&OnStarButtonSizeAllocateThunk), this);
}

void LocationBarViewGtk::OnInputInProgress(bool in_progress) {
  // This is identical to the Windows code, except that we don't proxy the call
  // back through the Toolbar, and just access the model here.
  // The edit should make sure we're only notified when something changes.
  DCHECK(toolbar_model_->GetInputInProgress() != in_progress);

  toolbar_model_->SetInputInProgress(in_progress);
  Update(NULL);
}

void LocationBarViewGtk::OnKillFocus() {
}

void LocationBarViewGtk::OnSetFocus() {
  Profile* profile = browser_->profile();
  AccessibilityTextBoxInfo info(
      profile,
      l10n_util::GetStringUTF8(IDS_ACCNAME_LOCATION),
      std::string(),
      false);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      content::Source<Profile>(profile),
      content::Details<AccessibilityTextBoxInfo>(&info));

  // Update the keyword and search hint states.
  OnChanged();
}

gfx::Image LocationBarViewGtk::GetFavicon() const {
  return FaviconTabHelper::FromWebContents(GetWebContents())->GetFavicon();
}

string16 LocationBarViewGtk::GetTitle() const {
  return GetWebContents()->GetTitle();
}

InstantController* LocationBarViewGtk::GetInstant() {
  return browser_->instant_controller() ?
      browser_->instant_controller()->instant() : NULL;
}

void LocationBarViewGtk::ShowFirstRunBubble() {
  // We need the browser window to be shown before we can show the bubble, but
  // we get called before that's happened.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&LocationBarViewGtk::ShowFirstRunBubbleInternal,
                 weak_ptr_factory_.GetWeakPtr()));
}

void LocationBarViewGtk::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  location_entry_->model()->SetInstantSuggestion(suggestion);
}

string16 LocationBarViewGtk::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewGtk::GetWindowOpenDisposition() const {
  return disposition_;
}

content::PageTransition LocationBarViewGtk::GetPageTransition() const {
  return transition_;
}

void LocationBarViewGtk::AcceptInput() {
  location_entry_->model()->AcceptInput(CURRENT_TAB, false);
}

void LocationBarViewGtk::FocusLocation(bool select_all) {
  location_entry_->SetFocus();
  if (select_all)
    location_entry_->SelectAll(true);
}

void LocationBarViewGtk::FocusSearch() {
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarViewGtk::UpdateContentSettingsIcons() {
  bool any_visible = false;
  for (ScopedVector<PageToolViewGtk>::iterator i(
           content_setting_views_.begin());
       i != content_setting_views_.end(); ++i) {
    (*i)->Update(
        toolbar_model_->GetInputInProgress() ? NULL : GetWebContents());
    any_visible = (*i)->IsVisible() || any_visible;
  }

  // If there are no visible content things, hide the top level box so it
  // doesn't mess with padding.
  gtk_widget_set_visible(content_setting_hbox_.get(), any_visible);
}

void LocationBarViewGtk::UpdatePageActions() {
  UpdateScriptBubbleIcon();

  std::vector<ExtensionAction*> new_page_actions;

  WebContents* contents = GetWebContents();
  if (contents) {
    LocationBarController* location_bar_controller =
        extensions::TabHelper::FromWebContents(contents)->
            location_bar_controller();
    new_page_actions = location_bar_controller->GetCurrentActions();
  }

  // Initialize on the first call, or re-initialize if more extensions have been
  // loaded or added after startup.
  if (new_page_actions != page_actions_) {
    page_actions_.swap(new_page_actions);
    page_action_views_.clear();

    for (size_t i = 0; i < page_actions_.size(); ++i) {
      page_action_views_.push_back(
          new PageActionViewGtk(this, page_actions_[i]));
      gtk_box_pack_end(GTK_BOX(page_action_hbox_.get()),
                       page_action_views_[i]->widget(), FALSE, FALSE, 0);
    }
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }

  if (!page_action_views_.empty() && contents) {
    GURL url = chrome::GetActiveWebContents(browser())->GetURL();

    for (size_t i = 0; i < page_action_views_.size(); i++) {
      page_action_views_[i]->UpdateVisibility(
          toolbar_model_->GetInputInProgress() ? NULL : contents, url);
    }
    gtk_widget_queue_draw(hbox_.get());
  }

  // If there are no visible page actions, hide the hbox too, so that it does
  // not affect the padding in the location bar.
  gtk_widget_set_visible(page_action_hbox_.get(),
                         PageActionVisibleCount() && !ShouldOnlyShowLocation());
}

void LocationBarViewGtk::InvalidatePageActions() {
  size_t count_before = page_action_views_.size();
  page_action_views_.clear();
  if (page_action_views_.size() != count_before) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::Source<LocationBar>(this),
        content::NotificationService::NoDetails());
  }
}

void LocationBarViewGtk::UpdateWebIntentsButton() {
  web_intents_button_view_->Update(GetWebContents());
  gtk_widget_set_visible(web_intents_hbox_.get(),
                         web_intents_button_view_->IsVisible());
}

void LocationBarViewGtk::UpdateOpenPDFInReaderPrompt() {
  // Not implemented on Gtk.
}

void LocationBarViewGtk::SaveStateToContents(WebContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarViewGtk::Revert() {
  location_entry_->RevertAll();
}

const OmniboxView* LocationBarViewGtk::GetLocationEntry() const {
  return location_entry_.get();
}

OmniboxView* LocationBarViewGtk::GetLocationEntry() {
  return location_entry_.get();
}

LocationBarTesting* LocationBarViewGtk::GetLocationBarForTesting() {
  return this;
}

int LocationBarViewGtk::PageActionCount() {
  return page_action_views_.size();
}

int LocationBarViewGtk::PageActionVisibleCount() {
  int count = 0;
  gtk_container_foreach(GTK_CONTAINER(page_action_hbox_.get()),
                        CountVisibleWidgets, &count);
  return count;
}

ExtensionAction* LocationBarViewGtk::GetPageAction(size_t index) {
  if (index >= page_action_views_.size()) {
    NOTREACHED();
    return NULL;
  }

  return page_action_views_[index]->page_action();
}

ExtensionAction* LocationBarViewGtk::GetVisiblePageAction(size_t index) {
  size_t visible_index = 0;
  for (size_t i = 0; i < page_action_views_.size(); ++i) {
    if (page_action_views_[i]->IsVisible()) {
      if (index == visible_index++)
        return page_action_views_[i]->page_action();
    }
  }

  NOTREACHED();
  return NULL;
}

void LocationBarViewGtk::TestPageActionPressed(size_t index) {
  if (index >= page_action_views_.size()) {
    NOTREACHED();
    return;
  }

  page_action_views_[index]->TestActivatePageAction();
}

void LocationBarViewGtk::TestActionBoxMenuItemSelected(int command_id) {
  action_box_button_->action_box_button_controller()->
      ExecuteCommand(command_id);
}

bool LocationBarViewGtk::GetBookmarkStarVisibility() {
  return starred_;
}

void LocationBarViewGtk::Observe(int type,
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

    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      if (theme_service_->UsingNativeTheme()) {
        gtk_widget_modify_bg(tab_to_search_box_, GTK_STATE_NORMAL, NULL);

        GdkColor border_color = theme_service_->GetGdkColor(
            ThemeService::COLOR_FRAME);
        gtk_util::SetRoundedWindowBorderColor(tab_to_search_box_, border_color);

        gtk_util::UndoForceFontSize(security_info_label_);
        gtk_util::UndoForceFontSize(tab_to_search_full_label_);
        gtk_util::UndoForceFontSize(tab_to_search_partial_label_);
        gtk_util::UndoForceFontSize(tab_to_search_hint_leading_label_);
        gtk_util::UndoForceFontSize(tab_to_search_hint_trailing_label_);

        gtk_alignment_set_padding(GTK_ALIGNMENT(location_entry_alignment_),
                                  0, 0, 0, 0);
        gtk_alignment_set_padding(GTK_ALIGNMENT(tab_to_search_alignment_),
                                  1, 1, 1, 0);
        gtk_alignment_set_padding(GTK_ALIGNMENT(site_type_alignment_),
                                  1, 1, 1, 0);
      } else {
        gtk_widget_modify_bg(tab_to_search_box_, GTK_STATE_NORMAL,
                             &kKeywordBackgroundColor);
        gtk_util::SetRoundedWindowBorderColor(tab_to_search_box_,
                                              kKeywordBorderColor);

        // Until we switch to vector graphics, force the font size of labels.
        // 12.1px = 9pt @ 96dpi
        gtk_util::ForceFontSizePixels(security_info_label_, 12.1);
        gtk_util::ForceFontSizePixels(tab_to_search_full_label_,
            browser_defaults::kAutocompleteEditFontPixelSize);
        gtk_util::ForceFontSizePixels(tab_to_search_partial_label_,
            browser_defaults::kAutocompleteEditFontPixelSize);
        gtk_util::ForceFontSizePixels(tab_to_search_hint_leading_label_,
            browser_defaults::kAutocompleteEditFontPixelSize);
        gtk_util::ForceFontSizePixels(tab_to_search_hint_trailing_label_,
            browser_defaults::kAutocompleteEditFontPixelSize);

        const int top_bottom = popup_window_mode_ ? kBorderThickness : 0;
        gtk_alignment_set_padding(GTK_ALIGNMENT(location_entry_alignment_),
                                  kTopMargin + kBorderThickness,
                                  kBottomMargin + kBorderThickness,
                                  top_bottom, top_bottom);
        gtk_alignment_set_padding(GTK_ALIGNMENT(tab_to_search_alignment_),
                                  1, 1, 0, 0);
        gtk_alignment_set_padding(GTK_ALIGNMENT(site_type_alignment_),
                                  1, 1, 0, 0);
      }

      UpdateZoomIcon();
      UpdateScriptBubbleIcon();
      UpdateStarIcon();
      UpdateSiteTypeArea();
      UpdateContentSettingsIcons();
      UpdateWebIntentsButton();
      break;
    }

    default:
      NOTREACHED();
  }
}

gboolean LocationBarViewGtk::HandleExpose(GtkWidget* widget,
                                          GdkEventExpose* event) {
  // If we're not using GTK theming, draw our own border over the edge pixels
  // of the background.
  if (!GtkThemeService::GetFrom(browser_->profile())->UsingNativeTheme()) {
    int left, center, right;
    if (popup_window_mode_) {
      left = right = IDR_LOCATIONBG_POPUPMODE_EDGE;
      center = IDR_LOCATIONBG_POPUPMODE_CENTER;
    } else {
      left = IDR_LOCATIONBG_L;
      center = IDR_LOCATIONBG_C;
      right = IDR_LOCATIONBG_R;
    }

    NineBox background(left, center, right,
                       0, 0, 0, 0, 0, 0);
    background.RenderToWidget(widget);
  }

  // Draw ExtensionAction backgrounds and borders, if necessary.  The borders
  // appear exactly between the elements, so they can't draw the borders
  // themselves.
  gfx::CanvasSkiaPaint canvas(event, /*opaque=*/false);
  for (ScopedVector<PageActionViewGtk>::const_iterator
           page_action_view = page_action_views_.begin();
       page_action_view != page_action_views_.end();
       ++page_action_view) {
    if ((*page_action_view)->IsVisible()) {
      // Figure out where the page action is drawn so we can draw
      // borders to its left and right.
      GtkAllocation allocation;
      gtk_widget_get_allocation((*page_action_view)->widget(), &allocation);
      ExtensionAction* action = (*page_action_view)->page_action();
      gfx::Rect bounds(allocation);
      // Make the bounding rectangle include the whole vertical range of the
      // location bar, and the mid-point pixels between adjacent page actions.
      //
      // For odd InnerPadding()s, "InnerPadding() + 1" includes the mid-point
      // between two page actions in the bounding rectangle.  For even paddings,
      // the +1 is dropped, which is right since there is no pixel at the
      // mid-point.
      bounds.Inset(-(InnerPadding() + 1) / 2,
                   theme_service_->UsingNativeTheme() ? -1 : 0);
      location_bar_util::PaintExtensionActionBackground(
          *action, SessionID::IdForTab(GetWebContents()),
          &canvas, bounds,
          theme_service_->get_location_bar_text_color(),
          theme_service_->get_location_bar_bg_color());
    }
  }
  // Destroying |canvas| draws the background.

  return FALSE;  // Continue propagating the expose.
}

void LocationBarViewGtk::UpdateSiteTypeArea() {
  // The icon is always visible except when the |tab_to_search_alignment_| is
  // visible.
  if (!location_entry_->model()->keyword().empty() &&
      !location_entry_->model()->is_keyword_hint()) {
    gtk_widget_hide(site_type_area());
    return;
  }

  int resource_id = location_entry_->GetIcon();
  gtk_image_set_from_pixbuf(
      GTK_IMAGE(location_icon_image_),
      theme_service_->GetImageNamed(resource_id).ToGdkPixbuf());

  if (toolbar_model_->GetSecurityLevel() == ToolbarModel::EV_SECURE) {
    if (!gtk_util::IsActingAsRoundedWindow(site_type_event_box_)) {
      // Fun fact: If wee try to make |site_type_event_box_| act as a
      // rounded window while it doesn't have a visible window, GTK interprets
      // this as a sign that it should paint the skyline texture into the
      // omnibox.
      gtk_event_box_set_visible_window(GTK_EVENT_BOX(site_type_event_box_),
                                       TRUE);

      gtk_util::ActAsRoundedWindow(site_type_event_box_,
                                   kEvSecureBorderColor,
                                   kCornerSize,
                                   gtk_util::ROUNDED_ALL,
                                   gtk_util::BORDER_ALL);
    }

    string16 info_text = toolbar_model_->GetEVCertName();
    gtk_label_set_text(GTK_LABEL(security_info_label_),
                       UTF16ToUTF8(info_text).c_str());

    UpdateEVCertificateLabelSize();

    gtk_widget_show(GTK_WIDGET(security_info_label_));
  } else {
    if (gtk_util::IsActingAsRoundedWindow(site_type_event_box_)) {
      gtk_util::StopActingAsRoundedWindow(site_type_event_box_);

      gtk_event_box_set_visible_window(GTK_EVENT_BOX(site_type_event_box_),
                                       FALSE);
    }

    gtk_widget_hide(GTK_WIDGET(security_info_label_));
  }

  if (GetLocationEntry()->IsEditingOrEmpty()) {
    // Do not show the tooltip if the user has been editing the location
    // bar, or the location bar is at the NTP.
    gtk_widget_set_tooltip_text(location_icon_image_, "");
  } else {
    gtk_widget_set_tooltip_text(location_icon_image_,
        l10n_util::GetStringUTF8(IDS_TOOLTIP_LOCATION_ICON).c_str());
  }

  gtk_widget_show(site_type_area());

  SetSiteTypeDragSource();
}

void LocationBarViewGtk::UpdateEVCertificateLabelSize() {
  // Figure out the width of the average character.
  PangoLayout* layout = gtk_label_get_layout(GTK_LABEL(security_info_label_));
  PangoContext* context = pango_layout_get_context(layout);
  PangoFontMetrics* metrics = pango_context_get_metrics(
      context,
      gtk_widget_get_style(security_info_label_)->font_desc,
      pango_context_get_language(context));
  int char_width =
      pango_font_metrics_get_approximate_char_width(metrics) / PANGO_SCALE;

  // The EV label should never take up more than half the hbox. We try to
  // correct our inaccurate measurement units ("the average character width")
  // by dividing more than an even 2.
  GtkAllocation security_label_allocation;
  gtk_widget_get_allocation(security_info_label_, &security_label_allocation);
  GtkAllocation entry_box_allocation;
  gtk_widget_get_allocation(entry_box_, &entry_box_allocation);
  int text_area = security_label_allocation.width +
                  entry_box_allocation.width;
  int max_chars = static_cast<int>(static_cast<float>(text_area) /
                                   static_cast<float>(char_width) / 2.75);
  // Don't let the label be smaller than 10 characters so that the country
  // code is always visible.
  gtk_label_set_max_width_chars(GTK_LABEL(security_info_label_),
                                std::max(10, max_chars));

  pango_font_metrics_unref(metrics);
}

void LocationBarViewGtk::SetKeywordLabel(const string16& keyword) {
  if (keyword.empty())
    return;

  Profile* profile = browser_->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service)
    return;

  bool is_extension_keyword;
  const string16 short_name = template_url_service->GetKeywordShortName(
      keyword, &is_extension_keyword);
  const string16 min_string = location_bar_util::CalculateMinString(short_name);
  const string16 full_name = is_extension_keyword ?
      short_name :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, short_name);
  const string16 partial_name = is_extension_keyword ?
      min_string :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, min_string);
  gtk_label_set_text(GTK_LABEL(tab_to_search_full_label_),
                     UTF16ToUTF8(full_name).c_str());
  gtk_label_set_text(GTK_LABEL(tab_to_search_partial_label_),
                     UTF16ToUTF8(partial_name).c_str());

  if (last_keyword_ != keyword) {
    last_keyword_ = keyword;

    if (is_extension_keyword) {
      const TemplateURL* template_url =
          template_url_service->GetTemplateURLForKeyword(keyword);
      gfx::Image image = extensions::OmniboxAPI::Get(profile)->
          GetOmniboxIcon(template_url->GetExtensionId());
      gtk_image_set_from_pixbuf(GTK_IMAGE(tab_to_search_magnifier_),
                                image.ToGdkPixbuf());
    } else {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      gtk_image_set_from_pixbuf(GTK_IMAGE(tab_to_search_magnifier_),
          rb.GetNativeImageNamed(IDR_OMNIBOX_SEARCH).ToGdkPixbuf());
    }
  }
}

void LocationBarViewGtk::SetKeywordHintLabel(const string16& keyword) {
  if (keyword.empty())
    return;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  if (!template_url_service)
    return;

  bool is_extension_keyword;
  const string16 short_name = template_url_service->
      GetKeywordShortName(keyword, &is_extension_keyword);
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
  std::vector<size_t> content_param_offsets;
  const string16 keyword_hint = l10n_util::GetStringFUTF16(
      message_id,
      string16(),
      short_name,
      &content_param_offsets);
  if (content_param_offsets.size() != 2) {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
    return;
  }

  std::string leading(UTF16ToUTF8(
      keyword_hint.substr(0, content_param_offsets.front())));
  std::string trailing(UTF16ToUTF8(
      keyword_hint.substr(content_param_offsets.front())));
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_leading_label_),
                     leading.c_str());
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_trailing_label_),
                     trailing.c_str());
}

void LocationBarViewGtk::ShowFirstRunBubbleInternal() {
  if (!location_entry_.get() || !gtk_widget_get_window(widget()))
    return;

  gfx::Rect bounds = gtk_util::WidgetBounds(location_icon_image_);
  bounds.set_x(bounds.x() + kFirstRunBubbleLeftSpacing);
  FirstRunBubble::Show(browser_, location_icon_image_, bounds);
}

gboolean LocationBarViewGtk::OnIconReleased(GtkWidget* sender,
                                            GdkEventButton* event) {
  WebContents* tab = GetWebContents();

  if (event->button == 1) {
    // Do not show page info if the user has been editing the location
    // bar, or the location bar is at the NTP.
    if (GetLocationEntry()->IsEditingOrEmpty())
      return FALSE;

    // (0,0) event coordinates indicates that the release came at the end of
    // a drag.
    if (event->x == 0 && event->y == 0)
      return FALSE;

    NavigationEntry* nav_entry = tab->GetController().GetActiveEntry();
    if (!nav_entry) {
      NOTREACHED();
      return FALSE;
    }
    chrome::ShowPageInfo(browser_, tab, nav_entry->GetURL(),
                         nav_entry->GetSSL(), true);
    return TRUE;
  } else if (event->button == 2) {
    // When the user middle clicks on the location icon, try to open the
    // contents of the PRIMARY selection in the current tab.
    // If the click was outside our bounds, do nothing.
    if (!gtk_util::WidgetBounds(sender).Contains(
            gfx::Point(event->x, event->y))) {
      return FALSE;
    }

    GURL url;
    if (!gtk_util::URLFromPrimarySelection(browser_->profile(), &url))
      return FALSE;

    tab->OpenURL(OpenURLParams(
        url, content::Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
        false));
    return TRUE;
  }

  return FALSE;
}

void LocationBarViewGtk::OnIconDragData(GtkWidget* sender,
                                        GdkDragContext* context,
                                        GtkSelectionData* data,
                                        guint info, guint time) {
  ui::WriteURLWithName(data, drag_url_, drag_title_, info);
}

void LocationBarViewGtk::OnIconDragBegin(GtkWidget* sender,
                                         GdkDragContext* context) {
  gfx::Image favicon = GetFavicon();
  if (favicon.IsEmpty())
    return;
  drag_icon_ = bookmark_utils::GetDragRepresentation(favicon.ToGdkPixbuf(),
      GetTitle(), theme_service_);
  gtk_drag_set_icon_widget(context, drag_icon_, 0, 0);

  WebContents* tab = GetWebContents();
  if (!tab)
    return;
  drag_url_ = tab->GetURL();
  drag_title_ = tab->GetTitle();
}

void LocationBarViewGtk::OnIconDragEnd(GtkWidget* sender,
                                       GdkDragContext* context) {
  DCHECK(drag_icon_);
  gtk_widget_destroy(drag_icon_);
  drag_icon_ = NULL;
  drag_url_ = GURL::EmptyGURL();
  drag_title_.clear();
}

void LocationBarViewGtk::OnHboxSizeAllocate(GtkWidget* sender,
                                            GtkAllocation* allocation) {
  if (hbox_width_ != allocation->width) {
    hbox_width_ = allocation->width;
    UpdateEVCertificateLabelSize();
  }
}

void LocationBarViewGtk::OnEntryBoxSizeAllocate(GtkWidget* sender,
                                                GtkAllocation* allocation) {
  if (entry_box_width_ != allocation->width) {
    entry_box_width_ = allocation->width;
    AdjustChildrenVisibility();
  }
}

gboolean LocationBarViewGtk::OnZoomButtonPress(GtkWidget* widget,
                                               GdkEventButton* event) {
  if (event->button == 1 && GetWebContents()) {
    // If the zoom icon is clicked, show the zoom bubble and keep it open until
    // it loses focus.
    ZoomBubbleGtk::ShowBubble(GetWebContents(), false);
    return TRUE;
  }
  return FALSE;
}

gboolean LocationBarViewGtk::OnScriptBubbleButtonPress(GtkWidget* widget,
                                                       GdkEventButton* event) {
  if (event->button == 1 && GetWebContents()) {
    ScriptBubbleGtk::Show(script_bubble_button_image_, GetWebContents());
    return TRUE;
  }
  return FALSE;
}

gboolean LocationBarViewGtk::OnScriptBubbleButtonExpose(GtkWidget* widget,
                                                        GdkEventExpose* event) {
  gfx::CanvasSkiaPaint canvas(event, false);
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  badge_util::PaintBadge(&canvas,
                         gfx::Rect(allocation),
                         base::UintToString(num_running_scripts_),
                         SK_ColorWHITE,
                         SkColorSetRGB(0, 170, 0),
                         allocation.width,
                         extensions::ActionInfo::TYPE_PAGE);
  return FALSE;
}

void LocationBarViewGtk::OnStarButtonSizeAllocate(GtkWidget* sender,
                                                  GtkAllocation* allocation) {
  if (!on_star_sized_.is_null()) {
    on_star_sized_.Run();
    on_star_sized_.Reset();
  }
  star_sized_ = true;
}

gboolean LocationBarViewGtk::OnStarButtonPress(GtkWidget* widget,
                                               GdkEventButton* event) {
  if (event->button == 1) {
    chrome::ExecuteCommand(browser_, IDC_BOOKMARK_PAGE);
    return TRUE;
  }
  return FALSE;
}

void LocationBarViewGtk::ShowZoomBubble() {
  if (toolbar_model_->GetInputInProgress() || !GetWebContents())
    return;

  ZoomBubbleGtk::ShowBubble(GetWebContents(), true);
}

void LocationBarViewGtk::ShowStarBubble(const GURL& url,
                                        bool newly_bookmarked) {
  if (!star_.get())
    return;

  if (star_sized_) {
    BookmarkBubbleGtk::Show(star_.get(), browser_->profile(), url,
                            newly_bookmarked);
  } else {
    on_star_sized_ = base::Bind(&BookmarkBubbleGtk::Show,
                                star_.get(), browser_->profile(),
                                url, newly_bookmarked);
  }
}

void LocationBarViewGtk::ShowChromeToMobileBubble() {
  ChromeToMobileBubbleGtk::Show(action_box_button_->widget(), browser_);
}

void LocationBarViewGtk::SetStarred(bool starred) {
  if (starred == starred_)
    return;

  starred_ = starred;
  UpdateStarIcon();
}

void LocationBarViewGtk::ZoomChangedForActiveTab(bool can_show_bubble) {
  UpdateZoomIcon();

  if (can_show_bubble && gtk_widget_get_visible(zoom_.get()))
    ShowZoomBubble();
}

void LocationBarViewGtk::UpdateZoomIcon() {
  WebContents* web_contents = GetWebContents();
  if (!zoom_.get() || !web_contents)
    return;

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  if (!zoom_controller || zoom_controller->IsAtDefaultZoom() ||
      toolbar_model_->GetInputInProgress()) {
    gtk_widget_hide(zoom_.get());
    ZoomBubbleGtk::CloseBubble();
    return;
  }

  const int zoom_resource = zoom_controller->GetResourceForZoomLevel();
  gtk_image_set_from_pixbuf(GTK_IMAGE(zoom_image_),
      theme_service_->GetImageNamed(zoom_resource).ToGdkPixbuf());

  string16 tooltip = l10n_util::GetStringFUTF16Int(
      IDS_TOOLTIP_ZOOM, zoom_controller->zoom_percent());
  gtk_widget_set_tooltip_text(zoom_.get(), UTF16ToUTF8(tooltip).c_str());

  gtk_widget_show(zoom_.get());
}

void LocationBarViewGtk::UpdateScriptBubbleIcon() {
  num_running_scripts_ = 0;
  if (GetWebContents()) {
    extensions::TabHelper* tab_helper =
        extensions::TabHelper::FromWebContents(GetWebContents());
    if (tab_helper && tab_helper->script_bubble_controller()) {
      num_running_scripts_ = tab_helper->script_bubble_controller()->
          extensions_running_scripts().size();
    }
  }

  if (num_running_scripts_ == 0u)
    gtk_widget_hide(script_bubble_button_.get());
  else
    gtk_widget_show(script_bubble_button_.get());
}

void LocationBarViewGtk::UpdateStarIcon() {
  if (!star_.get())
    return;
  // Indicate the star icon is not correctly sized. It will be marked as sized
  // when the next size-allocate signal is received by the star widget.
  star_sized_ = false;
  bool star_enabled = !toolbar_model_->GetInputInProgress() &&
                      edit_bookmarks_enabled_.GetValue();
  command_updater_->UpdateCommandEnabled(IDC_BOOKMARK_PAGE, star_enabled);
  if (extensions::FeatureSwitch::action_box()->IsEnabled() && !starred_) {
    star_enabled = false;
  }
  if (star_enabled) {
    gtk_widget_show_all(star_.get());
    int id = starred_ ? IDR_STAR_LIT : IDR_STAR;
    gtk_image_set_from_pixbuf(GTK_IMAGE(star_image_),
                              theme_service_->GetImageNamed(id).ToGdkPixbuf());
  } else {
    gtk_widget_hide_all(star_.get());
  }
}

bool LocationBarViewGtk::ShouldOnlyShowLocation() {
  return !browser_->is_type_tabbed();
}

void LocationBarViewGtk::AdjustChildrenVisibility() {
  int text_width = location_entry_->TextWidth();
  int available_width = entry_box_width_ - text_width - InnerPadding();

  // Only one of |tab_to_search_alignment_| and |tab_to_search_hint_| can be
  // visible at the same time.
  if (!show_selected_keyword_ &&
      gtk_widget_get_visible(tab_to_search_alignment_)) {
    gtk_widget_hide(tab_to_search_alignment_);
  } else if (!show_keyword_hint_ &&
             gtk_widget_get_visible(tab_to_search_hint_)) {
    gtk_widget_hide(tab_to_search_hint_);
  }

  if (show_selected_keyword_) {
    GtkRequisition box, full_label, partial_label;
    gtk_widget_size_request(tab_to_search_box_, &box);
    gtk_widget_size_request(tab_to_search_full_label_, &full_label);
    gtk_widget_size_request(tab_to_search_partial_label_, &partial_label);
    int full_partial_width_diff = full_label.width - partial_label.width;
    int full_box_width;
    int partial_box_width;
    if (gtk_widget_get_visible(tab_to_search_full_label_)) {
      full_box_width = box.width;
      partial_box_width = full_box_width - full_partial_width_diff;
    } else {
      partial_box_width = box.width;
      full_box_width = partial_box_width + full_partial_width_diff;
    }

    if (partial_box_width >= entry_box_width_ - InnerPadding()) {
      gtk_widget_hide(tab_to_search_alignment_);
    } else if (full_box_width >= available_width) {
      gtk_widget_hide(tab_to_search_full_label_);
      gtk_widget_show(tab_to_search_partial_label_);
      gtk_widget_show(tab_to_search_alignment_);
    } else if (full_box_width < available_width) {
      gtk_widget_hide(tab_to_search_partial_label_);
      gtk_widget_show(tab_to_search_full_label_);
      gtk_widget_show(tab_to_search_alignment_);
    }
  } else if (show_keyword_hint_) {
    GtkRequisition leading, icon, trailing;
    gtk_widget_size_request(tab_to_search_hint_leading_label_, &leading);
    gtk_widget_size_request(tab_to_search_hint_icon_, &icon);
    gtk_widget_size_request(tab_to_search_hint_trailing_label_, &trailing);
    int full_width = leading.width + icon.width + trailing.width;

    if (icon.width >= entry_box_width_ - InnerPadding()) {
      gtk_widget_hide(tab_to_search_hint_);
    } else if (full_width >= available_width) {
      gtk_widget_hide(tab_to_search_hint_leading_label_);
      gtk_widget_hide(tab_to_search_hint_trailing_label_);
      gtk_widget_show(tab_to_search_hint_);
    } else if (full_width < available_width) {
      gtk_widget_show(tab_to_search_hint_leading_label_);
      gtk_widget_show(tab_to_search_hint_trailing_label_);
      gtk_widget_show(tab_to_search_hint_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarViewGtk::PageToolViewGtk

LocationBarViewGtk::PageToolViewGtk::PageToolViewGtk(
    const LocationBarViewGtk* parent)
    : alignment_(gtk_alignment_new(0, 0, 1, 1)),
      event_box_(gtk_event_box_new()),
      hbox_(gtk_hbox_new(FALSE, InnerPadding())),
      image_(gtk_image_new()),
      label_(gtk_label_new(NULL)),
      parent_(parent),
      animation_(this),
      weak_factory_(this) {
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_.get()), 1, 1, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment_.get()), event_box_.get());

  // Make the event box not visible so it does not paint a background.
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  g_signal_connect(event_box_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressedThunk), this);
  g_signal_connect(event_box_.get(), "expose-event",
                   G_CALLBACK(&OnExposeThunk), this);

  gtk_widget_set_no_show_all(label_.get(), TRUE);
  gtk_label_set_line_wrap(GTK_LABEL(label_.get()), FALSE);

  gtk_box_pack_start(GTK_BOX(hbox_), image_.get(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), label_.get(), FALSE, FALSE, 0);

  gtk_container_set_border_width(GTK_CONTAINER(hbox_), kHboxBorder);

  gtk_container_add(GTK_CONTAINER(event_box_.get()), hbox_);
  gtk_widget_hide(widget());

  animation_.SetSlideDuration(kPageToolAnimationTime);
}

LocationBarViewGtk::PageToolViewGtk::~PageToolViewGtk() {
  image_.Destroy();
  label_.Destroy();
  event_box_.Destroy();
  alignment_.Destroy();
}

GtkWidget* LocationBarViewGtk::PageToolViewGtk::widget() {
  return alignment_.get();
}

bool LocationBarViewGtk::PageToolViewGtk::IsVisible() {
  return gtk_widget_get_visible(widget());
}

void LocationBarViewGtk::PageToolViewGtk::StartAnimating() {
  if (animation_.IsShowing() || animation_.IsClosing())
    return;

  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), TRUE);
  GdkColor border_color = button_border_color();
  gtk_util::ActAsRoundedWindow(event_box_.get(), border_color,
                               kCornerSize,
                               gtk_util::ROUNDED_ALL, gtk_util::BORDER_ALL);

  gtk_widget_set_size_request(label_.get(), -1, -1);
  gtk_widget_size_request(label_.get(), &label_req_);
  gtk_widget_set_size_request(label_.get(), 0, -1);
  gtk_widget_show(label_.get());

  animation_.Show();
}

void LocationBarViewGtk::PageToolViewGtk::CloseAnimation() {
  animation_.Hide();
}

void LocationBarViewGtk::PageToolViewGtk::AnimationProgressed(
    const ui::Animation* animation) {
  gtk_widget_set_size_request(
      label_.get(),
      animation->GetCurrentValue() * label_req_.width,
      -1);
}

void LocationBarViewGtk::PageToolViewGtk::AnimationEnded(
    const ui::Animation* animation) {
}

void LocationBarViewGtk::PageToolViewGtk::AnimationCanceled(
    const ui::Animation* animation) {
}

gboolean LocationBarViewGtk::PageToolViewGtk::OnButtonPressed(
    GtkWidget* sender, GdkEvent* event) {
  OnClick(sender);
  return TRUE;
}

gboolean LocationBarViewGtk::PageToolViewGtk::OnExpose(
    GtkWidget* sender, GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "LocationBarViewGtk::PageToolViewGtk::OnExpose");

  if (!(animation_.IsShowing() || animation_.IsClosing()))
    return FALSE;

  GtkAllocation allocation;
  gtk_widget_get_allocation(sender, &allocation);
  const int height = allocation.height;

  cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(sender));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  cairo_pattern_t* pattern = cairo_pattern_create_linear(0, 0, 0, height);

  const GdkColor top_color = gradient_top_color();
  const GdkColor bottom_color = gradient_bottom_color();
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.0,
      top_color.red/255.0,
      top_color.blue/255.0,
      top_color.green/255.0);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0,
      bottom_color.red/255.0,
      bottom_color.blue/255.0,
      bottom_color.green/255.0);

  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_pattern_destroy(pattern);
  cairo_destroy(cr);

  return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarViewGtk::PageActionViewGtk

LocationBarViewGtk::PageActionViewGtk::PageActionViewGtk(
    LocationBarViewGtk* owner,
    ExtensionAction* page_action)
    : owner_(NULL),
      page_action_(page_action),
      current_tab_id_(-1),
      window_(NULL),
      accel_group_(NULL),
      preview_enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(scoped_icon_animation_observer_(
          page_action->GetIconAnimation(
              SessionID::IdForTab(owner->GetWebContents())),
          this)) {
  event_box_.Own(gtk_event_box_new());
  gtk_widget_set_size_request(event_box_.get(),
                              Extension::kPageActionIconMaxSize,
                              Extension::kPageActionIconMaxSize);

  // Make the event box not visible so it does not paint a background.
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  g_signal_connect(event_box_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressedThunk), this);
  g_signal_connect_after(event_box_.get(), "expose-event",
                         G_CALLBACK(OnExposeEventThunk), this);
  g_signal_connect(event_box_.get(), "realize",
                   G_CALLBACK(OnRealizeThunk), this);

  image_.Own(gtk_image_new());
  gtk_container_add(GTK_CONTAINER(event_box_.get()), image_.get());

  const Extension* extension = owner->browser()->profile()->
      GetExtensionService()->GetExtensionById(page_action->extension_id(),
                                              false);
  DCHECK(extension);

  icon_factory_.reset(
      new ExtensionActionIconFactory(extension, page_action, this));

  // We set the owner last of all so that we can determine whether we are in
  // the process of initializing this class or not.
  owner_ = owner;
}

LocationBarViewGtk::PageActionViewGtk::~PageActionViewGtk() {
  DisconnectPageActionAccelerator();

  image_.Destroy();
  event_box_.Destroy();
}

bool LocationBarViewGtk::PageActionViewGtk::IsVisible() {
  return gtk_widget_get_visible(widget());
}

void LocationBarViewGtk::PageActionViewGtk::UpdateVisibility(
    WebContents* contents, const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = contents ? ExtensionTabUtil::GetTabId(contents) : -1;
  current_url_ = url;

  bool visible = contents &&
      (preview_enabled_ || page_action_->GetIsVisible(current_tab_id_));
  if (visible) {
    // Set the tooltip.
    gtk_widget_set_tooltip_text(event_box_.get(),
        page_action_->GetTitle(current_tab_id_).c_str());

    // Set the image.
    gfx::Image icon = icon_factory_->GetIcon(current_tab_id_);
    if (!icon.IsEmpty()) {
      GdkPixbuf* pixbuf = icon.ToGdkPixbuf();
      DCHECK(pixbuf);
      gtk_image_set_from_pixbuf(GTK_IMAGE(image_.get()), pixbuf);
    }
  }

  bool old_visible = IsVisible();
  if (visible)
    gtk_widget_show_all(event_box_.get());
  else
    gtk_widget_hide_all(event_box_.get());

  if (visible != old_visible) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        content::Source<ExtensionAction>(page_action_),
        content::Details<WebContents>(contents));
  }
}

void LocationBarViewGtk::PageActionViewGtk::OnIconUpdated() {
  // If we have no owner, that means this class is still being constructed.
  WebContents* web_contents = owner_ ? owner_->GetWebContents() : NULL;
  if (web_contents)
    UpdateVisibility(web_contents, current_url_);
}

void LocationBarViewGtk::PageActionViewGtk::TestActivatePageAction() {
  GdkEventButton event = {};
  event.type = GDK_BUTTON_PRESS;
  event.button = 1;
  OnButtonPressed(widget(), &event);
}

void LocationBarViewGtk::PageActionViewGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_WINDOW_CLOSED);
  DisconnectPageActionAccelerator();
}

void LocationBarViewGtk::PageActionViewGtk::ConnectPageActionAccelerator() {
  const ExtensionSet* extensions = owner_->browser()->profile()->
      GetExtensionService()->extensions();
  const Extension* extension =
      extensions->GetByID(page_action_->extension_id());
  window_ = owner_->browser()->window()->GetNativeWindow();

  extensions::CommandService* command_service =
      extensions::CommandService::Get(owner_->browser()->profile());

  extensions::Command command_page_action;
  if (command_service->GetPageActionCommand(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &command_page_action,
          NULL)) {
    // Found the page action shortcut command, register it.
    page_action_keybinding_.reset(
        new ui::Accelerator(command_page_action.accelerator()));
  }

  extensions::Command command_script_badge;
  if (command_service->GetScriptBadgeCommand(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &command_script_badge,
          NULL)) {
    // Found the script badge shortcut command, register it.
    script_badge_keybinding_.reset(
        new ui::Accelerator(command_script_badge.accelerator()));
  }

  if (page_action_keybinding_.get() || script_badge_keybinding_.get()) {
    accel_group_ = gtk_accel_group_new();
    gtk_window_add_accel_group(window_, accel_group_);

    if (page_action_keybinding_.get()) {
      gtk_accel_group_connect(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(*page_action_keybinding_),
          ui::GetGdkModifierForAccelerator(*page_action_keybinding_),
          GtkAccelFlags(0),
          g_cclosure_new(G_CALLBACK(OnGtkAccelerator), this, NULL));
    }
    if (script_badge_keybinding_.get()) {
      gtk_accel_group_connect(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(*script_badge_keybinding_),
          ui::GetGdkModifierForAccelerator(*script_badge_keybinding_),
          GtkAccelFlags(0),
          g_cclosure_new(G_CALLBACK(OnGtkAccelerator), this, NULL));
    }

    // Since we've added an accelerator, we'll need to unregister it before
    // the window is closed, so we listen for the window being closed.
    registrar_.Add(this,
                   chrome::NOTIFICATION_WINDOW_CLOSED,
                   content::Source<GtkWindow>(window_));
  }
}

void LocationBarViewGtk::PageActionViewGtk::OnIconChanged() {
  UpdateVisibility(owner_->GetWebContents(), current_url_);
}

void LocationBarViewGtk::PageActionViewGtk::DisconnectPageActionAccelerator() {
  if (accel_group_) {
    if (page_action_keybinding_.get()) {
      gtk_accel_group_disconnect_key(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(*page_action_keybinding_),
          ui::GetGdkModifierForAccelerator(*page_action_keybinding_));
    }
    if (script_badge_keybinding_.get()) {
      gtk_accel_group_disconnect_key(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(*script_badge_keybinding_),
          ui::GetGdkModifierForAccelerator(*script_badge_keybinding_));
    }
    gtk_window_remove_accel_group(window_, accel_group_);
    g_object_unref(accel_group_);
    accel_group_ = NULL;
    page_action_keybinding_.reset(NULL);
    script_badge_keybinding_.reset(NULL);
  }
}

gboolean LocationBarViewGtk::PageActionViewGtk::OnButtonPressed(
    GtkWidget* sender,
    GdkEventButton* event) {
  // Double and triple-clicks generate both a GDK_BUTTON_PRESS and a
  // GDK_[23]BUTTON_PRESS event. We don't want to double-trigger by acting on
  // both.
  if (event->type != GDK_BUTTON_PRESS)
    return TRUE;

  WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return TRUE;

  ExtensionService* extension_service =
      owner_->browser()->profile()->GetExtensionService();
  if (!extension_service)
    return TRUE;

  const Extension* extension =
      extension_service->extensions()->GetByID(page_action()->extension_id());
  if (!extension)
    return TRUE;

  LocationBarController* controller =
      extensions::TabHelper::FromWebContents(web_contents)->
          location_bar_controller();

  switch (controller->OnClicked(extension->id(), event->button)) {
    case LocationBarController::ACTION_NONE:
      break;

    case LocationBarController::ACTION_SHOW_POPUP:
      ExtensionPopupGtk::Show(
          page_action_->GetPopupUrl(current_tab_id_),
          owner_->browser_,
          event_box_.get(),
          ExtensionPopupGtk::SHOW);
      break;

    case LocationBarController::ACTION_SHOW_CONTEXT_MENU:
      context_menu_model_ =
          new ExtensionContextMenuModel(extension, owner_->browser_, this);
      context_menu_.reset(
          new MenuGtk(NULL, context_menu_model_.get()));
      context_menu_->PopupForWidget(sender, event->button, event->time);
      break;

    case LocationBarController::ACTION_SHOW_SCRIPT_POPUP:
      ExtensionPopupGtk::Show(
          ExtensionInfoUI::GetURL(extension->id()),
          owner_->browser_,
          event_box_.get(),
          ExtensionPopupGtk::SHOW);
      break;
  }

  return TRUE;
}

gboolean LocationBarViewGtk::PageActionViewGtk::OnExposeEvent(
    GtkWidget* widget,
    GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "LocationBarViewGtk::PageActionViewGtk::OnExpose");
  WebContents* contents = owner_->GetWebContents();
  if (!contents)
    return FALSE;

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_id < 0)
    return FALSE;

  std::string badge_text = page_action_->GetBadgeText(tab_id);
  if (badge_text.empty())
    return FALSE;

  gfx::CanvasSkiaPaint canvas(event, false);
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  page_action_->PaintBadge(&canvas, gfx::Rect(allocation), tab_id);
  return FALSE;
}

void LocationBarViewGtk::PageActionViewGtk::OnRealize(GtkWidget* widget) {
  ConnectPageActionAccelerator();
}

// static
gboolean LocationBarViewGtk::PageActionViewGtk::OnGtkAccelerator(
    GtkAccelGroup* accel_group,
    GObject* acceleratable,
    guint keyval,
    GdkModifierType modifier,
    void* user_data) {
  PageActionViewGtk* view = static_cast<PageActionViewGtk*>(user_data);
  if (!gtk_widget_get_visible(view->widget()))
    return FALSE;

  GdkEventButton event = {};
  event.type = GDK_BUTTON_PRESS;
  event.button = 1;
  return view->OnButtonPressed(view->widget(), &event);
}

void LocationBarViewGtk::PageActionViewGtk::InspectPopup(
    ExtensionAction* action) {
  ExtensionPopupGtk::Show(
      action->GetPopupUrl(current_tab_id_),
      owner_->browser_,
      event_box_.get(),
      ExtensionPopupGtk::SHOW_AND_INSPECT);
}
