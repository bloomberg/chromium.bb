// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include "base/utf_string_conversions.h"
#include "chrome/browser/blocked_content_container.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/info_bubble.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(OS_LINUX)
#include "gfx/gtk_util.h"
#endif

// If we don't clamp the maximum width, then very long URLs and titles can make
// the bubble arbitrarily wide.
const int kMaxContentsWidth = 500;

// When we have multiline labels, we should set a minimum width lest we get very
// narrow bubbles with lots of line-wrapping.
const int kMinMultiLineContentsWidth = 250;

class ContentSettingBubbleContents::Favicon : public views::ImageView {
 public:
  Favicon(const SkBitmap& image,
          ContentSettingBubbleContents* parent,
          views::Link* link);
  virtual ~Favicon();

 private:
#if defined(OS_WIN)
  static HCURSOR g_hand_cursor;
#endif

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual gfx::NativeCursor GetCursorForPoint(
      views::Event::EventType event_type,
      const gfx::Point& p);

  ContentSettingBubbleContents* parent_;
  views::Link* link_;
};

#if defined(OS_WIN)
HCURSOR ContentSettingBubbleContents::Favicon::g_hand_cursor = NULL;
#endif

ContentSettingBubbleContents::Favicon::Favicon(
    const SkBitmap& image,
    ContentSettingBubbleContents* parent,
    views::Link* link)
    : parent_(parent),
      link_(link) {
  SetImage(image);
}

ContentSettingBubbleContents::Favicon::~Favicon() {
}

