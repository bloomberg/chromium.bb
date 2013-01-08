// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include "content/public/common/content_constants.h"
#include "content/public/common/referrer.h"
#include "net/base/host_port_pair.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

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

namespace IPC {

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  DCHECK(p.possibly_invalid_spec().length() <= content::kMaxURLChars);

  // Beware of print-parse inconsistency which would change an invalid
  // URL into a valid one. Ideally, the message would contain this flag
  // so that the read side could make the check, but performing it here
  // avoids changing the on-the-wire representation of such a fundamental
  // type as GURL. See https://crbug.com/166486 for additional work in
  // this area.
  if (!p.is_valid()) {
    GURL reconstructed_url(p.possibly_invalid_spec());
    if (reconstructed_url.is_valid()) {
      DLOG(WARNING) << "GURL string " << p.possibly_invalid_spec()
                    << " (marked invalid) but parsed as valid.";
      m->WriteString(std::string());
      return;
    }
  }

  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, PickleIterator* iter, GURL* p) {
  std::string s;
  if (!m->ReadString(iter, &s) || s.length() > content::kMaxURLChars) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::string* l) {
  l->append(p.spec());
}

void ParamTraits<net::HostPortPair>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const Message* m,
                                          PickleIterator* iter,
                                          param_type* r) {
  std::string host;
  uint16 port;
  if (!ReadParam(m, iter, &host) || !ReadParam(m, iter, &port))
    return false;

  r->set_host(host);
  r->set_port(port);
  return true;
}

void ParamTraits<net::HostPortPair>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<content::Referrer>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.policy);
}

bool ParamTraits<content::Referrer>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  return ReadParam(m, iter, &r->url) && ReadParam(m, iter, &r->policy);
}

void ParamTraits<content::Referrer>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(",");
  LogParam(p.policy, l);
  l->append(")");
}

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, PickleIterator* iter,
                                   gfx::Point* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.x(), p.y()));
}

void ParamTraits<gfx::PointF>::Write(Message* m, const gfx::PointF& v) {
  ParamTraits<float>::Write(m, v.x());
  ParamTraits<float>::Write(m, v.y());
}

bool ParamTraits<gfx::PointF>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::PointF* r) {
  float x, y;
  if (!ParamTraits<float>::Read(m, iter, &x) ||
      !ParamTraits<float>::Read(m, iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::PointF>::Log(const gfx::PointF& v, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", v.x(), v.y()));
}

void ParamTraits<gfx::Size>::Write(Message* m, const gfx::Size& p) {
  DCHECK_GE(p.width(), 0);
  DCHECK_GE(p.height(), 0);
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Size>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Size* r) {
  int w, h;
  if (!m->ReadInt(iter, &w) || w < 0 ||
      !m->ReadInt(iter, &h) || h < 0)
    return false;
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.width(), p.height()));
}

void ParamTraits<gfx::SizeF>::Write(Message* m, const gfx::SizeF& p) {
  ParamTraits<float>::Write(m, p.width());
  ParamTraits<float>::Write(m, p.height());
}

bool ParamTraits<gfx::SizeF>::Read(const Message* m,
                                   PickleIterator* iter,
                                   gfx::SizeF* p) {
  float w, h;
  if (!ParamTraits<float>::Read(m, iter, &w) ||
      !ParamTraits<float>::Read(m, iter, &h))
    return false;
  p->set_width(w);
  p->set_height(h);
  return true;
}

void ParamTraits<gfx::SizeF>::Log(const gfx::SizeF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", p.width(), p.height()));
}

void ParamTraits<gfx::Vector2d>::Write(Message* m, const gfx::Vector2d& v) {
  m->WriteInt(v.x());
  m->WriteInt(v.y());
}

bool ParamTraits<gfx::Vector2d>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::Vector2d* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Vector2d>::Log(const gfx::Vector2d& v, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", v.x(), v.y()));
}

void ParamTraits<gfx::Vector2dF>::Write(Message* m, const gfx::Vector2dF& v) {
  ParamTraits<float>::Write(m, v.x());
  ParamTraits<float>::Write(m, v.y());
}

bool ParamTraits<gfx::Vector2dF>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::Vector2dF* r) {
  float x, y;
  if (!ParamTraits<float>::Read(m, iter, &x) ||
      !ParamTraits<float>::Read(m, iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Vector2dF>::Log(const gfx::Vector2dF& v, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", v.x(), v.y()));
}

void ParamTraits<gfx::Rect>::Write(Message* m, const gfx::Rect& p) {
  WriteParam(m, p.origin());
  WriteParam(m, p.size());
}

bool ParamTraits<gfx::Rect>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Rect* r) {
  gfx::Point origin;
  gfx::Size size;
  if (!ReadParam(m, iter, &origin) ||
      !ReadParam(m, iter, &size))
    return false;
  r->set_origin(origin);
  r->set_size(size);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d, %d, %d)", p.x(), p.y(),
                               p.width(), p.height()));
}

void ParamTraits<gfx::RectF>::Write(Message* m, const gfx::RectF& p) {
  ParamTraits<float>::Write(m, p.x());
  ParamTraits<float>::Write(m, p.y());
  ParamTraits<float>::Write(m, p.width());
  ParamTraits<float>::Write(m, p.height());
}

bool ParamTraits<gfx::RectF>::Read(const Message* m,
                                   PickleIterator* iter,
                                   gfx::RectF* r) {
  float x, y, w, h;
  if (!ParamTraits<float>::Read(m, iter, &x) ||
      !ParamTraits<float>::Read(m, iter, &y) ||
      !ParamTraits<float>::Read(m, iter, &w) ||
      !ParamTraits<float>::Read(m, iter, &h))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::RectF>::Log(const gfx::RectF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f, %f, %f)", p.x(), p.y(),
                               p.width(), p.height()));
}

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

bool ParamTraits<SkBitmap>::Read(const Message* m,
                                 PickleIterator* iter,
                                 SkBitmap* r) {
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

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::string* l) {
  l->append("<SkBitmap>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC
