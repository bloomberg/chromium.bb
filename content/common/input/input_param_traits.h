// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines IPC::ParamTraits<> specializations for several
// input-related types that require manual serialiation code.

#ifndef CONTENT_COMMON_INPUT_INPUT_PARAM_TRAITS_H_
#define CONTENT_COMMON_INPUT_INPUT_PARAM_TRAITS_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_param_traits_macros.h"
#include "content/common/input/event_packet.h"
#include "content/common/input/input_event.h"
#include "content/common/input/web_input_event_payload.h"

namespace IPC {

// TODO(jdduke): There should be a common copy of this utility somewhere...
// Move or remove appropriately.
template <class P>
struct ParamTraits<scoped_ptr<P> > {
  typedef scoped_ptr<P> param_type;
  static void Write(Message* m, const param_type& p) {
    bool valid = !!p;
    WriteParam(m, valid);
    if (valid)
      WriteParam(m, *p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    bool valid = false;
    if (!ReadParam(m, iter, &valid))
      return false;

    if (!valid) {
      r->reset();
      return true;
    }

    param_type temp(new P());
    if (!ReadParam(m, iter, temp.get()))
      return false;

    r->swap(temp);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    if (p)
      LogParam(*p, l);
    else
      l->append("NULL");
  }
};

template <>
struct CONTENT_EXPORT ParamTraits<content::EventPacket> {
  typedef content::EventPacket param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<content::InputEvent> {
  typedef content::InputEvent param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<content::WebInputEventPayload> {
  typedef content::WebInputEventPayload param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_INPUT_INPUT_PARAM_TRAITS_H_
