// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/pepper_file_messages.h"

namespace IPC {

void ParamTraits<webkit::ppapi::DirEntry>::Write(Message* m,
                                                 const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.is_dir);
}

bool ParamTraits<webkit::ppapi::DirEntry>::Read(const Message* m,
                                                void** iter,
                                                param_type* p) {
  return ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->is_dir);
}

void ParamTraits<webkit::ppapi::DirEntry>::Log(const param_type& p,
                                               std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.is_dir, l);
  l->append(")");
}

void ParamTraits<webkit::ppapi::PepperFilePath>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, static_cast<unsigned>(p.domain()));
  WriteParam(m, p.path());
}

bool ParamTraits<webkit::ppapi::PepperFilePath>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  unsigned domain;
  FilePath path;
  if (!ReadParam(m, iter, &domain) || !ReadParam(m, iter, &path))
    return false;
  if (domain > webkit::ppapi::PepperFilePath::DOMAIN_MAX_VALID)
    return false;

  *p = webkit::ppapi::PepperFilePath(
      static_cast<webkit::ppapi::PepperFilePath::Domain>(domain), path);
  return true;
}

void ParamTraits<webkit::ppapi::PepperFilePath>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.domain()), l);
  l->append(", ");
  LogParam(p.path(), l);
  l->append(")");
}

}  // namespace IPC
