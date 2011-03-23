// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/web_apps.h"
#include "content/common/common_param_traits.h"
#include "googleurl/src/gurl.h"
#include "printing/backend/print_backend.h"
#include "printing/native_metafile.h"
#include "printing/page_range.h"
#include "webkit/glue/password_form.h"

namespace IPC {

void ParamTraits<ContentSetting>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<ContentSetting>::Read(const Message* m, void** iter,
                                       param_type* r) {
  int value;
  if (!ReadParam(m, iter, &value))
    return false;
  if (value < 0 || value >= static_cast<int>(CONTENT_SETTING_NUM_SETTINGS))
    return false;
  *r = static_cast<param_type>(value);
  return true;
}

void ParamTraits<ContentSetting>::Log(const param_type& p, std::string* l) {
  LogParam(static_cast<int>(p), l);
}

void ParamTraits<ContentSettings>::Write(
    Message* m, const ContentSettings& settings) {
  for (size_t i = 0; i < arraysize(settings.settings); ++i)
    WriteParam(m, settings.settings[i]);
}

bool ParamTraits<ContentSettings>::Read(
    const Message* m, void** iter, ContentSettings* r) {
  for (size_t i = 0; i < arraysize(r->settings); ++i) {
    if (!ReadParam(m, iter, &r->settings[i]))
      return false;
  }
  return true;
}

void ParamTraits<ContentSettings>::Log(
    const ContentSettings& p, std::string* l) {
  l->append("<ContentSettings>");
}

void ParamTraits<WebApplicationInfo>::Write(Message* m,
                                            const WebApplicationInfo& p) {
  WriteParam(m, p.title);
  WriteParam(m, p.description);
  WriteParam(m, p.app_url);
  WriteParam(m, p.launch_container);
  WriteParam(m, p.icons.size());
  WriteParam(m, p.permissions.size());

  for (size_t i = 0; i < p.icons.size(); ++i) {
    WriteParam(m, p.icons[i].url);
    WriteParam(m, p.icons[i].width);
    WriteParam(m, p.icons[i].height);
    WriteParam(m, p.icons[i].data);
  }

  for (size_t i = 0; i < p.permissions.size(); ++i)
    WriteParam(m, p.permissions[i]);
}

bool ParamTraits<WebApplicationInfo>::Read(
    const Message* m, void** iter, WebApplicationInfo* r) {
  size_t icon_count = 0;
  size_t permissions_count = 0;

  bool result =
    ReadParam(m, iter, &r->title) &&
    ReadParam(m, iter, &r->description) &&
    ReadParam(m, iter, &r->app_url) &&
    ReadParam(m, iter, &r->launch_container) &&
    ReadParam(m, iter, &icon_count) &&
    ReadParam(m, iter, &permissions_count);
  if (!result)
    return false;

  for (size_t i = 0; i < icon_count; ++i) {
    param_type::IconInfo icon_info;
    result =
        ReadParam(m, iter, &icon_info.url) &&
        ReadParam(m, iter, &icon_info.width) &&
        ReadParam(m, iter, &icon_info.height) &&
        ReadParam(m, iter, &icon_info.data);
    if (!result)
      return false;
    r->icons.push_back(icon_info);
  }

  for (size_t i = 0; i < permissions_count; ++i) {
    std::string permission;
    if (!ReadParam(m, iter, &permission))
      return false;
    r->permissions.push_back(permission);
  }

  return true;
}

void ParamTraits<WebApplicationInfo>::Log(const WebApplicationInfo& p,
                                          std::string* l) {
  l->append("<WebApplicationInfo>");
}

void ParamTraits<ThumbnailScore>::Write(Message* m, const param_type& p) {
  IPC::ParamTraits<double>::Write(m, p.boring_score);
  IPC::ParamTraits<bool>::Write(m, p.good_clipping);
  IPC::ParamTraits<bool>::Write(m, p.at_top);
  IPC::ParamTraits<base::Time>::Write(m, p.time_at_snapshot);
}

bool ParamTraits<ThumbnailScore>::Read(const Message* m, void** iter,
                                       param_type* r) {
  double boring_score;
  bool good_clipping, at_top;
  base::Time time_at_snapshot;
  if (!IPC::ParamTraits<double>::Read(m, iter, &boring_score) ||
      !IPC::ParamTraits<bool>::Read(m, iter, &good_clipping) ||
      !IPC::ParamTraits<bool>::Read(m, iter, &at_top) ||
      !IPC::ParamTraits<base::Time>::Read(m, iter, &time_at_snapshot))
    return false;

  r->boring_score = boring_score;
  r->good_clipping = good_clipping;
  r->at_top = at_top;
  r->time_at_snapshot = time_at_snapshot;
  return true;
}

void ParamTraits<ThumbnailScore>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %d, %d)",
                               p.boring_score, p.good_clipping, p.at_top));
}

