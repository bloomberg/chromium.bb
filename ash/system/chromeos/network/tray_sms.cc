// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_sms.h"

#include "ash/shell.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

// Min height of the list of messages in the popup.
const int kMessageListMinHeight = 200;
// Top/bottom padding of the text items.
const int kPaddingVertical = 10;

const char kSmsNumberKey[] = "number";
const char kSmsTextKey[] = "text";

bool GetMessageFromDictionary(const base::DictionaryValue* message,
                              std::string* number,
                              std::string* text) {
  if (!message->GetStringWithoutPathExpansion(kSmsNumberKey, number))
    return false;
  if (!message->GetStringWithoutPathExpansion(kSmsTextKey, text))
    return false;
  return true;
}

}  // namespace

namespace ash {

class TraySms::SmsDefaultView : public TrayItemMore {
 public:
  explicit SmsDefaultView(TraySms* owner)
      : TrayItemMore(owner, true) {
    SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_SMS));
    Update();
  }

  virtual ~SmsDefaultView() {}

  void Update() {
    int message_count = static_cast<TraySms*>(owner())->messages().GetSize();
    base::string16 label = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SMS_MESSAGES, base::IntToString16(message_count));
    SetLabel(label);
    SetAccessibleName(label);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsDefaultView);
};

// An entry (row) in SmsDetailedView or NotificationView.
class TraySms::SmsMessageView : public views::View,
                                public views::ButtonListener {
 public:
  enum ViewType {
    VIEW_DETAILED,
    VIEW_NOTIFICATION
  };

  SmsMessageView(TraySms* owner,
                 ViewType view_type,
                 size_t index,
                 const std::string& number,
                 const std::string& message)
      : owner_(owner),
        index_(index) {
    number_label_ = new views::Label(
        l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_SMS_NUMBER,
                                   base::UTF8ToUTF16(number)),
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::BoldFont));
    number_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    message_label_ = new views::Label(base::UTF8ToUTF16(message));
    message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_label_->SetMultiLine(true);

    if (view_type == VIEW_DETAILED)
      LayoutDetailedView();
    else
      LayoutNotificationView();
  }

  virtual ~SmsMessageView() {
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    owner_->RemoveMessage(index_);
    owner_->Update(false);
  }

 private:
  void LayoutDetailedView() {
    views::ImageButton* close_button = new views::ImageButton(this);
    close_button->SetImage(
        views::CustomButton::STATE_NORMAL,
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_SMS_DISMISS));
    const int msg_width = owner_->system_tray()->GetSystemBubble()->
        bubble_view()->GetPreferredSize().width() -
            (kNotificationIconWidth + kTrayPopupPaddingHorizontal * 2);
    message_label_->SizeToFit(msg_width);

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    // Message
    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       0 /* resize percent */,
                       views::GridLayout::FIXED, msg_width, msg_width);

    // Close button
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kNotificationIconWidth, kNotificationIconWidth);


    layout->AddPaddingRow(0, kPaddingVertical);
    layout->StartRow(0, 0);
    layout->AddView(number_label_);
    layout->AddView(close_button, 1, 2);  // 2 rows for icon
    layout->StartRow(0, 0);
    layout->AddView(message_label_);

    layout->AddPaddingRow(0, kPaddingVertical);
  }

  void LayoutNotificationView() {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    AddChildView(number_label_);
    message_label_->SizeToFit(kTrayNotificationContentsWidth);
    AddChildView(message_label_);
  }

  TraySms* owner_;
  size_t index_;
  views::Label* number_label_;
  views::Label* message_label_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessageView);
};

class TraySms::SmsDetailedView : public TrayDetailsView,
                                 public ViewClickListener {
 public:
  explicit SmsDetailedView(TraySms* owner)
      : TrayDetailsView(owner) {
    Init();
    Update();
  }

  virtual ~SmsDetailedView() {
  }

  void Init() {
    CreateScrollableList();
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_SMS, this);
  }

  void Update() {
    UpdateMessageList();
    Layout();
    SchedulePaint();
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    gfx::Size preferred_size = TrayDetailsView::GetPreferredSize();
    if (preferred_size.height() < kMessageListMinHeight)
      preferred_size.set_height(kMessageListMinHeight);
    return preferred_size;
  }

 private:
  void UpdateMessageList() {
    const base::ListValue& messages =
        static_cast<TraySms*>(owner())->messages();
    scroll_content()->RemoveAllChildViews(true);
    for (size_t index = 0; index < messages.GetSize(); ++index) {
      const base::DictionaryValue* message = NULL;
      if (!messages.GetDictionary(index, &message)) {
        LOG(ERROR) << "SMS message not a dictionary at: " << index;
        continue;
      }
      std::string number, text;
      if (!GetMessageFromDictionary(message, &number, &text)) {
        LOG(ERROR) << "Error parsing SMS message";
        continue;
      }
      SmsMessageView* msgview = new SmsMessageView(
          static_cast<TraySms*>(owner()), SmsMessageView::VIEW_DETAILED, index,
          number, text);
      scroll_content()->AddChildView(msgview);
    }
    scroller()->Layout();
  }

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE {
    if (sender == footer()->content())
      TransitionToDefaultView();
  }

  DISALLOW_COPY_AND_ASSIGN(SmsDetailedView);
};

