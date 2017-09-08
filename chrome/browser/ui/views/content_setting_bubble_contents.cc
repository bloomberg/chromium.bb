// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// If we don't clamp the maximum width, then very long URLs and titles can make
// the bubble arbitrarily wide.
const int kMaxContentsWidth = 500;

// The new default width for the content settings bubble. The review process to
// the width on per-bubble basis is tracked with https://crbug.com/649650.
const int kMaxDefaultContentsWidth = 320;

// When we have multiline labels, we should set a minimum width lest we get very
// narrow bubbles with lots of line-wrapping.
const int kMinMultiLineContentsWidth = 250;

}  // namespace

using content::PluginService;
using content::WebContents;

// ContentSettingBubbleContents::MediaComboboxModel ----------------------------

ContentSettingBubbleContents::MediaComboboxModel::MediaComboboxModel(
    content::MediaStreamType type)
    : type_(type) {
  DCHECK(type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
         type_ == content::MEDIA_DEVICE_VIDEO_CAPTURE);
}

ContentSettingBubbleContents::MediaComboboxModel::~MediaComboboxModel() {}

const content::MediaStreamDevices&
ContentSettingBubbleContents::MediaComboboxModel::GetDevices() const {
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  return type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE
             ? dispatcher->GetAudioCaptureDevices()
             : dispatcher->GetVideoCaptureDevices();
}

int ContentSettingBubbleContents::MediaComboboxModel::GetDeviceIndex(
    const content::MediaStreamDevice& device) const {
  const auto& devices = GetDevices();
  for (size_t i = 0; i < devices.size(); ++i) {
    if (device.id == devices[i].id)
      return i;
  }
  NOTREACHED();
  return 0;
}

int ContentSettingBubbleContents::MediaComboboxModel::GetItemCount() const {
  return std::max(1, static_cast<int>(GetDevices().size()));
}

base::string16 ContentSettingBubbleContents::MediaComboboxModel::GetItemAt(
    int index) {
  return GetDevices().empty()
             ? l10n_util::GetStringUTF16(IDS_MEDIA_MENU_NO_DEVICE_TITLE)
             : base::UTF8ToUTF16(GetDevices()[index].name);
}

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

// ContentSettingBubbleContents::ListItemContainer -----------------------------

class ContentSettingBubbleContents::ListItemContainer : public views::View {
 public:
  explicit ListItemContainer(ContentSettingBubbleContents* parent);

  // Creates and adds child views representing |item|.
  void AddItem(const ContentSettingBubbleModel::ListItem& item);

  // Calling this will delete related children.
  void RemoveRowAtIndex(int index);

  // Returns row index of |link| among list items.
  int GetRowIndexOf(const views::Link* link) const;

 private:
  using Row = std::pair<views::ImageView*, views::Label*>;

  void ResetLayout();
  void AddRowToLayout(const Row& row, bool padding_above);

  ContentSettingBubbleContents* parent_;

  int icon_column_width_;

  // Our controls representing list items, so we can add or remove
  // these dynamically. Each pair represetns one list item.
  std::vector<Row> list_item_views_;

  DISALLOW_COPY_AND_ASSIGN(ListItemContainer);
};

ContentSettingBubbleContents::ListItemContainer::ListItemContainer(
    ContentSettingBubbleContents* parent)
        : parent_(parent), icon_column_width_(0) {
  ResetLayout();
}

void ContentSettingBubbleContents::ListItemContainer::AddItem(
    const ContentSettingBubbleModel::ListItem& item) {
  views::ImageView* icon = nullptr;
  views::Label* label = nullptr;
  if (item.has_link) {
    views::Link* link = new views::Link(item.title);
    link->set_listener(parent_);
    link->SetElideBehavior(gfx::ELIDE_MIDDLE);
    icon = new Favicon(item.image, parent_, link);
    label = link;
  } else {
    icon = new views::ImageView();
    icon->SetImage(item.image.AsImageSkia());
    label = new views::Label(item.title);
  }
  icon_column_width_ = std::max(icon->GetPreferredSize().width(),
                                icon_column_width_);
  list_item_views_.push_back(Row(icon, label));
  AddRowToLayout(list_item_views_.back(), list_item_views_.size() > 1);
}

