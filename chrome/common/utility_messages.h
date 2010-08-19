// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_UTILITY_MESSAGES_H_
#define CHROME_COMMON_UTILITY_MESSAGES_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

// Traits for UpdateManifest::Result.
template <>
struct ParamTraits<UpdateManifest::Result> {
  typedef UpdateManifest::Result param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.extension_id);
    WriteParam(m, p.version);
    WriteParam(m, p.browser_min_version);
    WriteParam(m, p.package_hash);
    WriteParam(m, p.crx_url);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->extension_id) &&
           ReadParam(m, iter, &p->version) &&
           ReadParam(m, iter, &p->browser_min_version) &&
           ReadParam(m, iter, &p->package_hash) &&
           ReadParam(m, iter, &p->crx_url);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.extension_id, l);
    l->append(", ");
    LogParam(p.version, l);
    l->append(", ");
    LogParam(p.browser_min_version, l);
    l->append(", ");
    LogParam(p.package_hash, l);
    l->append(", ");
    LogParam(p.crx_url, l);
    l->append(")");
  }
};

template<>
struct ParamTraits<UpdateManifest::Results> {
  typedef UpdateManifest::Results param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.list);
    WriteParam(m, p.daystart_elapsed_seconds);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->list) &&
           ReadParam(m, iter, &p->daystart_elapsed_seconds);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.list, l);
    l->append(", ");
    LogParam(p.daystart_elapsed_seconds, l);
    l->append(")");
  }
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/utility_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_UTILITY_MESSAGES_H_
