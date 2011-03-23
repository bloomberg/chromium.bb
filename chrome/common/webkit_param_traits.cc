// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webkit_param_traits.h"

#include "base/format_macros.h"

namespace IPC {

void ParamTraits<WebKit::WebCache::ResourceTypeStat>::Log(
    const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%" PRIuS " %" PRIuS " %" PRIuS " %" PRIuS,
                               p.count, p.size, p.liveSize, p.decodedSize));
}

void ParamTraits<WebKit::WebTextCheckingResult>::Write(Message* m,
                                                       const param_type& p) {
#if defined(WEB_TEXT_CHECKING_RESULT_IS_A_STRUCT)
  WriteParam(m, static_cast<int>(p.error));
  WriteParam(m, p.position);
  WriteParam(m, p.length);
#else
  WriteParam(m, static_cast<int>(p.error()));
  WriteParam(m, p.position());
  WriteParam(m, p.length());
#endif
}

bool ParamTraits<WebKit::WebTextCheckingResult>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  int error = 0;
  if (!ReadParam(m, iter, &error))
    return false;
  if (error != WebKit::WebTextCheckingResult::ErrorSpelling &&
      error != WebKit::WebTextCheckingResult::ErrorGrammar)
    return false;
  int position = 0;
  if (!ReadParam(m, iter, &position))
    return false;
  int length = 0;
  if (!ReadParam(m, iter, &length))
    return false;

  *p = WebKit::WebTextCheckingResult(
      static_cast<WebKit::WebTextCheckingResult::Error>(error),
      position,
      length);
  return true;
}

void ParamTraits<WebKit::WebTextCheckingResult>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
#if defined(WEB_TEXT_CHECKING_RESULT_IS_A_STRUCT)
  LogParam(static_cast<int>(p.error), l);
  l->append(", ");
  LogParam(p.position, l);
  l->append(", ");
  LogParam(p.length, l);
#else
  LogParam(static_cast<int>(p.error()), l);
  l->append(", ");
  LogParam(p.position(), l);
  l->append(", ");
  LogParam(p.length(), l);
#endif
  l->append(")");
}

}  // namespace IPC