void ContentSettingBubbleContents::ListItemContainer::RemoveRowAtIndex(
    int index) {
  auto& children = list_item_views_[index];
  delete children.first;
  delete children.second;
  list_item_views_.erase(list_item_views_.begin() + index);

  // As views::GridLayout can't remove rows, we have to rebuild it entirely.
  ResetLayout();
  for (size_t i = 0; i < list_item_views_.size(); i++)
    AddRowToLayout(list_item_views_[i], i > 0);
}

int ContentSettingBubbleContents::ListItemContainer::GetRowIndexOf(
    const views::Link* link) const {
  auto has_link = [link](const Row& row) { return row.second == link; };
  auto iter = std::find_if(list_item_views_.begin(), list_item_views_.end(),
                           has_link);
  return (iter == list_item_views_.end())
             ? -1
             : std::distance(list_item_views_.begin(), iter);
}

void ContentSettingBubbleContents::ListItemContainer::ResetLayout() {
  using views::GridLayout;
  GridLayout* layout = GridLayout::CreateAndInstall(this);
  views::ColumnSet* item_list_column_set = layout->AddColumnSet(0);
  item_list_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                                  GridLayout::USE_PREF, 0, 0);
  const int related_control_horizontal_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  item_list_column_set->AddPaddingColumn(0, related_control_horizontal_spacing);
  item_list_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                                  GridLayout::USE_PREF, 0, 0);
}

void ContentSettingBubbleContents::ListItemContainer::AddRowToLayout(
    const Row& row,
    bool padding_above) {
  views::GridLayout* layout =
      static_cast<views::GridLayout*>(GetLayoutManager());
  DCHECK(layout);
  if (padding_above) {
    const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL);
    layout->AddPaddingRow(0, vertical_padding);
  }
  layout->StartRow(0, 0);
  layout->AddView(row.first);
  layout->AddView(row.second);
}

// ContentSettingBubbleContents -----------------------------------------------

ContentSettingBubbleContents::ContentSettingBubbleContents(
    ContentSettingBubbleModel* content_setting_bubble_model,
    content::WebContents* web_contents,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : content::WebContentsObserver(web_contents),
      BubbleDialogDelegateView(anchor_view, arrow),
      content_setting_bubble_model_(content_setting_bubble_model),
      list_item_container_(nullptr),
      custom_link_(nullptr),
      manage_checkbox_(nullptr),
      learn_more_button_(nullptr) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CONTENT_SETTING_CONTENTS);
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
  // Must remove the children here so the comboboxes get destroyed before
  // their associated models.
  RemoveAllChildViews(true);
}

gfx::Size ContentSettingBubbleContents::CalculatePreferredSize() const {
  gfx::Size preferred_size(views::View::CalculatePreferredSize());
  int preferred_width =
      (!content_setting_bubble_model_->bubble_content().domain_lists.empty() &&
       (kMinMultiLineContentsWidth > preferred_size.width()))
          ? kMinMultiLineContentsWidth
          : preferred_size.width();
  if (content_setting_bubble_model_->AsSubresourceFilterBubbleModel()) {
    preferred_size.set_width(std::min(preferred_width,
                                      kMaxDefaultContentsWidth));
  } else {
    preferred_size.set_width(std::min(preferred_width, kMaxContentsWidth));
  }
  return preferred_size;
}

void ContentSettingBubbleContents::OnListItemAdded(
    const ContentSettingBubbleModel::ListItem& item) {
  DCHECK(list_item_container_);
  list_item_container_->AddItem(item);
  SizeToContents();
}

void ContentSettingBubbleContents::OnListItemRemovedAt(int index) {
  DCHECK(list_item_container_);
  list_item_container_->RemoveRowAtIndex(index);
  SizeToContents();
}

void ContentSettingBubbleContents::OnNativeThemeChanged(
    const ui::NativeTheme* theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(theme);
  if (learn_more_button_)
    StyleLearnMoreButton(theme);
}

