// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains ParamTraits templates to support serialization of WebKit
// data types over IPC.
//
// NOTE: IT IS IMPORTANT THAT ONLY POD (plain old data) TYPES ARE SERIALIZED.
//
// There are several reasons for this restrictions:
//
//   o  We don't want inclusion of this file to imply linking to WebKit code.
//
//   o  Many WebKit structures are not thread-safe.  WebString, for example,
//      contains a reference counted buffer, which does not use thread-safe
//      reference counting.  If we allowed serializing WebString, then we may
//      run the risk of introducing subtle thread-safety bugs if people passed a
//      WebString across threads via PostTask(NewRunnableMethod(...)).
//
//   o  The WebKit API has redundant types for strings, and we should avoid
//      using those beyond code that interfaces with the WebKit API.

#ifndef CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_
#define CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_
#pragma once

#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"

namespace WebKit {
struct WebCompositionUnderline;
struct WebFindOptions;
struct WebMediaPlayerAction;
struct WebRect;
struct WebScreenInfo;
}

namespace IPC {

template <>
struct ParamTraits<WebKit::WebRect> {
  typedef WebKit::WebRect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebScreenInfo> {
  typedef WebKit::WebScreenInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<WebKit::WebConsoleMessage::Level> {
  typedef int Type;
};

template <>
struct SimilarTypeTraits<WebKit::WebPopupType> {
  typedef int Type;
};

template <>
struct ParamTraits<WebKit::WebFindOptions> {
  typedef WebKit::WebFindOptions param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebCache::UsageStats> {
  typedef WebKit::WebCache::UsageStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.minDeadCapacity);
    WriteParam(m, p.maxDeadCapacity);
    WriteParam(m, p.capacity);
    WriteParam(m, p.liveSize);
    WriteParam(m, p.deadSize);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->minDeadCapacity) &&
      ReadParam(m, iter, &r->maxDeadCapacity) &&
      ReadParam(m, iter, &r->capacity) &&
      ReadParam(m, iter, &r->liveSize) &&
      ReadParam(m, iter, &r->deadSize);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<WebCache::UsageStats>");
  }
};

template <>
struct ParamTraits<WebKit::WebCache::ResourceTypeStat> {
  typedef WebKit::WebCache::ResourceTypeStat param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.count);
    WriteParam(m, p.size);
    WriteParam(m, p.liveSize);
    WriteParam(m, p.decodedSize);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
        ReadParam(m, iter, &r->count) &&
        ReadParam(m, iter, &r->size) &&
        ReadParam(m, iter, &r->liveSize) &&
        ReadParam(m, iter, &r->decodedSize);
    return result;
  }
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebCache::ResourceTypeStats> {
  typedef WebKit::WebCache::ResourceTypeStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.images);
    WriteParam(m, p.cssStyleSheets);
    WriteParam(m, p.scripts);
    WriteParam(m, p.xslStyleSheets);
    WriteParam(m, p.fonts);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
      ReadParam(m, iter, &r->images) &&
      ReadParam(m, iter, &r->cssStyleSheets) &&
      ReadParam(m, iter, &r->scripts) &&
      ReadParam(m, iter, &r->xslStyleSheets) &&
      ReadParam(m, iter, &r->fonts);
    return result;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<WebCoreStats>");
    LogParam(p.images, l);
    LogParam(p.cssStyleSheets, l);
    LogParam(p.scripts, l);
    LogParam(p.xslStyleSheets, l);
    LogParam(p.fonts, l);
    l->append("</WebCoreStats>");
  }
};

template <>
struct SimilarTypeTraits<WebKit::WebTextDirection> {
  typedef int Type;
};

template <>
struct SimilarTypeTraits<WebKit::WebDragOperation> {
  typedef int Type;
};

template <>
struct ParamTraits<WebKit::WebMediaPlayerAction> {
  typedef WebKit::WebMediaPlayerAction param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<WebKit::WebContextMenuData::MediaType> {
  typedef int Type;
};

template <>
struct ParamTraits<WebKit::WebCompositionUnderline> {
  typedef WebKit::WebCompositionUnderline param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebTextInputType> {
  typedef WebKit::WebTextInputType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
      case WebKit::WebTextInputTypeNone:
        control = "WebKit::WebTextInputTypeNone";
        break;
      case WebKit::WebTextInputTypeText:
        control = "WebKit::WebTextInputTypeText";
        break;
      case WebKit::WebTextInputTypePassword:
        control = "WebKit::WebTextInputTypePassword";
        break;
      default:
        NOTIMPLEMENTED();
        control = "UNKNOWN";
        break;
    }
    LogParam(control, l);
  }
};

template <>
struct SimilarTypeTraits<WebKit::WebFileError> {
  typedef int Type;
};

template <>
struct ParamTraits<WebKit::WebTextCheckingResult> {
  typedef WebKit::WebTextCheckingResult param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_
