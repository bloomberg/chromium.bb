// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file exists to aggregate all of the javascript used by the
// settings page into a single file which will be flattened and served
// as a single resource.
<include src="preferences.js"></include>
<include src="pref_ui.js"></include>
<include src="deletable_item_list.js"></include>
<include src="inline_editable_list.js"></include>
<include src="controlled_setting.js"></include>
<include src="options_page.js"></include>
<if expr="pp_ifdef('chromeos')">
  <include src="about_page.js"></include>
  <include src="../chromeos/user_images_grid.js"></include>
  <include src="chromeos/cellular_plan_element.js"></include>
  <include src="chromeos/change_picture_options.js"></include>
  <include src="chromeos/internet_detail_ip_config_list.js"></include>
  <include src="chromeos/internet_network_element.js"></include>
  <include src="chromeos/internet_options.js"></include>
  <include src="chromeos/internet_detail.js"></include>
  <include src="chromeos/system_options.js"></include>
  <include src="chromeos/bluetooth_list_element.js"></include>
  <include src="chromeos/accounts_options.js"></include>
  <include src="chromeos/proxy_options.js"></include>
  <include src="chromeos/proxy_rules_list.js"></include>
  <include src="chromeos/accounts_user_list.js"></include>
  <include src="chromeos/accounts_user_name_edit.js"></include>
  <include src="chromeos/virtual_keyboard.js"></include>
  <include src="chromeos/virtual_keyboard_list.js"></include>
  var AboutPage = options.AboutPage;
  var AccountsOptions = options.AccountsOptions;
  var ChangePictureOptions = options.ChangePictureOptions;
  var InternetOptions = options.InternetOptions;
  var DetailsInternetPage = options.internet.DetailsInternetPage;
  var SystemOptions = options.SystemOptions;
</if>
<if expr="not pp_ifdef('win32') and not pp_ifdef('darwin')">
  <include src="certificate_tree.js"></include>
  <include src="certificate_manager.js"></include>
  <include src="certificate_restore_overlay.js"></include>
  <include src="certificate_backup_overlay.js"></include>
  <include src="certificate_edit_ca_trust_overlay.js"></include>
  <include src="certificate_import_error_overlay.js"></include>
  var CertificateManager = options.CertificateManager;
  var CertificateRestoreOverlay = options.CertificateRestoreOverlay;
  var CertificateBackupOverlay = options.CertificateBackupOverlay;
  var CertificateEditCaTrustOverlay = options.CertificateEditCaTrustOverlay;
  var CertificateImportErrorOverlay = options.CertificateImportErrorOverlay;
</if>
<include src="advanced_options.js"></include>
<include src="alert_overlay.js"></include>
<include src="autocomplete_list.js"></include>
<include src="autofill_edit_address_overlay.js"></include>
<include src="autofill_edit_creditcard_overlay.js"></include>
<include src="autofill_options_list.js"></include>
<include src="autofill_options.js"></include>
<include src="browser_options.js"></include>
<include src="browser_options_startup_page_list.js"></include>
<include src="clear_browser_data_overlay.js"></include>
<include src="content_settings.js"></include>
<include src="content_settings_exceptions_area.js"></include>
<include src="content_settings_ui.js"></include>
<include src="cookies_list.js"></include>
<include src="cookies_view.js"></include>
<include src="extension_list.js"></include>
<include src="extension_settings.js"></include>
<include src="font_settings.js"></include>
<if expr="pp_ifdef('enable_register_protocol_handler')">
  <include src="handler_options.js"></script>
  <include src="handler_options_list.js"></script>
</if>
<include src="import_data_overlay.js"></include>
<include src="instant_confirm_overlay.js"></include>
<if expr="pp_ifdef('enable_web_intents')">
  <include src="intents_list.js"></include>
  <include src="intents_view.js"></include>
</if>
<include src="language_add_language_overlay.js"></include>
<include src="language_list.js"></include>
<include src="language_options.js"></include>
<include src="manage_profile_overlay.js"></include>
<include src="pack_extension_overlay.js"></include>
<include src="password_manager.js"></include>
<include src="password_manager_list.js"></include>
<include src="personal_options.js"></include>
<include src="personal_options_profile_list.js"></include>
<include src="profiles_icon_grid.js"></include>
<include src="search_engine_manager.js"></include>
<include src="search_engine_manager_engine_list.js"></include>
<include src="search_page.js"></include>
<include src="../sync_setup_overlay.js"></include>
<include src="options.js"></include>
