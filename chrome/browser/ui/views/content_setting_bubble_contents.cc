// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/native_cursor.h"

namespace {

// If we don't clamp the maximum width, then very long URLs and titles can make
// the bubble arbitrarily wide.
const int kMaxContentsWidth = 500;

// When we have multiline labels, we should set a minimum width lest we get very
// narrow bubbles with lots of line-wrapping.
const int kMinMultiLineContentsWidth = 250;

// The minimum width of the media menu buttons.
const int kMinMediaMenuButtonWidth = 150;

}  // namespace

using content::PluginService;
using content::WebContents;


// ContentSettingBubbleContents::Favicon --------------------------------------

class ContentSettingBubbleContents::Favicon : public views::ImageView {
 public:
  Favicon(const gfx::Image& image,
          ContentSettingBubbleContents* parent,
          views::Link* link);
  ~Favicon() override;

 private:
  // views::View overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;

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
  return views::GetNativeHandCursor();
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
    content::WebContents* web_contents,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : content::WebContentsObserver(web_contents),
      BubbleDelegateView(anchor_view, arrow),
      content_setting_bubble_model_(content_setting_bubble_model),
      custom_link_(NULL),
      manage_link_(NULL),
      learn_more_link_(NULL),
      close_button_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
  STLDeleteValues(&media_menus_);
}

gfx::Size ContentSettingBubbleContents::GetPreferredSize() const {
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
      it->first->SetText(base::UTF8ToUTF16(label));
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
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  bool bubble_content_empty = true;

  if (!bubble_content.title.empty()) {
    views::Label* title_label = new views::Label(base::UTF8ToUTF16(
        bubble_content.title));
    title_label->SetMultiLine(true);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(title_label);
    bubble_content_empty = false;
  }

  if (!bubble_content.learn_more_link.empty()) {
    learn_more_link_ =
        new views::Link(base::UTF8ToUTF16(bubble_content.learn_more_link));
    learn_more_link_->set_listener(this);
    learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(learn_more_link_);
    bubble_content_empty = false;
  }

  // Layout for the item list (blocked plugins and popups).
  if (!bubble_content.list_items.empty()) {
    const int kItemListColumnSetId = 2;
    views::ColumnSet* item_list_column_set =
        layout->AddColumnSet(kItemListColumnSetId);
    item_list_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                                    GridLayout::USE_PREF, 0, 0);
    item_list_column_set->AddPaddingColumn(
        0, views::kRelatedControlHorizontalSpacing);
    item_list_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                                    GridLayout::USE_PREF, 0, 0);

    int row = 0;
    for (const ContentSettingBubbleModel::ListItem& list_item :
         bubble_content.list_items) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kItemListColumnSetId);
      if (list_item.has_link) {
        views::Link* link = new views::Link(base::UTF8ToUTF16(list_item.title));
        link->set_listener(this);
        link->SetElideBehavior(gfx::ELIDE_MIDDLE);
        list_item_links_[link] = row;
        layout->AddView(new Favicon(list_item.image, this, link));
        layout->AddView(link);
      } else {
        views::ImageView* icon = new views::ImageView();
        icon->SetImage(list_item.image.AsImageSkia());
        layout->AddView(icon);
        layout->AddView(new views::Label(base::UTF8ToUTF16(list_item.title)));
      }
      row++;
      bubble_content_empty = false;
    }
  }

  const int indented_kSingleColumnSetId = 3;
  // Insert a column set with greater indent.
  views::ColumnSet* indented_single_column_set =
      layout->AddColumnSet(indented_kSingleColumnSetId);
  indented_single_column_set->AddPaddingColumn(0, views::kCheckboxIndent);
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
      views::RadioButton* radio =
          new views::RadioButton(base::UTF8ToUTF16(*i), 0);
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
  if (content_setting_bubble_model_->AsMediaStreamBubbleModel()) {
    const int kMediaMenuColumnSetId = 4;
    views::ColumnSet* menu_column_set =
        layout->AddColumnSet(kMediaMenuColumnSetId);
    menu_column_set->AddPaddingColumn(0, views::kCheckboxIndent);
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                               GridLayout::USE_PREF, 0, 0);
    menu_column_set->AddPaddingColumn(
        0, views::kRelatedControlHorizontalSpacing);
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                               GridLayout::USE_PREF, 0, 0);

    for (ContentSettingBubbleModel::MediaMenuMap::const_iterator i(
         bubble_content.media_menus.begin());
         i != bubble_content.media_menus.end(); ++i) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kMediaMenuColumnSetId);

      views::Label* label =
          new views::Label(base::UTF8ToUTF16(i->second.label));
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

      views::MenuButton* menu_button = new views::MenuButton(
          base::UTF8ToUTF16((i->second.selected_device.name)), this, true);
      menu_button->SetStyle(views::Button::STYLE_BUTTON);
      menu_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      menu_button->set_animate_on_state_change(false);

      MediaMenuParts* menu_view = new MediaMenuParts(i->first);
      menu_view->menu_model.reset(new ContentSettingMediaMenuModel(
          i->first,
          content_setting_bubble_model_.get(),
          base::Bind(&ContentSettingBubbleContents::UpdateMenuLabel,
                     base::Unretained(this))));
      media_menus_[menu_button] = menu_view;

      if (!menu_view->menu_model->GetItemCount()) {
        // Show a "None available" title and grey out the menu when there are
        // no available devices.
        menu_button->SetText(
            l10n_util::GetStringUTF16(IDS_MEDIA_MENU_NO_DEVICE_TITLE));
        menu_button->SetEnabled(false);
      }

      // Disable the device selection when the website is managing the devices
      // itself.
      if (i->second.disabled)
        menu_button->SetEnabled(false);

      layout->AddView(label);
      layout->AddView(menu_button);

      bubble_content_empty = false;
    }
  }

  UpdateMenuButtonSizes();

  const gfx::FontList& domain_font =
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BoldFont);
  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i(
           bubble_content.domain_lists.begin());
       i != bubble_content.domain_lists.end(); ++i) {
    layout->StartRow(0, kSingleColumnSetId);
    views::Label* section_title = new views::Label(base::UTF8ToUTF16(i->title));
    section_title->SetMultiLine(true);
    section_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(section_title, 1, 1, GridLayout::FILL, GridLayout::LEADING);
    for (std::set<std::string>::const_iterator j = i->hosts.begin();
         j != i->hosts.end(); ++j) {
      layout->StartRow(0, indented_kSingleColumnSetId);
      layout->AddView(new views::Label(base::UTF8ToUTF16(*j), domain_font));
    }
    bubble_content_empty = false;
  }

  if (!bubble_content.custom_link.empty()) {
    custom_link_ =
        new views::Link(base::UTF8ToUTF16(bubble_content.custom_link));
    custom_link_->SetEnabled(bubble_content.custom_link_enabled);
    custom_link_->set_listener(this);
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(custom_link_);
    bubble_content_empty = false;
  }

  const int kDoubleColumnSetId = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(kDoubleColumnSetId);
  if (!bubble_content_empty) {
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kSingleColumnSetId);
      layout->AddView(new views::Separator(views::Separator::HORIZONTAL), 1, 1,
                      GridLayout::FILL, GridLayout::FILL);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }

    double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                                 GridLayout::USE_PREF, 0, 0);
    double_column_set->AddPaddingColumn(
        0, views::kUnrelatedControlHorizontalSpacing);
    double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                                 GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, kDoubleColumnSetId);
    manage_link_ =
        new views::Link(base::UTF8ToUTF16(bubble_content.manage_link));
    manage_link_->set_listener(this);
    layout->AddView(manage_link_);

    close_button_ =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
    close_button_->SetStyle(views::Button::STYLE_BUTTON);
    layout->AddView(close_button_);
}

