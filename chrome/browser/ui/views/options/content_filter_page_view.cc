// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/content_filter_page_view.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_exceptions_table_model.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/options/exceptions_view.h"
#include "chrome/browser/ui/views/options/simple_content_exceptions_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/radio_button.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

ContentFilterPageView::ContentFilterPageView(Profile* profile,
                                             ContentSettingsType content_type)
    : OptionsPageView(profile),
      content_type_(content_type),
      allow_radio_(NULL),
      ask_radio_(NULL),
      block_radio_(NULL),
      exceptions_button_(NULL) {
}

ContentFilterPageView::~ContentFilterPageView() {
}

////////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, OptionsPageView implementation:

void ContentFilterPageView::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddPaddingColumn(0, views::kRelatedControlVerticalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  static const int kTitleIDs[] = {
    IDS_MODIFY_COOKIE_STORING_LABEL,
    IDS_IMAGES_SETTING_LABEL,
    IDS_JS_SETTING_LABEL,
    IDS_PLUGIN_SETTING_LABEL,
    IDS_POPUP_SETTING_LABEL,
    IDS_GEOLOCATION_SETTING_LABEL,
    IDS_NOTIFICATIONS_SETTING_LABEL,
    0,
  };
  COMPILE_ASSERT(arraysize(kTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  views::Label* title_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(kTitleIDs[content_type_])));
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  static const int kAllowIDs[] = {
    IDS_COOKIES_ALLOW_RADIO,
    IDS_IMAGES_LOAD_RADIO,
    IDS_JS_ALLOW_RADIO,
    IDS_PLUGIN_LOAD_RADIO,
    IDS_POPUP_ALLOW_RADIO,
    IDS_GEOLOCATION_ALLOW_RADIO,
    IDS_NOTIFICATIONS_ALLOW_RADIO,
    0,
  };
  COMPILE_ASSERT(arraysize(kAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  const int radio_button_group = 0;
  allow_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(kAllowIDs[content_type_])),
      radio_button_group);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  static const int kAskIDs[] = {
    IDS_COOKIES_ASK_EVERY_TIME_RADIO,
    0,
    0,
    IDS_PLUGIN_ASK_RADIO,
    0,
    IDS_GEOLOCATION_ASK_RADIO,
    IDS_NOTIFICATIONS_ASK_RADIO,
    0,
  };
  COMPILE_ASSERT(arraysize(kAskIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  if (content_type_ != CONTENT_SETTINGS_TYPE_COOKIES) {
    int askID = kAskIDs[content_type_];
    if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableClickToPlay)) {
      askID = 0;
    }
    if (askID != 0) {
      ask_radio_ = new views::RadioButton(
          UTF16ToWide(l10n_util::GetStringUTF16(askID)), radio_button_group);
      ask_radio_->set_listener(this);
      ask_radio_->SetMultiLine(true);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(ask_radio_);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }
  }

  static const int kBlockIDs[] = {
    IDS_COOKIES_BLOCK_RADIO,
    IDS_IMAGES_NOLOAD_RADIO,
    IDS_JS_DONOTALLOW_RADIO,
    IDS_PLUGIN_NOLOAD_RADIO,
    IDS_POPUP_BLOCK_RADIO,
    IDS_GEOLOCATION_BLOCK_RADIO,
    IDS_NOTIFICATIONS_BLOCK_RADIO,
    0,
  };
  COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  block_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(kBlockIDs[content_type_])),
      radio_button_group);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  exceptions_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_EXCEPTIONS_BUTTON)));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(exceptions_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);

  UpdateView();

  registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::GEOLOCATION_SETTINGS_CHANGED,
      NotificationService::AllSources());
}

///////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView,
void ContentFilterPageView::UpdateView() {
  ContentSetting default_setting;
  bool is_content_type_managed = false;
  if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    default_setting = profile()->GetGeolocationContentSettingsMap()->
        GetDefaultContentSetting();
    is_content_type_managed = profile()->GetGeolocationContentSettingsMap()->
        IsDefaultContentSettingManaged();
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting = profile()->GetDesktopNotificationService()->
        GetDefaultContentSetting();
    is_content_type_managed = profile()->GetDesktopNotificationService()->
        IsDefaultContentSettingManaged();
  } else {
    default_setting = profile()->GetHostContentSettingsMap()->
        GetDefaultContentSetting(content_type_);
    is_content_type_managed = profile()->GetHostContentSettingsMap()->
        IsDefaultContentSettingManaged(content_type_);
  }
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    allow_radio_->SetChecked(true);
  } else if (default_setting == CONTENT_SETTING_ASK) {
    DCHECK(ask_radio_ != NULL);
    ask_radio_->SetChecked(true);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_BLOCK);
    block_radio_->SetChecked(true);
  }

  // Set enable state of the buttons.
  allow_radio_->SetEnabled(!is_content_type_managed);
  if (ask_radio_)
    ask_radio_->SetEnabled(!is_content_type_managed);
  block_radio_->SetEnabled(!is_content_type_managed);
  exceptions_button_->SetEnabled(!is_content_type_managed);
}

///////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, views::ButtonListener implementation:

void ContentFilterPageView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == exceptions_button_) {
    if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
      SimpleContentExceptionsView::ShowExceptionsWindow(
          GetWindow()->GetNativeWindow(),
          new GeolocationExceptionsTableModel(
              profile()->GetGeolocationContentSettingsMap()),
          IDS_GEOLOCATION_EXCEPTION_TITLE);
    } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
      SimpleContentExceptionsView::ShowExceptionsWindow(
          GetWindow()->GetNativeWindow(),
          new NotificationExceptionsTableModel(
              profile()->GetDesktopNotificationService()),
          IDS_NOTIFICATIONS_EXCEPTION_TITLE);
    } else {
      HostContentSettingsMap* settings = profile()->GetHostContentSettingsMap();
      HostContentSettingsMap* otr_settings =
          profile()->HasOffTheRecordProfile() ?
              profile()->GetOffTheRecordProfile()->GetHostContentSettingsMap() :
              NULL;
      if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS &&
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableResourceContentSettings)) {
        PluginExceptionsTableModel* model =
            new PluginExceptionsTableModel(settings, otr_settings);
        model->LoadSettings();
        SimpleContentExceptionsView::ShowExceptionsWindow(
            GetWindow()->GetNativeWindow(),
            model,
            IDS_PLUGINS_EXCEPTION_TITLE);
      } else {
        ExceptionsView::ShowExceptionsWindow(GetWindow()->GetNativeWindow(),
                                             settings,
                                             otr_settings,
                                             content_type_);
      }
    }
    return;
  }

  DCHECK((sender == allow_radio_) || (sender == ask_radio_) ||
         (sender == block_radio_));
  ContentSetting default_setting = allow_radio_->checked() ?
      CONTENT_SETTING_ALLOW :
      (block_radio_->checked() ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ASK);
  if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    profile()->GetGeolocationContentSettingsMap()->SetDefaultContentSetting(
        default_setting);
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    profile()->GetDesktopNotificationService()->SetDefaultContentSetting(
        default_setting);
  } else {
    profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        content_type_, default_setting);
  }
}

void ContentFilterPageView::NotifyContentSettingsChanged(
    const ContentSettingsDetails* details) {
  if (details->type() == CONTENT_SETTINGS_TYPE_DEFAULT ||
      details->type() == content_type_) {
    UpdateView();
  }
}

void ContentFilterPageView::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (type == NotificationType::CONTENT_SETTINGS_CHANGED) {
    NotifyContentSettingsChanged(
        Details<ContentSettingsDetails>(details).ptr());
  } else if (type == NotificationType::GEOLOCATION_SETTINGS_CHANGED) {
    NotifyContentSettingsChanged(
        Details<ContentSettingsDetails>(details).ptr());
  } else if (type == NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED) {
    ContentSettingsDetails content_settings_details(
        ContentSettingsPattern(),
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        "");
    NotifyContentSettingsChanged(&content_settings_details);
  } else {
    OptionsPageBase::Observe(type, source, details);
  }
}
