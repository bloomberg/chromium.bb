// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_file_messages.h"

namespace IPC {

void ParamTraits<webkit::ppapi::PepperFilePath>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, static_cast<unsigned>(p.domain()));
  WriteParam(m, p.path());
}

bool ParamTraits<webkit::ppapi::PepperFilePath>::Read(const Message* m,
                                                      PickleIterator* iter,
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