void ParamTraits<webkit_glue::PasswordForm>::Write(Message* m,
                                                   const param_type& p) {
  WriteParam(m, p.signon_realm);
  WriteParam(m, p.origin);
  WriteParam(m, p.action);
  WriteParam(m, p.submit_element);
  WriteParam(m, p.username_element);
  WriteParam(m, p.username_value);
  WriteParam(m, p.password_element);
  WriteParam(m, p.password_value);
  WriteParam(m, p.old_password_element);
  WriteParam(m, p.old_password_value);
  WriteParam(m, p.ssl_valid);
  WriteParam(m, p.preferred);
  WriteParam(m, p.blacklisted_by_user);
}

bool ParamTraits<webkit_glue::PasswordForm>::Read(const Message* m, void** iter,
                                                  param_type* p) {
  return
      ReadParam(m, iter, &p->signon_realm) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->submit_element) &&
      ReadParam(m, iter, &p->username_element) &&
      ReadParam(m, iter, &p->username_value) &&
      ReadParam(m, iter, &p->password_element) &&
      ReadParam(m, iter, &p->password_value) &&
      ReadParam(m, iter, &p->old_password_element) &&
      ReadParam(m, iter, &p->old_password_value) &&
      ReadParam(m, iter, &p->ssl_valid) &&
      ReadParam(m, iter, &p->preferred) &&
      ReadParam(m, iter, &p->blacklisted_by_user);
}
void ParamTraits<webkit_glue::PasswordForm>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("<PasswordForm>");
}

void ParamTraits<printing::PageRange>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.from);
  WriteParam(m, p.to);
}

bool ParamTraits<printing::PageRange>::Read(
    const Message* m, void** iter, param_type* p) {
  return ReadParam(m, iter, &p->from) &&
         ReadParam(m, iter, &p->to);
}

void ParamTraits<printing::PageRange>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.to, l);
  l->append(", ");
  LogParam(p.from, l);
  l->append(")");
}

void ParamTraits<printing::NativeMetafile>::Write(
    Message* m, const param_type& p) {
  std::vector<uint8> buffer;
  uint32 size = p.GetDataSize();
  if (size) {
    buffer.resize(size);
    p.GetData(&buffer.front(), size);
  }
  WriteParam(m, buffer);
}

bool ParamTraits<printing::NativeMetafile>::Read(
    const Message* m, void** iter, param_type* p) {
  std::vector<uint8> buffer;
  return ReadParam(m, iter, &buffer) &&
         p->InitFromData(&buffer.front(), static_cast<uint32>(buffer.size()));
}

void ParamTraits<printing::NativeMetafile>::Log(
    const param_type& p, std::string* l) {
  l->append("<printing::NativeMetafile>");
}

void ParamTraits<printing::PrinterCapsAndDefaults>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.printer_capabilities);
  WriteParam(m, p.caps_mime_type);
  WriteParam(m, p.printer_defaults);
  WriteParam(m, p.defaults_mime_type);
}

bool ParamTraits<printing::PrinterCapsAndDefaults>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->printer_capabilities) &&
      ReadParam(m, iter, &p->caps_mime_type) &&
      ReadParam(m, iter, &p->printer_defaults) &&
      ReadParam(m, iter, &p->defaults_mime_type);
}

void ParamTraits<printing::PrinterCapsAndDefaults>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.printer_capabilities, l);
  l->append(",");
  LogParam(p.caps_mime_type, l);
  l->append(",");
  LogParam(p.printer_defaults, l);
  l->append(",");
  LogParam(p.defaults_mime_type, l);
  l->append(")");
}

}  // namespace IPC
