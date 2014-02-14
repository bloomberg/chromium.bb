// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/leak_detector.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"

using blink::WebLeakDetector;

namespace content {

LeakDetector::LeakDetector()
    : previous_number_of_live_documents_(0),
      previous_number_of_live_nodes_(0) {
}

LeakDetectionResult LeakDetector::TryLeakDetection(blink::WebFrame* frame) {
  LeakDetectionResult result;
  unsigned number_of_live_documents = 0;
  unsigned number_of_live_nodes = 0;

  WebLeakDetector::collectGarbargeAndGetDOMCounts(
      frame, &number_of_live_documents, &number_of_live_nodes);

  result.leaked =
      previous_number_of_live_documents_ > 0 &&
      previous_number_of_live_nodes_ > 0 &&
      (previous_number_of_live_documents_ < number_of_live_documents ||
       previous_number_of_live_nodes_ < number_of_live_nodes);

  if (result.leaked) {
    base::DictionaryValue detail;
    base::ListValue* list = new base::ListValue();
    list->AppendInteger(previous_number_of_live_documents_);
    list->AppendInteger(number_of_live_documents);
    detail.Set("numberOfLiveDocuments", list);

    list = new base::ListValue();
    list->AppendInteger(previous_number_of_live_nodes_);
    list->AppendInteger(number_of_live_nodes);
    detail.Set("numberOfLiveNodes", list);

    std::string detail_str;
    base::JSONWriter::Write(&detail, &detail_str);
    result.detail = detail_str;
  }

  previous_number_of_live_documents_ = number_of_live_documents;
  previous_number_of_live_nodes_ = number_of_live_nodes;

  return result;
}

}  // namespace content