void ContentSettingBubbleContents::Init() {
  using views::GridLayout;

  GridLayout* layout = views::GridLayout::CreateAndInstall(this);
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int related_control_horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int related_control_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int unrelated_control_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

  const int kSingleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_control_horizontal_spacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  bool bubble_content_empty = true;

  if (!bubble_content.title.empty()) {
    const int title_context =
        provider->IsHarmonyMode()
            ? static_cast<int>(views::style::CONTEXT_DIALOG_TITLE)
            : CONTEXT_BODY_TEXT_SMALL;
    views::Label* title_label =
        new views::Label(bubble_content.title, title_context);
    title_label->SetMultiLine(true);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(title_label);
    bubble_content_empty = false;
  }

  if (!bubble_content.message.empty()) {
    views::Label* message_label = new views::Label(bubble_content.message);
    // For bubble's without titles there is no need for padding.
    if (!bubble_content.title.empty())
      layout->AddPaddingRow(0, unrelated_control_vertical_spacing);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(message_label);
    bubble_content_empty = false;
  }

  // Layout for the item list (blocked plugins and popups).
  if (!bubble_content.list_items.empty()) {
    const int kItemListColumnSetId = 2;
    views::ColumnSet* column_set = layout->AddColumnSet(kItemListColumnSetId);
    column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);

    for (const ContentSettingBubbleModel::ListItem& list_item :
         bubble_content.list_items) {
      if (!list_item_container_) {
        if (!bubble_content_empty)
          layout->AddPaddingRow(0, related_control_vertical_spacing);

        list_item_container_ = new ListItemContainer(this);
        layout->StartRow(0, kItemListColumnSetId);
        layout->AddView(list_item_container_);
      }

      list_item_container_->AddItem(list_item);
      bubble_content_empty = false;
    }
  }

  const int kIndentedSingleColumnSetId = 3;
  // Insert a column set with greater indent.
  views::ColumnSet* indented_single_column_set =
      layout->AddColumnSet(kIndentedSingleColumnSetId);
  indented_single_column_set->AddPaddingColumn(
      0, provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT));
  indented_single_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                                        1, GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;
  if (!radio_group.radio_items.empty()) {
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, related_control_vertical_spacing);
    for (ContentSettingBubbleModel::RadioItems::const_iterator i(
         radio_group.radio_items.begin());
         i != radio_group.radio_items.end(); ++i) {
      views::RadioButton* radio = new views::RadioButton(*i, 0);
      radio->SetEnabled(bubble_content.radio_group_enabled);
      radio->set_listener(this);
      radio_group_.push_back(radio);
      layout->StartRow(0, kIndentedSingleColumnSetId);
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
    menu_column_set->AddPaddingColumn(
        0, provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT));
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                               GridLayout::USE_PREF, 0, 0);
    menu_column_set->AddPaddingColumn(0, related_control_horizontal_spacing);
    menu_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                               GridLayout::USE_PREF, 0, 0);

    for (ContentSettingBubbleModel::MediaMenuMap::const_iterator i(
         bubble_content.media_menus.begin());
         i != bubble_content.media_menus.end(); ++i) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, related_control_vertical_spacing);
      layout->StartRow(0, kMediaMenuColumnSetId);

      views::Label* label = new views::Label(i->second.label);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      layout->AddView(label);

      combobox_models_.emplace_back(i->first);
      MediaComboboxModel* model = &combobox_models_.back();
      views::Combobox* combobox = new views::Combobox(model);
      // Disable the device selection when the website is managing the devices
      // itself or if there are no devices present.
      combobox->SetEnabled(
          !(i->second.disabled || model->GetDevices().empty()));
      combobox->set_listener(this);
      combobox->SetSelectedIndex(
          model->GetDevices().empty()
              ? 0
              : model->GetDeviceIndex(i->second.selected_device));
      layout->AddView(combobox);

      bubble_content_empty = false;
    }
  }

  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i(
           bubble_content.domain_lists.begin());
       i != bubble_content.domain_lists.end(); ++i) {
    layout->StartRow(0, kSingleColumnSetId);
    views::Label* section_title = new views::Label(i->title);
    section_title->SetMultiLine(true);
    section_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(section_title, 1, 1, GridLayout::FILL, GridLayout::LEADING);
    for (std::set<std::string>::const_iterator j = i->hosts.begin();
         j != i->hosts.end(); ++j) {
      layout->StartRow(0, kIndentedSingleColumnSetId);
      // TODO(tapted): Verify this when we have a mock. http://crbug.com/700196.
      layout->AddView(new views::Label(
          base::UTF8ToUTF16(*j), CONTEXT_BODY_TEXT_LARGE, STYLE_EMPHASIZED));
    }
    bubble_content_empty = false;
  }

  if (!bubble_content.custom_link.empty()) {
    custom_link_ = new views::Link(bubble_content.custom_link);
    custom_link_->SetEnabled(bubble_content.custom_link_enabled);
    custom_link_->set_listener(this);
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, related_control_vertical_spacing);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(custom_link_);
    bubble_content_empty = false;
  }

  if (bubble_content.show_manage_text_as_checkbox) {
    layout->AddPaddingRow(0, related_control_vertical_spacing);
    layout->StartRow(0, kIndentedSingleColumnSetId);
    manage_checkbox_ = new views::Checkbox(bubble_content.manage_text);
    manage_checkbox_->set_listener(this);
    layout->AddView(manage_checkbox_);
  }

  if (!bubble_content_empty && !provider->IsHarmonyMode()) {
    layout->AddPaddingRow(0, related_control_vertical_spacing);
    layout->StartRow(0, kSingleColumnSetId);
    layout->AddView(new views::Separator(), 1, 1, GridLayout::FILL,
                    GridLayout::FILL);
    layout->AddPaddingRow(0, related_control_vertical_spacing);
  }

  if (list_item_container_)
    content_setting_bubble_model_->set_owner(this);
}

