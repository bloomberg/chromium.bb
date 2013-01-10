// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/automation/automation_renderer_helper.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/automation_events.h"
#include "chrome/common/automation_messages.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

using WebKit::WebFrame;
using WebKit::WebSize;
using WebKit::WebURL;
using WebKit::WebView;

AutomationRendererHelper::AutomationRendererHelper(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

AutomationRendererHelper::~AutomationRendererHelper() { }

bool AutomationRendererHelper::SnapshotEntirePage(
    WebView* view,
    std::vector<unsigned char>* png_data,
    std::string* error_msg) {
  WebFrame* frame = view->mainFrame();
  WebSize old_size = view->size();
  WebSize new_size = frame->contentsSize();
  // For RTL, the minimum scroll offset may be negative.
  WebSize min_scroll = frame->minimumScrollOffset();
  WebSize old_scroll = frame->scrollOffset();
  bool fixed_layout_enabled = view->isFixedLayoutModeEnabled();
  WebSize fixed_size = view->fixedLayoutSize();

  frame->setCanHaveScrollbars(false);
  view->setFixedLayoutSize(old_size);
  view->enableFixedLayoutMode(true);
  view->resize(new_size);
  view->layout();
  frame->setScrollOffset(WebSize(0, 0));

  skia::ScopedPlatformCanvas canvas(new_size.width, new_size.height, true);

  view->paint(webkit_glue::ToWebCanvas(canvas.get()),
              gfx::Rect(0, 0, new_size.width, new_size.height));

  frame->setCanHaveScrollbars(true);
  view->setFixedLayoutSize(fixed_size);
  view->enableFixedLayoutMode(fixed_layout_enabled);
  view->resize(old_size);
  view->layout();
  frame->setScrollOffset(WebSize(old_scroll.width - min_scroll.width,
                                 old_scroll.height - min_scroll.height));

  const SkBitmap& bmp = skia::GetTopDevice(*canvas)->accessBitmap(false);
  SkAutoLockPixels lock_pixels(bmp);
  // EncodeBGRA uses FORMAT_SkBitmap, which doesn't work on windows for some
  // cases dealing with transparency. See crbug.com/96317. Use FORMAT_BGRA.
  bool encode_success = gfx::PNGCodec::Encode(
      reinterpret_cast<unsigned char*>(bmp.getPixels()),
      gfx::PNGCodec::FORMAT_BGRA,
      gfx::Size(bmp.width(), bmp.height()),
      bmp.rowBytes(),
      true,  // discard_transparency
      std::vector<gfx::PNGCodec::Comment>(),
      png_data);
  if (!encode_success)
    *error_msg = "failed to encode image as png";
  return encode_success;
}

void AutomationRendererHelper::OnSnapshotEntirePage() {
  std::vector<unsigned char> png_data;
  std::string error_msg;
  bool success = false;
  if (render_view()->GetWebView()) {
    success = SnapshotEntirePage(
        render_view()->GetWebView(), &png_data, &error_msg);
  } else {
    error_msg = "cannot snapshot page because webview is null";
  }

  // Check that the image is not too large, allowing a 1kb buffer for other
  // message data.
  if (success && png_data.size() > IPC::Channel::kMaximumMessageSize - 1024) {
    png_data.clear();
    success = false;
    error_msg = "image is too large to be transferred over ipc";
  }
  Send(new AutomationMsg_SnapshotEntirePageACK(
      routing_id(), success, png_data, error_msg));
}

namespace {

scoped_ptr<base::Value> EvaluateScriptInFrame(WebFrame* web_frame,
                                              const std::string& script) {
  v8::HandleScope handle_scope;
  v8::Local<v8::Value> result = v8::Local<v8::Value>::New(
      web_frame->executeScriptAndReturnValue(
          WebKit::WebScriptSource(UTF8ToUTF16(script))));
  if (!result.IsEmpty()) {
    v8::Local<v8::Context> context = web_frame->mainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    return scoped_ptr<base::Value>(converter->FromV8Value(result, context));
  } else {
    return scoped_ptr<base::Value>(base::Value::CreateNullValue());
  }
}

WebFrame* FrameFromXPath(WebView* view, const string16& frame_xpath) {
  WebFrame* frame = view->mainFrame();
  if (frame_xpath.empty())
    return frame;

  std::vector<string16> xpaths;
  base::SplitString(frame_xpath, '\n', &xpaths);
  for (std::vector<string16>::const_iterator i = xpaths.begin();
       frame && i != xpaths.end(); ++i) {
    frame = frame->findChildByExpression(*i);
  }
  return frame;
}

bool EvaluateScriptChainHelper(
    WebView* web_view,
    const std::string& script,
    const std::string& frame_xpath,
    scoped_ptr<base::DictionaryValue>* result,
    std::string* error_msg) {
  WebFrame* web_frame = FrameFromXPath(web_view, UTF8ToUTF16(frame_xpath));
  if (!web_frame) {
    *error_msg = "Failed to locate frame by xpath: " + frame_xpath;
    return false;
  }
  scoped_ptr<base::Value> script_value =
      EvaluateScriptInFrame(web_frame, script);
  base::DictionaryValue* dict;
  if (!script_value.get() || !script_value->GetAsDictionary(&dict)) {
    *error_msg = "Script did not return an object";
    return false;
  }
  base::Value* error_value;
  if (dict->Get("error", &error_value)) {
    base::JSONWriter::Write(error_value, error_msg);
    return false;
  }
  result->reset(static_cast<base::DictionaryValue*>(script_value.release()));
  return true;
}

}  // namespace

bool AutomationRendererHelper::EvaluateScriptChain(
    const std::vector<ScriptEvaluationRequest>& script_chain,
    scoped_ptr<base::DictionaryValue>* result,
    std::string* error_msg) {
  CHECK(!script_chain.empty());
  WebView* web_view = render_view()->GetWebView();
  scoped_ptr<base::DictionaryValue> temp_result;
  for (size_t i = 0; i < script_chain.size(); ++i) {
    std::string args_utf8 = "null";
    if (temp_result.get())
      base::JSONWriter::Write(temp_result.get(), &args_utf8);
    std::string wrapper_script = base::StringPrintf(
        "(function(){return %s\n}).apply(null, [%s])",
        script_chain[i].script.c_str(), args_utf8.c_str());
    if (!EvaluateScriptChainHelper(web_view, wrapper_script,
                                   script_chain[i].frame_xpath,
                                   &temp_result, error_msg)) {
      return false;
    }
  }
  std::string result_str;
  base::JSONWriter::Write(temp_result.get(), &result_str);
  result->reset(temp_result.release());
  return true;
}

bool AutomationRendererHelper::ProcessMouseEvent(
    const AutomationMouseEvent& event,
    std::string* error_msg) {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view) {
    *error_msg = "Failed to process mouse event because webview does not exist";
    return false;
  }
  WebKit::WebMouseEvent mouse_event = event.mouse_event;
  if (!event.location_script_chain.empty()) {
    scoped_ptr<base::DictionaryValue> result;
    if (!EvaluateScriptChain(event.location_script_chain, &result, error_msg))
      return false;
    int x, y;
    if (!result->GetInteger("x", &x) ||
        !result->GetInteger("y", &y)) {
      *error_msg = "Script did not return an (x,y) location";
      return false;
    }
    mouse_event.x = x;
    mouse_event.y = y;
  }
  Send(new AutomationMsg_WillProcessMouseEventAt(
      routing_id(),
      gfx::Point(mouse_event.x, mouse_event.y)));
  web_view->handleInputEvent(mouse_event);
  return true;
}

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
void AutomationRendererHelper::OnHeapProfilerDump(const std::string& reason) {
  if (!::IsHeapProfilerRunning()) {
    return;
  }
  ::HeapProfilerDump(reason.c_str());
}
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

bool AutomationRendererHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AutomationRendererHelper, message,
                           deserialize_success)
    IPC_MESSAGE_HANDLER(AutomationMsg_SnapshotEntirePage, OnSnapshotEntirePage)
#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
    IPC_MESSAGE_HANDLER(AutomationMsg_HeapProfilerDump, OnHeapProfilerDump)
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
    IPC_MESSAGE_HANDLER(AutomationMsg_ProcessMouseEvent, OnProcessMouseEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  if (!deserialize_success) {
    LOG(ERROR) << "Failed to deserialize an IPC message";
  }
  return handled;
}

void AutomationRendererHelper::WillPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  Send(new AutomationMsg_WillPerformClientRedirect(
      routing_id(), frame->identifier(), interval));
}

void AutomationRendererHelper::DidCancelClientRedirect(WebFrame* frame) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}

void AutomationRendererHelper::DidCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}

void AutomationRendererHelper::OnProcessMouseEvent(
    const AutomationMouseEvent& event) {
  std::string error_msg;
  bool success = ProcessMouseEvent(event, &error_msg);
  Send(new AutomationMsg_ProcessMouseEventACK(
      routing_id(), success, error_msg));
}
