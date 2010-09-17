// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_filter_page_view.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_exceptions_table_model.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/exceptions_view.h"
#include "chrome/browser/views/options/simple_content_exceptions_view.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
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
  column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  static const int kTitleIDs[] = {
    IDS_MODIFY_COOKIE_STORING_LABEL,
    IDS_IMAGES_SETTING_LABEL,
    IDS_JS_SETTING_LABEL,
    IDS_PLUGIN_SETTING_LABEL,
    IDS_POPUP_SETTING_LABEL,
    IDS_GEOLOCATION_SETTING_LABEL,
    IDS_NOTIFICATIONS_SETTING_LABEL,
  };
  COMPILE_ASSERT(arraysize(kTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  views::Label* title_label = new views::Label(
      l10n_util::GetString(kTitleIDs[content_type_]));
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  static const int kAllowIDs[] = {
    IDS_COOKIES_ALLOW_RADIO,
    IDS_IMAGES_LOAD_RADIO,
    IDS_JS_ALLOW_RADIO,
    IDS_PLUGIN_LOAD_RADIO,
    IDS_POPUP_ALLOW_RADIO,
    IDS_GEOLOCATION_ALLOW_RADIO,
    IDS_NOTIFICATIONS_ALLOW_RADIO,
  };
  COMPILE_ASSERT(arraysize(kAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  const int radio_button_group = 0;
  allow_radio_ = new views::RadioButton(
      l10n_util::GetString(kAllowIDs[content_type_]), radio_button_group);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  static const int kAskIDs[] = {
    IDS_COOKIES_ASK_EVERY_TIME_RADIO,
    0,
    0,
    IDS_PLUGIN_LOAD_SANDBOXED_RADIO,
    0,
    IDS_GEOLOCATION_ASK_RADIO,
    IDS_NOTIFICATIONS_ASK_RADIO,
  };
  COMPILE_ASSERT(arraysize(kAskIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  DCHECK_EQ(arraysize(kAskIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  if (content_type_ != CONTENT_SETTINGS_TYPE_COOKIES) {
    if (kAskIDs[content_type_] != 0) {
      ask_radio_ = new views::RadioButton(
          l10n_util::GetString(kAskIDs[content_type_]), radio_button_group);
      ask_radio_->set_listener(this);
      ask_radio_->SetMultiLine(true);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(ask_radio_);
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
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
  };
  COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 Need_a_setting_for_every_content_settings_type);
  block_radio_ = new views::RadioButton(
      l10n_util::GetString(kBlockIDs[content_type_]), radio_button_group);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  ContentSetting default_setting;
  if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS) {
    default_setting = profile()->GetHostContentSettingsMap()->
        GetDefaultContentSetting(content_type_);
    if (profile()->GetHostContentSettingsMap()->GetBlockNonsandboxedPlugins())
      default_setting = CONTENT_SETTING_ASK;
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    default_setting = profile()->GetGeolocationContentSettingsMap()->
        GetDefaultContentSetting();
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting = profile()->GetDesktopNotificationService()->
        GetDefaultContentSetting();
  } else {
    default_setting = profile()->GetHostContentSettingsMap()->
        GetDefaultContentSetting(content_type_);
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

  exceptions_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(exceptions_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
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
  if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS) {
    if (default_setting == CONTENT_SETTING_ASK) {
      default_setting = CONTENT_SETTING_ALLOW;
      profile()->GetHostContentSettingsMap()->SetBlockNonsandboxedPlugins(true);
    } else {
      profile()->GetHostContentSettingsMap()->
          SetBlockNonsandboxedPlugins(false);
    }
    profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        content_type_, default_setting);
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
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