void ContentSettingBubbleContents::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Content settings are based on the main frame, so if it switches then
  // close up shop.
  content_setting_bubble_model_->OnDoneClicked();
  GetWidget()->Close();
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  RadioGroup::const_iterator i(
      std::find(radio_group_.begin(), radio_group_.end(), sender));
  if (i != radio_group_.end()) {
    content_setting_bubble_model_->OnRadioClicked(i - radio_group_.begin());
    return;
  }
  DCHECK_EQ(sender, close_button_);
  content_setting_bubble_model_->OnDoneClicked();
  GetWidget()->Close();
}

void ContentSettingBubbleContents::LinkClicked(views::Link* source,
                                               int event_flags) {
  if (source == learn_more_link_) {
    content_setting_bubble_model_->OnLearnMoreLinkClicked();
    GetWidget()->Close();
    return;
  }
  if (source == custom_link_) {
    content_setting_bubble_model_->OnCustomLinkClicked();
    GetWidget()->Close();
    return;
  }
  if (source == manage_link_) {
    GetWidget()->Close();
    content_setting_bubble_model_->OnManageLinkClicked();
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  ListItemLinks::const_iterator i(list_item_links_.find(source));
  DCHECK(i != list_item_links_.end());
  content_setting_bubble_model_->OnListItemClicked(i->second);
}

void ContentSettingBubbleContents::OnMenuButtonClicked(
    views::View* source,
    const gfx::Point& point) {
    MediaMenuPartsMap::iterator j(media_menus_.find(
        static_cast<views::MenuButton*>(source)));
    DCHECK(j != media_menus_.end());
    menu_runner_.reset(new views::MenuRunner(j->second->menu_model.get(),
                                             views::MenuRunner::HAS_MNEMONICS));

    gfx::Point screen_location;
    views::View::ConvertPointToScreen(j->first, &screen_location);
    ignore_result(
        menu_runner_->RunMenuAt(source->GetWidget(),
                                j->first,
                                gfx::Rect(screen_location, j->first->size()),
                                views::MENU_ANCHOR_TOPLEFT,
                                ui::MENU_SOURCE_NONE));
}

void ContentSettingBubbleContents::UpdateMenuButtonSizes() {
  const views::MenuConfig& config = views::MenuConfig::instance();
  const int margins = config.item_left_margin + config.check_width +
                      config.label_to_arrow_padding + config.arrow_width +
                      config.arrow_to_edge_padding;

  // The preferred media menu size sort of copies the logic in
  // MenuItemView::CalculateDimensions(). When this was using TextButton, it
  // completely coincidentally matched the logic in MenuItemView. We now need
  // to redo this manually.
  int menu_width = 0;
  for (MediaMenuPartsMap::const_iterator i = media_menus_.begin();
       i != media_menus_.end(); ++i) {
    for (int j = 0; j < i->second->menu_model->GetItemCount(); ++j) {
      int string_width = gfx::GetStringWidth(
          i->second->menu_model->GetLabelAt(j),
          config.font_list);

      menu_width = std::max(menu_width, string_width);
    }
  }

  // Make sure the width is at least kMinMediaMenuButtonWidth. The
  // maximum width will be clamped by kMaxContentsWidth of the view.
  menu_width = std::max(kMinMediaMenuButtonWidth, menu_width + margins);

  for (MediaMenuPartsMap::const_iterator i = media_menus_.begin();
       i != media_menus_.end(); ++i) {
    i->first->SetMinSize(gfx::Size(menu_width, 0));
    i->first->SetMaxSize(gfx::Size(menu_width, 0));
  }
}
