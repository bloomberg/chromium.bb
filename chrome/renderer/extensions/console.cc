// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/console.h"

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace extensions {
namespace console {

namespace {

class ByContextFinder : public content::RenderViewVisitor {
 public:
  static content::RenderView* Find(v8::Handle<v8::Context> context) {
    ByContextFinder finder(context);
    content::RenderView::ForEach(&finder);
    return finder.found_;
  }

 private:
  explicit ByContextFinder(v8::Handle<v8::Context> context)
      : context_(context), found_(NULL) {
  }

  virtual bool Visit(content::RenderView* render_view) OVERRIDE {
    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    if (helper &&
        helper->dispatcher()->v8_context_set().GetByV8Context(context_)) {
      found_ = render_view;
    }
    return !found_;
  }

  v8::Handle<v8::Context> context_;
  content::RenderView* found_;

  DISALLOW_COPY_AND_ASSIGN(ByContextFinder);
};

}  // namespace

void Debug(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_DEBUG, message);
}

void Log(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_LOG, message);
}

void Warn(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

void Error(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void AddMessage(content::RenderView* render_view,
                content::ConsoleMessageLevel level,
                const std::string& message) {
  WebKit::WebView* web_view = render_view->GetWebView();
  if (!web_view || !web_view->mainFrame())
    return;
  WebKit::WebConsoleMessage::Level target_level =
      WebKit::WebConsoleMessage::LevelLog;
  switch (level) {
    case content::CONSOLE_MESSAGE_LEVEL_DEBUG:
      target_level = WebKit::WebConsoleMessage::LevelDebug;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_LOG:
      target_level = WebKit::WebConsoleMessage::LevelLog;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_WARNING:
      target_level = WebKit::WebConsoleMessage::LevelWarning;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_ERROR:
      target_level = WebKit::WebConsoleMessage::LevelError;
      break;
  }
  web_view->mainFrame()->addMessageToConsole(
      WebKit::WebConsoleMessage(target_level, ASCIIToUTF16(message)));
}

void Debug(v8::Handle<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_DEBUG, message);
}

void Log(v8::Handle<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_LOG, message);
}

void Warn(v8::Handle<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

void Error(v8::Handle<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void AddMessage(v8::Handle<v8::Context> context,
                content::ConsoleMessageLevel level,
                const std::string& message) {
  if (context.IsEmpty()) {
    LOG(WARNING) << "Could not log \"" << message << "\": no context given";
    return;
  }
  content::RenderView* render_view = ByContextFinder::Find(context);
  if (!render_view) {
    LOG(WARNING) << "Could not log \"" << message << "\": no render view found";
    return;
  }
  AddMessage(render_view, level, message);
}

}  // namespace console
}  // namespace extensions
