// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/layout_dump.h"

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
  WebSize offset = frame->scrollOffset();
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

std::string DumpLayout(WebLocalFrame* frame, const LayoutDumpFlags& flags) {
  DCHECK(frame);
  std::string result;

  switch (flags.main_dump_mode) {
    case LayoutDumpMode::DUMP_AS_TEXT:
      result = DumpFrameHeaderIfNeeded(frame);
      if (flags.dump_as_printed && frame->document().isHTMLDocument()) {
        result += WebFrameContentDumper::dumpLayoutTreeAsText(
                      frame, WebFrameContentDumper::LayoutAsTextPrinting)
                      .utf8();
      } else {
        result += frame->document().contentAsTextForTesting().utf8();
      }
      result += "\n";
      break;
    case LayoutDumpMode::DUMP_AS_MARKUP:
      DCHECK(!flags.dump_as_printed);
      result = DumpFrameHeaderIfNeeded(frame);
      result += WebFrameContentDumper::dumpAsMarkup(frame).utf8();
      result += "\n";
      break;
    case LayoutDumpMode::DUMP_SCROLL_POSITIONS:
      if (frame->parent() == nullptr) {
        WebFrameContentDumper::LayoutAsTextControls layout_text_behavior =
            WebFrameContentDumper::LayoutAsTextNormal;
        if (flags.dump_as_printed)
          layout_text_behavior |= WebFrameContentDumper::LayoutAsTextPrinting;
        if (flags.debug_render_tree)
          layout_text_behavior |= WebFrameContentDumper::LayoutAsTextDebug;
        if (flags.dump_line_box_trees)
          layout_text_behavior |=
              WebFrameContentDumper::LayoutAsTextWithLineTrees;
        result = WebFrameContentDumper::dumpLayoutTreeAsText(
                     frame, layout_text_behavior)
                     .utf8();
      }
      result += DumpFrameScrollPosition(frame);
      break;
    default:
      DCHECK(false) << static_cast<int>(flags.main_dump_mode);
      result = "";
      break;
  }

  return result;
}

}  // namespace test_runner