bool ContentSettingBubbleContents::Favicon::OnMousePressed(
    const views::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

void ContentSettingBubbleContents::Favicon::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  if (!canceled &&
      (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
      HitTest(event.location()))
    parent_->LinkActivated(link_, event.GetFlags());
}

gfx::NativeCursor ContentSettingBubbleContents::Favicon::GetCursorForPoint(
    views::Event::EventType event_type,
    const gfx::Point& p) {
#if defined(OS_WIN)
  if (!g_hand_cursor)
    g_hand_cursor = LoadCursor(NULL, IDC_HAND);
  return g_hand_cursor;
#elif defined(OS_LINUX)
  return gfx::GetCursor(GDK_HAND2);
#endif
}

ContentSettingBubbleContents::ContentSettingBubbleContents(
    ContentSettingBubbleModel* content_setting_bubble_model,
    Profile* profile,
    TabContents* tab_contents)
    : content_setting_bubble_model_(content_setting_bubble_model),
      profile_(profile),
      tab_contents_(tab_contents),
      info_bubble_(NULL),
      custom_link_(NULL),
      manage_link_(NULL),
      close_button_(NULL) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
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

void ContentSettingBubbleContents::ViewHierarchyChanged(bool is_add,
                                                        View* parent,
                                                        View* child) {
  if (is_add && (child == this))
    InitControlLayout();
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const views::Event& event) {
  if (sender == close_button_) {
    info_bubble_->set_fade_away_on_close(true);
    info_bubble_->Close();  // CAREFUL: This deletes us.
    return;
  }

  for (RadioGroup::const_iterator i = radio_group_.begin();
       i != radio_group_.end(); ++i) {
    if (sender == *i) {
      content_setting_bubble_model_->OnRadioClicked(i - radio_group_.begin());
      return;
    }
  }
  NOTREACHED() << "unknown radio";
}

void ContentSettingBubbleContents::LinkActivated(views::Link* source,
                                                 int event_flags) {
  if (source == custom_link_) {
    content_setting_bubble_model_->OnCustomLinkClicked();
    info_bubble_->set_fade_away_on_close(true);
    info_bubble_->Close();  // CAREFUL: This deletes us.
    return;
  }
  if (source == manage_link_) {
    info_bubble_->set_fade_away_on_close(true);
    content_setting_bubble_model_->OnManageLinkClicked();
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  PopupLinks::const_iterator i(popup_links_.find(source));
  DCHECK(i != popup_links_.end());
  content_setting_bubble_model_->OnPopupClicked(i->second);
}

void ContentSettingBubbleContents::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}

void ContentSettingBubbleContents::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();
  bool bubble_content_empty = true;

  if (!bubble_content.title.empty()) {
    views::Label* title_label = new views::Label(UTF8ToWide(
        bubble_content.title));
    layout->StartRow(0, single_column_set_id);
    layout->AddView(title_label);
    bubble_content_empty = false;
  }

  const std::set<std::string>& plugins = bubble_content.resource_identifiers;
  if (!plugins.empty()) {
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    for (std::set<std::string>::const_iterator it = plugins.begin();
        it != plugins.end(); ++it) {
      std::wstring name = UTF16ToWide(
          NPAPI::PluginList::Singleton()->GetPluginGroupName(*it));
      if (name.empty())
        name = UTF8ToWide(*it);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(new views::Label(name));
      bubble_content_empty = false;
    }
  }

  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS) {
    const int popup_column_set_id = 2;
    views::ColumnSet* popup_column_set =
        layout->AddColumnSet(popup_column_set_id);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                                GridLayout::USE_PREF, 0, 0);
    popup_column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    popup_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                                GridLayout::USE_PREF, 0, 0);

    for (std::vector<ContentSettingBubbleModel::PopupItem>::const_iterator
         i(bubble_content.popup_items.begin());
         i != bubble_content.popup_items.end(); ++i) {
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
      layout->StartRow(0, popup_column_set_id);

      views::Link* link = new views::Link(UTF8ToWide(i->title));
      link->SetController(this);
      link->SetElideInMiddle(true);
      popup_links_[link] = i - bubble_content.popup_items.begin();
      layout->AddView(new Favicon((*i).bitmap, this, link));
      layout->AddView(link);
      bubble_content_empty = false;
    }
  }

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;
  if (!radio_group.radio_items.empty()) {
    for (ContentSettingBubbleModel::RadioItems::const_iterator i =
         radio_group.radio_items.begin();
         i != radio_group.radio_items.end(); ++i) {
      views::RadioButton* radio = new views::RadioButton(UTF8ToWide(*i), 0);
      radio->set_listener(this);
      radio_group_.push_back(radio);
      if (!bubble_content_empty)
        layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(radio);
      bubble_content_empty = false;
    }
    DCHECK(!radio_group_.empty());
    // Now that the buttons have been added to the view hierarchy, it's safe
    // to call SetChecked() on them.
    radio_group_[radio_group.default_item]->SetChecked(true);
  }

  gfx::Font domain_font =
      views::Label().font().DeriveFont(0, gfx::Font::BOLD);
  const int indented_single_column_set_id = 3;
  // Insert a column set to indent the domain list.
  views::ColumnSet* indented_single_column_set =
      layout->AddColumnSet(indented_single_column_set_id);
  indented_single_column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  indented_single_column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                                        1, GridLayout::USE_PREF, 0, 0);
  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i =
       bubble_content.domain_lists.begin();
       i != bubble_content.domain_lists.end(); ++i) {
    layout->StartRow(0, single_column_set_id);
    views::Label* section_title = new views::Label(UTF8ToWide(i->title));
    section_title->SetMultiLine(true);
    section_title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(section_title, 1, 1, GridLayout::FILL, GridLayout::LEADING);
    for (std::set<std::string>::const_iterator j = i->hosts.begin();
         j != i->hosts.end(); ++j) {
      layout->StartRow(0, indented_single_column_set_id);
      layout->AddView(new views::Label(UTF8ToWide(*j), domain_font));
    }
    bubble_content_empty = false;
  }

  if (!bubble_content.custom_link.empty()) {
    custom_link_ = new views::Link(UTF8ToWide(bubble_content.custom_link));
    custom_link_->SetEnabled(bubble_content.custom_link_enabled);
    custom_link_->SetController(this);
    if (!bubble_content_empty)
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, single_column_set_id);
    layout->AddView(custom_link_);
    bubble_content_empty = false;
  }

  if (!bubble_content_empty) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, single_column_set_id);
    layout->AddView(new views::Separator, 1, 1,
                    GridLayout::FILL, GridLayout::FILL);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  const int double_column_set_id = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(double_column_set_id);
  double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  double_column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_set_id);
  manage_link_ = new views::Link(UTF8ToWide(bubble_content.manage_link));
  manage_link_->SetController(this);
  layout->AddView(manage_link_);

  close_button_ =
      new views::NativeButton(this,
                              UTF16ToWide(l10n_util::GetStringUTF16(IDS_DONE)));
  layout->AddView(close_button_);
}
