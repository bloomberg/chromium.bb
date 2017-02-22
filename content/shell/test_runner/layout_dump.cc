// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/layout_dump.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace test_runner {

using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebLocalFrame;
using blink::WebSize;

namespace {

std::string DumpFrameHeaderIfNeeded(WebFrame* frame) {
  std::string result;

  // Add header for all but the main frame. Skip empty frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8());
    result.append("'\n--------\n");
  }

  return result;
}

std::string DumpFrameScrollPosition(WebFrame* frame) {
  std::string result;
  WebSize offset = frame->getScrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->parent()) {
      result =
          std::string("frame '") + frame->uniqueName().utf8().data() + "' ";
    }
    base::StringAppendF(&result, "scrolled to %d,%d\n", offset.width,
                        offset.height);
  }

  return result;
}

}  // namespace

std::string DumpLayout(WebLocalFrame* frame,
                       const LayoutTestRuntimeFlags& flags) {
  DCHECK(frame);
  std::string result;

  if (flags.dump_as_text()) {
    result = DumpFrameHeaderIfNeeded(frame);
    if (flags.is_printing() && frame->document().isHTMLDocument()) {
      result += WebFrameContentDumper::dumpLayoutTreeAsText(
                    frame, WebFrameContentDumper::LayoutAsTextPrinting)
                    .utf8();
    } else {
      result += frame->document().contentAsTextForTesting().utf8();
    }
    result += "\n";
  } else if (flags.dump_as_markup()) {
    DCHECK(!flags.is_printing());
    result = DumpFrameHeaderIfNeeded(frame);
    result += WebFrameContentDumper::dumpAsMarkup(frame).utf8();
    result += "\n";
  } else {
    if (frame->parent() == nullptr) {
      WebFrameContentDumper::LayoutAsTextControls layout_text_behavior =
          WebFrameContentDumper::LayoutAsTextNormal;
      if (flags.is_printing())
        layout_text_behavior |= WebFrameContentDumper::LayoutAsTextPrinting;
      result = WebFrameContentDumper::dumpLayoutTreeAsText(frame,
                                                           layout_text_behavior)
                   .utf8();
    }
    result += DumpFrameScrollPosition(frame);
  }

  return result;
}

}  // namespace test_runner
