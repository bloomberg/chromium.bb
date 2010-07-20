// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#include "chrome/common/chrome_constants.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/thumbnail_score.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "printing/native_metafile.h"
#include "printing/page_range.h"

#ifndef EXCLUDE_SKIA_DEPENDENCIES
#include "third_party/skia/include/core/SkBitmap.h"
#endif
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/password_form.h"

namespace IPC {

#ifndef EXCLUDE_SKIA_DEPENDENCIES

namespace {

struct SkBitmap_Data {
  // The configuration for the bitmap (bits per pixel, etc).
  SkBitmap::Config fConfig;

  // The width of the bitmap in pixels.
  uint32 fWidth;

  // The height of the bitmap in pixels.
  uint32 fHeight;

  void InitSkBitmapDataForTransfer(const SkBitmap& bitmap) {
    fConfig = bitmap.config();
    fWidth = bitmap.width();
    fHeight = bitmap.height();
  }

  // Returns whether |bitmap| successfully initialized.
  bool InitSkBitmapFromData(SkBitmap* bitmap, const char* pixels,
                            size_t total_pixels) const {
    if (total_pixels) {
      bitmap->setConfig(fConfig, fWidth, fHeight, 0);
      if (!bitmap->allocPixels())
        return false;
      if (total_pixels != bitmap->getSize())
        return false;
      memcpy(bitmap->getPixels(), pixels, total_pixels);
    }
    return true;
  }
};

}  // namespace


void ParamTraits<SkBitmap>::Write(Message* m, const SkBitmap& p) {
  size_t fixed_size = sizeof(SkBitmap_Data);
  SkBitmap_Data bmp_data;
  bmp_data.InitSkBitmapDataForTransfer(p);
  m->WriteData(reinterpret_cast<const char*>(&bmp_data),
               static_cast<int>(fixed_size));
  size_t pixel_size = p.getSize();
  SkAutoLockPixels p_lock(p);
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()),
               static_cast<int>(pixel_size));
}

bool ParamTraits<SkBitmap>::Read(const Message* m, void** iter, SkBitmap* r) {
  const char* fixed_data;
  int fixed_data_size = 0;
  if (!m->ReadData(iter, &fixed_data, &fixed_data_size) ||
     (fixed_data_size <= 0)) {
    NOTREACHED();
    return false;
  }
  if (fixed_data_size != sizeof(SkBitmap_Data))
    return false;  // Message is malformed.

  const char* variable_data;
  int variable_data_size = 0;
  if (!m->ReadData(iter, &variable_data, &variable_data_size) ||
     (variable_data_size < 0)) {
    NOTREACHED();
    return false;
  }
  const SkBitmap_Data* bmp_data =
      reinterpret_cast<const SkBitmap_Data*>(fixed_data);
  return bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::wstring* l) {
  l->append(StringPrintf(L"<SkBitmap>"));
}

#endif  // EXCLUDE_SKIA_DEPENDENCIES

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, void** iter, GURL* p) {
  std::string s;
  if (!m->ReadString(iter, &s) || s.length() > chrome::kMaxURLChars) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::wstring* l) {
  l->append(UTF8ToWide(p.spec()));
}

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, void** iter,
                                   gfx::Point* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d)", p.x(), p.y()));
}


void ParamTraits<gfx::Rect>::Write(Message* m, const gfx::Rect& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Rect>::Read(const Message* m, void** iter, gfx::Rect* r) {
  int x, y, w, h;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y) ||
      !m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d, %d, %d)", p.x(), p.y(),
                         p.width(), p.height()));
}


void ParamTraits<gfx::Size>::Write(Message* m, const gfx::Size& p) {
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Size>::Read(const Message* m, void** iter, gfx::Size* r) {
  int w, h;
  if (!m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d)", p.width(), p.height()));
}

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

void ParamTraits<ContentSetting>::Log(const param_type& p, std::wstring* l) {
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
    const ContentSettings& p, std::wstring* l) {
  l->append(StringPrintf(L"<ContentSettings>"));
}

void ParamTraits<webkit_glue::WebApplicationInfo>::Write(
    Message* m, const webkit_glue::WebApplicationInfo& p) {
  WriteParam(m, p.title);
  WriteParam(m, p.description);
  WriteParam(m, p.app_url);
  WriteParam(m, p.icons.size());
  for (size_t i = 0; i < p.icons.size(); ++i) {
    WriteParam(m, p.icons[i].url);
    WriteParam(m, p.icons[i].width);
    WriteParam(m, p.icons[i].height);
  }
}

bool ParamTraits<webkit_glue::WebApplicationInfo>::Read(
    const Message* m, void** iter, webkit_glue::WebApplicationInfo* r) {
  size_t icon_count;
  bool result =
    ReadParam(m, iter, &r->title) &&
    ReadParam(m, iter, &r->description) &&
    ReadParam(m, iter, &r->app_url) &&
    ReadParam(m, iter, &icon_count);
  if (!result)
    return false;
  for (size_t i = 0; i < icon_count; ++i) {
    param_type::IconInfo icon_info;
    result =
        ReadParam(m, iter, &icon_info.url) &&
        ReadParam(m, iter, &icon_info.width) &&
        ReadParam(m, iter, &icon_info.height);
    if (!result)
      return false;
    r->icons.push_back(icon_info);
  }
  return true;
}

void ParamTraits<webkit_glue::WebApplicationInfo>::Log(
    const webkit_glue::WebApplicationInfo& p, std::wstring* l) {
  l->append(L"<WebApplicationInfo>");
}

void ParamTraits<URLRequestStatus>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.status()));
  WriteParam(m, p.os_error());
}