class TraySms::SmsNotificationView : public TrayNotificationView {
 public:
  SmsNotificationView(TraySms* owner,
                      size_t message_index,
                      const std::string& number,
                      const std::string& text)
      : TrayNotificationView(owner, IDR_AURA_UBER_TRAY_SMS),
        message_index_(message_index) {
    SmsMessageView* message_view = new SmsMessageView(
        owner, SmsMessageView::VIEW_NOTIFICATION, message_index_, number, text);
    InitView(message_view);
  }

  void Update(size_t message_index,
              const std::string& number,
              const std::string& text) {
    SmsMessageView* message_view = new SmsMessageView(
        tray_sms(), SmsMessageView::VIEW_NOTIFICATION,
        message_index_, number, text);
    UpdateView(message_view);
  }

  // Overridden from TrayNotificationView:
  virtual void OnClose() OVERRIDE {
    tray_sms()->RemoveMessage(message_index_);
  }

  virtual void OnClickAction() OVERRIDE {
    owner()->PopupDetailedView(0, true);
  }

 private:
  TraySms* tray_sms() {
    return static_cast<TraySms*>(owner());
  }

  size_t message_index_;

  DISALLOW_COPY_AND_ASSIGN(SmsNotificationView);
};

TraySms::TraySms(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      detailed_(NULL),
      notification_(NULL) {
  // TODO(armansito): SMS could be a special case for cellular that requires a
  // user (perhaps the owner) to be logged in. If that is the case, then an
  // additional check should be done before subscribing for SMS notifications.
  if (chromeos::NetworkHandler::IsInitialized())
    chromeos::NetworkHandler::Get()->network_sms_handler()->AddObserver(this);
}

TraySms::~TraySms() {
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_sms_handler()->RemoveObserver(
        this);
  }
}

views::View* TraySms::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new SmsDefaultView(this);
  default_->SetVisible(!messages_.empty());
  return default_;
}

views::View* TraySms::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  HideNotificationView();
  if (messages_.empty())
    return NULL;
  detailed_ = new SmsDetailedView(this);
  return detailed_;
}

views::View* TraySms::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_ == NULL);
  if (detailed_)
    return NULL;
  size_t index;
  std::string number, text;
  if (GetLatestMessage(&index, &number, &text))
    notification_ = new SmsNotificationView(this, index, number, text);
  return notification_;
}

void TraySms::DestroyDefaultView() {
  default_ = NULL;
}

void TraySms::DestroyDetailedView() {
  detailed_ = NULL;
}

void TraySms::DestroyNotificationView() {
  notification_ = NULL;
}

void TraySms::MessageReceived(const base::DictionaryValue& message) {

  std::string message_text;
  if (!message.GetStringWithoutPathExpansion(
          chromeos::NetworkSmsHandler::kTextKey, &message_text)) {
    NET_LOG_ERROR("SMS message contains no content.", "");
    return;
  }
  // TODO(armansito): A message might be due to a special "Message Waiting"
  // state that the message is in. Once SMS handling moves to shill, such
  // messages should be filtered there so that this check becomes unnecessary.
  if (message_text.empty()) {
    NET_LOG_DEBUG("SMS has empty content text. Ignoring.", "");
    return;
  }
  std::string message_number;
  if (!message.GetStringWithoutPathExpansion(
          chromeos::NetworkSmsHandler::kNumberKey, &message_number)) {
    NET_LOG_DEBUG("SMS contains no number. Ignoring.", "");
    return;
  }

  NET_LOG_DEBUG("Received SMS from: " + message_number + " with text: " +
                message_text, "");

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString(kSmsNumberKey, message_number);
  dict->SetString(kSmsTextKey, message_text);
  messages_.Append(dict);
  Update(true);
}

bool TraySms::GetLatestMessage(size_t* index,
                               std::string* number,
                               std::string* text) {
  if (messages_.empty())
    return false;
  base::DictionaryValue* message;
  size_t message_index = messages_.GetSize() - 1;
  if (!messages_.GetDictionary(message_index, &message))
    return false;
  if (!GetMessageFromDictionary(message, number, text))
    return false;
  *index = message_index;
  return true;
}

void TraySms::RemoveMessage(size_t index) {
  if (index < messages_.GetSize())
    messages_.Remove(index, NULL);
}

void TraySms::Update(bool notify) {
  if (messages_.empty()) {
    if (default_)
      default_->SetVisible(false);
    if (detailed_)
      HideDetailedView();
    HideNotificationView();
  } else {
    if (default_) {
      default_->SetVisible(true);
      default_->Update();
    }
    if (detailed_)
      detailed_->Update();
    if (notification_) {
      size_t index;
      std::string number, text;
      if (GetLatestMessage(&index, &number, &text))
        notification_->Update(index, number, text);
    } else if (notify) {
      ShowNotificationView();
    }
  }
}

}  // namespace ash
