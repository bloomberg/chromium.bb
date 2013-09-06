// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/content_watcher.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {

namespace {

class MutationHandler : public ChromeV8Extension {
 public:
  explicit MutationHandler(Dispatcher* dispatcher,
                           ChromeV8Context* context,
                           base::WeakPtr<ContentWatcher> content_watcher)
      : ChromeV8Extension(dispatcher, context),
        content_watcher_(content_watcher) {
    RouteFunction("FrameMutated",
                  base::Bind(&MutationHandler::FrameMutated,
                             base::Unretained(this)));
  }

 private:
  void FrameMutated(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (content_watcher_.get()) {
      content_watcher_->ScanAndNotify(
          WebKit::WebFrame::frameForContext(context()->v8_context()));
    }
  }

  base::WeakPtr<ContentWatcher> content_watcher_;
};

}  // namespace

ContentWatcher::ContentWatcher(Dispatcher* dispatcher)
    : weak_ptr_factory_(this),
      dispatcher_(dispatcher) {}
ContentWatcher::~ContentWatcher() {}

scoped_ptr<NativeHandler> ContentWatcher::MakeNatives(
    ChromeV8Context* context) {
  return scoped_ptr<NativeHandler>(new MutationHandler(
      dispatcher_, context, weak_ptr_factory_.GetWeakPtr()));
}

void ContentWatcher::OnWatchPages(
    const std::vector<std::string>& new_css_selectors) {
  if (new_css_selectors == css_selectors_)
    return;

  css_selectors_ = new_css_selectors;

  for (std::map<WebKit::WebFrame*,
                std::vector<base::StringPiece> >::iterator
           it = matching_selectors_.begin();
       it != matching_selectors_.end(); ++it) {
    WebKit::WebFrame* frame = it->first;
    if (!css_selectors_.empty())
      EnsureWatchingMutations(frame);

    // Make sure to replace the contents of it->second because it contains
    // dangling StringPieces that referred into the old css_selectors_ content.
    it->second = FindMatchingSelectors(frame);
  }

  // For each top-level frame, inform the browser about its new matching set of
  // selectors.
  struct NotifyVisitor : public content::RenderViewVisitor {
    explicit NotifyVisitor(ContentWatcher* watcher) : watcher_(watcher) {}
    virtual bool Visit(content::RenderView* view) OVERRIDE {
      watcher_->NotifyBrowserOfChange(view->GetWebView()->mainFrame());
      return true;  // Continue visiting.
    }
    ContentWatcher* watcher_;
  };
  NotifyVisitor visitor(this);
  content::RenderView::ForEach(&visitor);
}

void ContentWatcher::DidCreateDocumentElement(WebKit::WebFrame* frame) {
  // Make sure the frame is represented in the matching_selectors_ map.
  matching_selectors_[frame];

  if (!css_selectors_.empty()) {
    EnsureWatchingMutations(frame);
  }
}

void ContentWatcher::EnsureWatchingMutations(WebKit::WebFrame* frame) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Context::Scope context_scope(frame->mainWorldScriptContext());
  if (ModuleSystem* module_system = GetModuleSystem(frame)) {
    ModuleSystem::NativesEnabledScope scope(module_system);
    module_system->Require("contentWatcher");
  }
}

ModuleSystem* ContentWatcher::GetModuleSystem(WebKit::WebFrame* frame) const {
  ChromeV8Context* v8_context =
      dispatcher_->v8_context_set().GetByV8Context(
          frame->mainWorldScriptContext());

  if (!v8_context)
    return NULL;
  return v8_context->module_system();
}

void ContentWatcher::ScanAndNotify(WebKit::WebFrame* frame) {
  std::vector<base::StringPiece> new_matches = FindMatchingSelectors(frame);
  std::vector<base::StringPiece>& old_matches = matching_selectors_[frame];
  if (new_matches == old_matches)
    return;

  using std::swap;
  swap(old_matches, new_matches);
  NotifyBrowserOfChange(frame);
}

std::vector<base::StringPiece> ContentWatcher::FindMatchingSelectors(
    WebKit::WebFrame* frame) const {
  std::vector<base::StringPiece> result;
  WebKit::WebDocument document = frame->document();
  for (size_t i = 0; i < css_selectors_.size(); ++i) {
    WebKit::WebExceptionCode exception = 0;
    if (!document.querySelector(WebKit::WebString::fromUTF8(css_selectors_[i]),
                                exception).isNull())
      result.push_back(css_selectors_[i]);
    DCHECK_EQ(0, exception)
        << "We should already have checked that the selectors are valid.";
  }
  return result;
}

void ContentWatcher::NotifyBrowserOfChange(
    WebKit::WebFrame* changed_frame) const {
  WebKit::WebFrame* const top_frame = changed_frame->top();
  const WebKit::WebSecurityOrigin top_origin =
      top_frame->document().securityOrigin();
  // Want to aggregate matched selectors from all frames where an
  // extension with access to top_origin could run on the frame.
  if (!top_origin.canAccess(changed_frame->document().securityOrigin())) {
    // If the changed frame can't be accessed by the top frame, then
    // no change in it could affect the set of selectors we'd send back.
    return;
  }

  std::set<base::StringPiece> transitive_selectors;
  for (WebKit::WebFrame* frame = top_frame; frame;
       frame = frame->traverseNext(/*wrap=*/false)) {
    if (top_origin.canAccess(frame->document().securityOrigin())) {
      std::map<WebKit::WebFrame*,
               std::vector<base::StringPiece> >::const_iterator
          frame_selectors = matching_selectors_.find(frame);
      if (frame_selectors != matching_selectors_.end()) {
        transitive_selectors.insert(frame_selectors->second.begin(),
                                    frame_selectors->second.end());
      }
    }
  }
  std::vector<std::string> selector_strings;
  for (std::set<base::StringPiece>::const_iterator
           it = transitive_selectors.begin();
       it != transitive_selectors.end(); ++it)
    selector_strings.push_back(it->as_string());
  content::RenderView* view =
      content::RenderView::FromWebView(top_frame->view());
  view->Send(new ExtensionHostMsg_OnWatchedPageChange(
      view->GetRoutingID(), selector_strings));
}

}  // namespace extensions