views::View* ContentSettingBubbleContents::CreateExtraView() {
  // Optionally add a help icon if the view wants to link to a help page.
  if (!content_setting_bubble_model_->bubble_content().show_learn_more)
    return nullptr;

  learn_more_button_ = views::CreateVectorImageButton(this);
  learn_more_button_->SetFocusForPlatform();
  learn_more_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  StyleLearnMoreButton(GetNativeTheme());
  return learn_more_button_;
}

// Note: The cancel button is really the "Manage" button.
bool ContentSettingBubbleContents::Cancel() {
  content_setting_bubble_model_->OnManageButtonClicked();
  return true;
}

bool ContentSettingBubbleContents::Accept() {
  content_setting_bubble_model_->OnDoneClicked();
  return true;
}

bool ContentSettingBubbleContents::Close() {
  return true;
}

int ContentSettingBubbleContents::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_OK;

  // Use the CANCEL button as a manage button.
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  if (!bubble_content.manage_text.empty() &&
      !bubble_content.show_manage_text_as_checkbox) {
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  }
  return buttons;
}

base::string16 ContentSettingBubbleContents::GetDialogButtonLabel(
    ui::DialogButton button) const {
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  if (button == ui::DIALOG_BUTTON_OK) {
    return bubble_content.done_button_text.empty()
               ? l10n_util::GetStringUTF16(IDS_DONE)
               : bubble_content.done_button_text;
  }
  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  DCHECK(!bubble_content.show_manage_text_as_checkbox);
  DCHECK(!bubble_content.manage_text.empty());
  return bubble_content.manage_text;
}

void ContentSettingBubbleContents::StyleLearnMoreButton(
    const ui::NativeTheme* theme) {
  DCHECK(learn_more_button_);
  SkColor text_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);
  learn_more_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(vector_icons::kHelpOutlineIcon, 16,
                            color_utils::DeriveDefaultIconColor(text_color)));
  learn_more_button_->set_ink_drop_base_color(text_color);
}

void ContentSettingBubbleContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Content settings are based on the main frame, so if it switches then
  // close up shop.
  GetWidget()->Close();
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == manage_checkbox_) {
    content_setting_bubble_model_->OnManageCheckboxChecked(
        manage_checkbox_->checked());

    // Toggling the check state may change the dialog button text.
    GetDialogClientView()->UpdateDialogButtons();
    GetDialogClientView()->Layout();
  } else if (sender == learn_more_button_) {
    content_setting_bubble_model_->OnLearnMoreClicked();
    GetWidget()->Close();
  } else {
    RadioGroup::const_iterator i(
        std::find(radio_group_.begin(), radio_group_.end(), sender));
    DCHECK(i != radio_group_.end());
    content_setting_bubble_model_->OnRadioClicked(i - radio_group_.begin());
  }
}

void ContentSettingBubbleContents::LinkClicked(views::Link* source,
                                               int event_flags) {
  if (source == custom_link_) {
    content_setting_bubble_model_->OnCustomLinkClicked();
    GetWidget()->Close();
    return;
  }
  int row = list_item_container_->GetRowIndexOf(source);
  DCHECK_NE(row, -1);
  content_setting_bubble_model_->OnListItemClicked(row, event_flags);
}

void ContentSettingBubbleContents::OnPerformAction(views::Combobox* combobox) {
  MediaComboboxModel* model =
      static_cast<MediaComboboxModel*>(combobox->model());
  content_setting_bubble_model_->OnMediaMenuClicked(
      model->type(), model->GetDevices()[combobox->selected_index()].id);
}
