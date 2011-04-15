// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

void ParamTraits<ContentSetting>::Write(Message* m, const param_type& p) {
  m->WriteInt(static_cast<int>(p));
}

bool ParamTraits<ContentSetting>::Read(const Message* m, void** iter,
                                       param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<ContentSetting>::Log(const param_type& p, std::string* l) {
  std::string content_setting;
  switch (p) {
    case CONTENT_SETTING_DEFAULT:
      content_setting = "CONTENT_SETTING_DEFAULT";
      break;
    case CONTENT_SETTING_ALLOW:
      content_setting = "CONTENT_SETTING_ALLOW";
      break;
    case CONTENT_SETTING_BLOCK:
      content_setting = "CONTENT_SETTING_BLOCK";
      break;
    case CONTENT_SETTING_ASK:
      content_setting = "CONTENT_SETTING_ASK";
      break;
    case CONTENT_SETTING_SESSION_ONLY:
      content_setting = "CONTENT_SETTING_SESSION_ONLY";
      break;
    default:
      content_setting = "UNKNOWN";
      break;
  }
  LogParam(content_setting, l);
}

void ParamTraits<ContentSettingsType>::Write(Message* m, const param_type& p) {
  m->WriteInt(static_cast<int>(p));
}

bool ParamTraits<ContentSettingsType>::Read(const Message* m, void** iter,
                                       param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<ContentSettingsType>::Log(const param_type& p,
                                           std::string* l) {
  std::string setting_type;
  switch (p) {
    case CONTENT_SETTINGS_TYPE_DEFAULT:
      setting_type = "CONTENT_SETTINGS_TYPE_DEFAULT";
      break;
    case CONTENT_SETTINGS_TYPE_COOKIES:
      setting_type = "CONTENT_SETTINGS_TYPE_COOKIES";
      break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
      setting_type = "CONTENT_SETTINGS_TYPE_IMAGES";
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      setting_type = "CONTENT_SETTINGS_TYPE_JAVASCRIPT";
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      setting_type = "CONTENT_SETTINGS_TYPE_PLUGINS";
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      setting_type = "CONTENT_SETTINGS_TYPE_POPUPS";
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      setting_type = "CONTENT_SETTINGS_TYPE_GEOLOCATION";
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      setting_type = "CONTENT_SETTINGS_TYPE_NOTIFICATIONS";
      break;
    case CONTENT_SETTINGS_TYPE_PRERENDER:
      setting_type = "CONTENT_SETTINGS_TYPE_PRERENDER";
      break;
    default:
      setting_type = "UNKNOWN";
      break;
  }
  LogParam(setting_type, l);
}

}  // namespace IPC
