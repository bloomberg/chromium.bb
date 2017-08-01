// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * All possible contentSettingsTypes that we currently support configuring in
 * the UI. Both top-level categories and content settings that represent
 * individual permissions under Site Details should appear here. This is a
 * subset of the constants found in site_settings_helper.cc and the values
 * should be kept in sync.
 * @enum {string}
 */
settings.ContentSettingsTypes = {
  COOKIES: 'cookies',
  IMAGES: 'images',
  JAVASCRIPT: 'javascript',
  PLUGINS: 'plugins',  // AKA Flash.
  POPUPS: 'popups',
  GEOLOCATION: 'location',
  NOTIFICATIONS: 'notifications',
  MIC: 'media-stream-mic',  // AKA Microphone.
  CAMERA: 'media-stream-camera',
  PROTOCOL_HANDLERS: 'register-protocol-handler',
  UNSANDBOXED_PLUGINS: 'ppapi-broker',
  AUTOMATIC_DOWNLOADS: 'multiple-automatic-downloads',
  BACKGROUND_SYNC: 'background-sync',
  MIDI_DEVICES: 'midi-sysex',
  USB_DEVICES: 'usb-chooser-data',
  ZOOM_LEVELS: 'zoom-levels',
  // <if expr="chromeos">
  PROTECTED_CONTENT: 'protectedContent',
  // </if>
  ADS: 'ads',
};

/**
 * Contains the possible string values for a given ContentSettingsTypes.
 * This should be kept in sync with the |ContentSetting| enum in
 * components/content_settings/core/common/content_settings.h
 * @enum {string}
 */
settings.ContentSetting = {
  DEFAULT: 'default',
  ALLOW: 'allow',
  BLOCK: 'block',
  ASK: 'ask',
  SESSION_ONLY: 'session_only',
  IMPORTANT_CONTENT: 'detect_important_content',
};

/**
 * Contains the possible sources of a ContentSetting.
 * This should be kept in sync with the |SiteSettingSource| enum in
 * chrome/browser/ui/webui/site_settings_helper.h
 * @enum {string}
 */
settings.SiteSettingSource = {
  EMBARGO: 'embargo',
  EXTENSION: 'extension',
  INSECURE_ORIGIN: 'insecure-origin',
  KILL_SWITCH: 'kill-switch',
  POLICY: 'policy',
  PREFERENCE: 'preference',
  DEFAULT: 'default',
};

/**
 * A category value to use for the All Sites list.
 * @const {string}
 */
settings.ALL_SITES = 'all-sites';

/**
 * An invalid subtype value.
 * @const {string}
 */
settings.INVALID_CATEGORY_SUBTYPE = '';
