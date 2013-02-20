// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace {

// If we don't clamp the maximum width, then very long URLs and titles can make
// the bubble arbitrarily wide.
const int kMaxContentsWidth = 500;

// When we have multiline labels, we should set a minimum width lest we get very
// narrow bubbles with lots of line-wrapping.
const int kMinMultiLineContentsWidth = 250;

// The minimum and maximum width of the media menu buttons.
const int kMinMediaMenuButtonWidth = 100;
const int kMaxMediaMenuButtonWidth = 600;

}

using content::PluginService;
using content::WebContents;


// ContentSettingBubbleContents::Favicon --------------------------------------

class ContentSettingBubbleContents::Favicon : public views::ImageView {
 public:
  Favicon(const gfx::Image& image,
          ContentSettingBubbleContents* parent,
          views::Link* link);
  virtual ~Favicon();

 private:
  // views::View overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;

  ContentSettingBubbleContents* parent_;
  views::Link* link_;
};

ContentSettingBubbleContents::Favicon::Favicon(
    const gfx::Image& image,
    ContentSettingBubbleContents* parent,
    views::Link* link)
    : parent_(parent),
      link_(link) {
  SetImage(image.AsImageSkia());
}

ContentSettingBubbleContents::Favicon::~Favicon() {
}

bool ContentSettingBubbleContents::Favicon::OnMousePressed(
    const ui::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

void ContentSettingBubbleContents::Favicon::OnMouseReleased(
    const ui::MouseEvent& event) {
  if ((event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
     HitTestPoint(event.location())) {
    parent_->LinkClicked(link_, event.flags());
  }
}

gfx::NativeCursor ContentSettingBubbleContents::Favicon::GetCursor(
    const ui::MouseEvent& event) {
#if defined(USE_AURA)
  return ui::kCursorHand;
#elif defined(OS_WIN)
  static HCURSOR g_hand_cursor = LoadCursor(NULL, IDC_HAND);
  return g_hand_cursor;
#endif
}


// ContentSettingBubbleContents::MediaMenuParts -------------------------------

struct ContentSettingBubbleContents::MediaMenuParts {
  explicit MediaMenuParts(content::MediaStreamType type);
  ~MediaMenuParts();

  content::MediaStreamType type;
  scoped_ptr<ui::SimpleMenuModel> menu_model;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaMenuParts);
};

ContentSettingBubbleContents::MediaMenuParts::MediaMenuParts(
    content::MediaStreamType type)
    : type(type) {}

ContentSettingBubbleContents::MediaMenuParts::~MediaMenuParts() {}


// ContentSettingBubbleContents -----------------------------------------------

ContentSettingBubbleContents::ContentSettingBubbleContents(
    ContentSettingBubbleModel* content_setting_bubble_model,
    WebContents* web_contents,
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location)
    : BubbleDelegateView(anchor_view, arrow_location),
      content_setting_bubble_model_(content_setting_bubble_model),
      web_contents_(web_contents),
      custom_link_(NULL),
      manage_link_(NULL),
      close_button_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_insets(gfx::Insets(5, 0, 5, 0));

  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(web_contents));
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
  STLDeleteValues(&media_menus_);
}

gfx::Size ContentSettingBubbleContents::GetPreferredSize() {
  gfx::Size preferred_size(views::View::GetPreferredSize());
  int preferred_width =
      (!content_setting_bubble_model_->bubble_content().domain_lists.empty() &&
       (kMinMultiLineContentsWidth > preferred_size.width())) ?
      kMinMultiLineContentsWidth : preferred_size.width();
  preferred_size.set_width(std::min(preferred_width, kMaxContentsWidth));
  return preferred_size;
}

void ContentSettingBubbleContents::UpdateMenuLabel(
    content::MediaStreamType type,
    const std::string& label) {
  for (MediaMenuPartsMap::const_iterator it = media_menus_.begin();
       it != media_menus_.end(); ++it) {
    if (it->second->type == type) {
      it->first->SetText(UTF8ToUTF16(label));
      return;
    }
  }
  NOTREACHED();
}

