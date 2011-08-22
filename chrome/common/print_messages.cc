// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/print_messages.h"

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/size.h"

PrintMsg_Print_Params::PrintMsg_Print_Params()
  : page_size(),
    printable_size(),
    margin_top(0),
    margin_left(0),
    dpi(0),
    min_shrink(0),
    max_shrink(0),
    desired_dpi(0),
    document_cookie(0),
    selection_only(0),
    supports_alpha_blend(0),
    preview_request_id(0),
    is_first_request(0),
    display_header_footer(0),
    date(),
    title(),
    url() {
}

PrintMsg_Print_Params::~PrintMsg_Print_Params() {}

void PrintMsg_Print_Params::Reset() {
  dpi = 0;
  max_shrink = 0;
  min_shrink = 0;
  desired_dpi = 0;
  selection_only = 0;
  document_cookie = 0;
  page_size = gfx::Size();
  page_size = gfx::Size();
  printable_size = gfx::Size();
  margin_left = 0;
  margin_top = 0;
  is_first_request = 0;
  preview_request_id = 0;
  display_header_footer = 0;
  date = string16();
  title = string16();
  url = string16();
}

PrintMsg_PrintPages_Params::PrintMsg_PrintPages_Params()
  : pages() {
}

PrintMsg_PrintPages_Params::~PrintMsg_PrintPages_Params() {}

void PrintMsg_PrintPages_Params::Reset() {
  params.Reset();
  pages = std::vector<int>();
}

