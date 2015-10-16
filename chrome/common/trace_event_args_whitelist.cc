// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/trace_event_args_whitelist.h"

#include "base/strings/pattern.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace {

const char* const kEventArgsWhitelist[][2] = {
    {"__metadata", "thread_name"},
    {"ipc", "ChannelProxy::Context::OnDispatchMessage"},
    {"toplevel", "*"},
    {NULL, NULL}};

}  // namespace

bool IsTraceEventArgsWhitelisted(const char* category_group_name,
                                 const char* event_name) {
  base::CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    const std::string& category_group_token = category_group_tokens.token();
    for (int i = 0; kEventArgsWhitelist[i][0] != NULL; ++i) {
      DCHECK(kEventArgsWhitelist[i][1]);

      if (base::MatchPattern(category_group_token.c_str(),
                             kEventArgsWhitelist[i][0]) &&
          base::MatchPattern(event_name, kEventArgsWhitelist[i][1])) {
        return true;
      }
    }
  }

  return false;
}