void ContentSettingBubbleContents::Init() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int kSingleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  bool bubble_content_empty = true;

  if (!bubble_content.title.empty()) {
    views::Label* title_label = new views::Label(UTF8ToUTF16(
        bubble_content.title));
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(title_label);
    bubble_content_empty = false;
  }

  const std::set<std::string>& plugins = bubble_content.resource_identifiers;
  if (!plugins.empty()) {
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    PluginFinder* finder = PluginFinder::GetInstance();
    for (std::set<std::string>::const_iterator i(plugins.begin());
         i != plugins.end(); ++i) {
      string16 name = finder->FindPluginNameWithIdentifier(*i);
      layout->StartRow(0, kSingleColumnSetId);
      layout->AddView(new views::Label(name));
      bubble_content_empty = false;
    }
  }

  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS) {
    const int kPopupColumnSetId = 2;
    views::ColumnSet* popup_column_set =
        layout->AddColumnSet(kPopupColumnSetId);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                                GridLayout::USE_PREF, 0, 0);
    popup_column_set->AddPaddingColumn(
        0, views::kRelatedControlHorizontalSpacing);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                                GridLayout::USE_PREF, 0, 0);

    for (std::vector<ContentSettingBubbleModel::PopupItem>::const_iterator
         i(bubble_content.popup_items.begin());
         i != bubble_content.popup_items.end(); ++i) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kPopupColumnSetId);

      views::Link* link = new views::Link(UTF8ToUTF16(i->title));
      link->set_listener(this);
      link->SetElideBehavior(views::Label::ELIDE_IN_MIDDLE);
      popup_links_[link] = i - bubble_content.popup_items.begin();
      layout->AddView(new Favicon(i->image, this, link));
      layout->AddView(link);
      bubble_content_empty = false;
    }
  }

  const int indented_kSingleColumnSetId = 3;
  // Insert a column set with greater indent.
  views::ColumnSet* indented_single_column_set =
      layout->AddColumnSet(indented_kSingleColumnSetId);
  indented_single_column_set->AddPaddingColumn(
      0, chrome_style::kCheckboxIndent);
  indented_single_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                                        1, GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;
  if (!radio_group.radio_items.empty()) {
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    for (ContentSettingBubbleModel::RadioItems::const_iterator i(
         radio_group.radio_items.begin());
         i != radio_group.radio_items.end(); ++i) {
      views::RadioButton* radio = new views::RadioButton(UTF8ToUTF16(*i), 0);
      radio->SetEnabled(bubble_content.radio_group_enabled);
      radio->set_listener(this);
      radio_group_.push_back(radio);
      layout->StartRow(0, indented_kSingleColumnSetId);
      layout->AddView(radio);
      bubble_content_empty = false;
    }
    DCHECK(!radio_group_.empty());
    // Now that the buttons have been added to the view hierarchy, it's safe
    // to call SetChecked() on them.
    radio_group_[radio_group.default_item]->SetChecked(true);
  }

  // Layout code for the media device menus.
  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    const int kMediaMenuColumnSetId = 2;
    views::ColumnSet* menu_column_set =
        layout->AddColumnSet(kMediaMenuColumnSetId);
    menu_column_set->AddPaddingColumn(
        0, chrome_style::kCheckboxIndent);
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                               GridLayout::USE_PREF, 0, 0);
    menu_column_set->AddPaddingColumn(
        0, views::kRelatedControlHorizontalSpacing);
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                               GridLayout::USE_PREF, 0, 0);

    int menu_width = 0;
    for (ContentSettingBubbleModel::MediaMenuMap::const_iterator i(
         bubble_content.media_menus.begin());
         i != bubble_content.media_menus.end(); ++i) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kMediaMenuColumnSetId);

      views::Label* label = new views::Label(UTF8ToUTF16(i->second.label));
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

      views::MenuButton* menu_button = new views::MenuButton(
          NULL, UTF8ToUTF16((i->second.selected_device.name)), this, false);
      menu_button->set_alignment(views::TextButton::ALIGN_LEFT);
      menu_button->set_border(
          new views::TextButtonNativeThemeBorder(menu_button));
      menu_button->set_animate_on_state_change(false);

      MediaMenuParts* menu_view = new MediaMenuParts(i->first);
      menu_view->menu_model.reset(new ContentSettingMediaMenuModel(
          i->first,
          content_setting_bubble_model_.get(),
          base::Bind(&ContentSettingBubbleContents::UpdateMenuLabel,
                     base::Unretained(this))));
      media_menus_[menu_button] = menu_view;

      // Use the longest width of the menus as the width of the menu buttons.
      menu_width = std::max(menu_width,
                            GetPreferredMediaMenuWidth(
                                menu_button, menu_view->menu_model.get()));

      layout->AddView(label);
      layout->AddView(menu_button);

      bubble_content_empty = false;
    }

    // Make sure the width is within [kMinMediaMenuButtonWidth,
    // kMaxMediaMenuButtonWidth].
    menu_width = std::max(kMinMediaMenuButtonWidth, menu_width);
    menu_width = std::min(kMaxMediaMenuButtonWidth, menu_width);

    // Set all the menu buttons to the width we calculated above.
    for (MediaMenuPartsMap::const_iterator i = media_menus_.begin();
         i != media_menus_.end(); ++i) {
      i->first->set_min_width(menu_width);
      i->first->set_max_width(menu_width);
    }
  }

  gfx::Font domain_font =
      views::Label().font().DeriveFont(0, gfx::Font::BOLD);
  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i(
       bubble_content.domain_lists.begin());
       i != bubble_content.domain_lists.end(); ++i) {
    layout->StartRow(0, kSingleColumnSetId);
    views::Label* section_title = new views::Label(UTF8ToUTF16(i->title));
    section_title->SetMultiLine(true);
    section_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(section_title, 1, 1, GridLayout::FILL, GridLayout::LEADING);
    for (std::set<std::string>::const_iterator j = i->hosts.begin();
         j != i->hosts.end(); ++j) {
      layout->StartRow(0, indented_kSingleColumnSetId);
      layout->AddView(new views::Label(UTF8ToUTF16(*j), domain_font));
    }
    bubble_content_empty = false;
  }

  if (!bubble_content.custom_link.empty()) {
    custom_link_ = new views::Link(UTF8ToUTF16(bubble_content.custom_link));
    custom_link_->SetEnabled(bubble_content.custom_link_enabled);
    custom_link_->set_listener(this);
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(custom_link_);
    bubble_content_empty = false;
  }

  if (!bubble_content_empty) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(new views::Separator, 1, 1,
                    GridLayout::FILL, GridLayout::FILL);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  const int kDoubleColumnSetId = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(kDoubleColumnSetId);
  double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  double_column_set->AddPaddingColumn(
      0, views::kUnrelatedControlHorizontalSpacing);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kDoubleColumnSetId);
  manage_link_ = new views::Link(UTF8ToUTF16(bubble_content.manage_link));
  manage_link_->set_listener(this);
  layout->AddView(manage_link_);

  close_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));
  layout->AddView(close_button_);
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == close_button_) {
    content_setting_bubble_model_->OnDoneClicked();
    StartFade(false);
    return;
  }

  for (RadioGroup::const_iterator i(radio_group_.begin());
       i != radio_group_.end(); ++i) {
    if (sender == *i) {
      content_setting_bubble_model_->OnRadioClicked(i - radio_group_.begin());
      return;
    }
  }
  NOTREACHED() << "unknown radio";
}