bool ParamTraits<URLRequestStatus>::Read(const Message* m, void** iter,
                                         param_type* r) {
  int status, os_error;
  if (!ReadParam(m, iter, &status) ||
      !ReadParam(m, iter, &os_error))
    return false;
  r->set_status(static_cast<URLRequestStatus::Status>(status));
  r->set_os_error(os_error);
  return true;
}

void ParamTraits<URLRequestStatus>::Log(const param_type& p, std::wstring* l) {
  std::wstring status;
  switch (p.status()) {
    case URLRequestStatus::SUCCESS:
      status = L"SUCCESS";
      break;
    case URLRequestStatus::IO_PENDING:
      status = L"IO_PENDING ";
      break;
    case URLRequestStatus::HANDLED_EXTERNALLY:
      status = L"HANDLED_EXTERNALLY";
      break;
    case URLRequestStatus::CANCELED:
      status = L"CANCELED";
      break;
    case URLRequestStatus::FAILED:
      status = L"FAILED";
      break;
    default:
      status = L"UNKNOWN";
      break;
  }
  if (p.status() == URLRequestStatus::FAILED)
    l->append(L"(");

  LogParam(status, l);

  if (p.status() == URLRequestStatus::FAILED) {
    l->append(L", ");
    LogParam(p.os_error(), l);
    l->append(L")");
  }
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

void ParamTraits<ThumbnailScore>::Log(const param_type& p, std::wstring* l) {
  l->append(StringPrintf(L"(%f, %d, %d)",
                         p.boring_score, p.good_clipping, p.at_top));
}

void ParamTraits<Geoposition::ErrorCode>::Write(
    Message* m, const Geoposition::ErrorCode& p) {
  int error_code = p;
  WriteParam(m, error_code);
}

bool ParamTraits<Geoposition::ErrorCode>::Read(
      const Message* m, void** iter, Geoposition::ErrorCode* p) {
  int error_code_param = 0;
  bool ret = ReadParam(m, iter, &error_code_param);
  *p = static_cast<Geoposition::ErrorCode>(error_code_param);
  return ret;
}

void ParamTraits<Geoposition::ErrorCode>::Log(
    const Geoposition::ErrorCode& p, std::wstring* l) {
  int error_code = p;
  l->append(StringPrintf(L"<Geoposition::ErrorCode>%d", error_code));
}

void ParamTraits<Geoposition>::Write(Message* m, const Geoposition& p) {
  WriteParam(m, p.latitude);
  WriteParam(m, p.longitude);
  WriteParam(m, p.accuracy);
  WriteParam(m, p.altitude);
  WriteParam(m, p.altitude_accuracy);
  WriteParam(m, p.speed);
  WriteParam(m, p.heading);
  WriteParam(m, p.timestamp);
  WriteParam(m, p.error_code);
  WriteParam(m, p.error_message);
}

bool ParamTraits<Geoposition>::Read(
      const Message* m, void** iter, Geoposition* p) {
  bool ret = ReadParam(m, iter, &p->latitude);
  ret = ret && ReadParam(m, iter, &p->longitude);
  ret = ret && ReadParam(m, iter, &p->accuracy);
  ret = ret && ReadParam(m, iter, &p->altitude);
  ret = ret && ReadParam(m, iter, &p->altitude_accuracy);
  ret = ret && ReadParam(m, iter, &p->speed);
  ret = ret && ReadParam(m, iter, &p->heading);
  ret = ret && ReadParam(m, iter, &p->timestamp);
  ret = ret && ReadParam(m, iter, &p->error_code);
  ret = ret && ReadParam(m, iter, &p->error_message);
  return ret;
}

void ParamTraits<Geoposition>::Log(const Geoposition& p, std::wstring* l) {
  l->append(
      StringPrintf(
          L"<Geoposition>"
          L"%.6f %.6f %.6f %.6f "
          L"%.6f %.6f %.6f ",
          p.latitude, p.longitude, p.accuracy, p.altitude,
          p.altitude_accuracy, p.speed, p.heading));
  LogParam(p.timestamp, l);
  l->append(L" ");
  l->append(UTF8ToWide(p.error_message));
  LogParam(p.error_code, l);
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
    const param_type& p, std::wstring* l) {
  l->append(L"(");
  LogParam(p.to, l);
  l->append(L", ");
  LogParam(p.from, l);
  l->append(L")");
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
#if defined(OS_WIN)
  return ReadParam(m, iter, &buffer) &&
         p->CreateFromData(&buffer.front(), static_cast<uint32>(buffer.size()));
#else  // defined(OS_WIN)
  return ReadParam(m, iter, &buffer) &&
         p->Init(&buffer.front(), static_cast<uint32>(buffer.size()));
#endif  // defined(OS_WIN)
}

void ParamTraits<printing::NativeMetafile>::Log(
    const param_type& p, std::wstring* l) {
  l->append(StringPrintf(L"<printing::NativeMetafile>"));
}

}  // namespace IPC
