// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages_params.h"

#include "chrome/common/render_messages.h"

namespace IPC {

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Write(Message* m,
                                                      const param_type& p) {
  m->WriteInt(p.type);
}

bool ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->type = static_cast<param_type::Type>(type);
  return true;
}

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Log(const param_type& p,
                                                    std::string* l) {
  std::string type;
  switch (p.type) {
    case ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER";
      break;
    default:
      type = "UNKNOWN";
      break;
  }
  LogParam(type, l);
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Write(
    Message* m, const param_type& p) {
  m->WriteInt(p.state);
}

bool ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->state = static_cast<param_type::State>(type);
  return true;
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Log(
    const param_type& p, std::string* l) {
  std::string state;
  switch (p.state) {
    case ViewHostMsg_GetSearchProviderInstallState_Params::DENIED:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::DENIED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED:
      state =
          "ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_BUT_NOT_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_BUT_NOT_DEFAULT";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_AS_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_AS_DEFAULT";
      break;
    default:
      state = "UNKNOWN";
      break;
  }
  LogParam(state, l);
}

}  // namespace IPC