void ContentSettingBubbleContents::LinkClicked(views::Link* source,
                                               int event_flags) {
  if (source == custom_link_) {
    content_setting_bubble_model_->OnCustomLinkClicked();
    StartFade(false);
    return;
  }
  if (source == manage_link_) {
    StartFade(false);
    content_setting_bubble_model_->OnManageLinkClicked();
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  PopupLinks::const_iterator i(popup_links_.find(source));
  DCHECK(i != popup_links_.end());
  content_setting_bubble_model_->OnPopupClicked(i->second);
}

void ContentSettingBubbleContents::OnMenuButtonClicked(
    views::View* source,
    const gfx::Point& point) {
  MediaMenuPartsMap::iterator i(media_menus_.find(
      static_cast<views::MenuButton*>(source)));
  DCHECK(i != media_menus_.end());

  views::MenuModelAdapter menu_model_adapter(i->second->menu_model.get());
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(i->first, &screen_location);
  ignore_result(menu_runner_->RunMenuAt(
      source->GetWidget(),
      i->first,
      gfx::Rect(screen_location, i->first->size()),
      views::MenuItemView::TOPLEFT,
      views::MenuRunner::HAS_MNEMONICS));
}

void ContentSettingBubbleContents::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);
  DCHECK(source == content::Source<WebContents>(web_contents_));
  web_contents_ = NULL;
}

int ContentSettingBubbleContents::GetPreferredMediaMenuWidth(
    views::MenuButton* button,
    ui::SimpleMenuModel* menu_model) {
  string16 title = button->text();

  int width = button->GetPreferredSize().width();
  for (int i = 0; i < menu_model->GetItemCount(); ++i) {
    button->SetText(menu_model->GetLabelAt(i));
    width = std::max(width, button->GetPreferredSize().width());
  }

  // Recover the title for the menu button.
  button->SetText(title);
  return width;
}
