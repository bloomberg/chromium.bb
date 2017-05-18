// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/page_load_metrics/page_load_metrics_param_traits.h"

#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

void ParamTraits<page_load_metrics::mojom::PageLoadTiming>::GetSize(
    base::PickleSizer* s,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  GetParamSize(s, timing.navigation_start);
  GetParamSize(s, timing.response_start);
  GetParamSize(s, *timing.document_timing);
  GetParamSize(s, *timing.paint_timing);
  GetParamSize(s, *timing.parse_timing);
  GetParamSize(s, *timing.style_sheet_timing);
}

void ParamTraits<page_load_metrics::mojom::PageLoadTiming>::Write(
    base::Pickle* m,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.document_timing || !timing.paint_timing || !timing.parse_timing ||
      !timing.style_sheet_timing) {
    // PageLoadTimings must have instances for all child struct members.
    LOG(DFATAL) << "Received invalid PageLoadTiming.";
    return;
  }
  WriteParam(m, timing.navigation_start);
  WriteParam(m, timing.response_start);
  WriteParam(m, *timing.document_timing);
  WriteParam(m, *timing.paint_timing);
  WriteParam(m, *timing.parse_timing);
  WriteParam(m, *timing.style_sheet_timing);
}

bool ParamTraits<page_load_metrics::mojom::PageLoadTiming>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    page_load_metrics::mojom::PageLoadTiming* timing) {
  // Provide default instances of all child struct members, which we'll read
  // into.
  timing->document_timing = page_load_metrics::mojom::DocumentTiming::New();
  timing->paint_timing = page_load_metrics::mojom::PaintTiming::New();
  timing->parse_timing = page_load_metrics::mojom::ParseTiming::New();
  timing->style_sheet_timing =
      page_load_metrics::mojom::StyleSheetTiming::New();
  return ReadParam(m, iter, &timing->navigation_start) &&
         ReadParam(m, iter, &timing->response_start) &&
         ReadParam(m, iter, timing->document_timing.get()) &&
         ReadParam(m, iter, timing->paint_timing.get()) &&
         ReadParam(m, iter, timing->parse_timing.get()) &&
         ReadParam(m, iter, timing->style_sheet_timing.get());
}

void ParamTraits<page_load_metrics::mojom::PageLoadTiming>::Log(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    std::string* l) {
  l->append("PageLoadTiming(");
  LogParam(timing.navigation_start, l);
  l->append(",");
  LogParam(timing.response_start, l);
  l->append(",");

  l->append("DocumentTiming(");
  LogParam(timing.document_timing->dom_content_loaded_event_start, l);
  l->append(",");
  LogParam(timing.document_timing->load_event_start, l);
  l->append(",");
  LogParam(timing.document_timing->first_layout, l);
  l->append(")");

  l->append(",");

  l->append("PaintTiming(");
  LogParam(timing.paint_timing->first_paint, l);
  l->append(",");
  LogParam(timing.paint_timing->first_text_paint, l);
  l->append(",");
  LogParam(timing.paint_timing->first_image_paint, l);
  l->append(",");
  LogParam(timing.paint_timing->first_contentful_paint, l);
  l->append(",");
  LogParam(timing.paint_timing->first_meaningful_paint, l);
  l->append(")");

  l->append(",");

  l->append("ParseTiming(");
  LogParam(timing.parse_timing->parse_start, l);
  l->append(",");
  LogParam(timing.parse_timing->parse_stop, l);
  l->append(",");
  LogParam(timing.parse_timing->parse_blocked_on_script_load_duration, l);
  l->append(",");
  LogParam(timing.parse_timing
               ->parse_blocked_on_script_load_from_document_write_duration,
           l);
  l->append(",");
  LogParam(timing.parse_timing->parse_blocked_on_script_execution_duration, l);
  l->append(",");
  LogParam(timing.parse_timing
               ->parse_blocked_on_script_execution_from_document_write_duration,
           l);
  l->append(")");

  l->append(",");

  l->append("StyleSheetTiming(");
  LogParam(
      timing.style_sheet_timing->author_style_sheet_parse_duration_before_fcp,
      l);
  l->append(",");
  LogParam(timing.style_sheet_timing->update_style_duration_before_fcp, l);
  l->append(")");

  l->append(")");
}

}  // namespace IPC
