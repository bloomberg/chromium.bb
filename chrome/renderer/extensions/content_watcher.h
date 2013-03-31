// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CONTENT_WATCHER_H_
#define CHROME_RENDERER_EXTENSIONS_CONTENT_WATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "v8/include/v8.h"

namespace WebKit {
class WebFrame;
}

namespace extensions {
class Dispatcher;
class Extension;
class NativeHandler;

// Watches the content of WebFrames to notify extensions when they match various
// patterns.  This class tracks the set of relevant patterns (set by
// ExtensionMsg_WatchPages) and the set that match on each WebFrame, and sends a
// ExtensionHostMsg_OnWatchedPageChange whenever a RenderView's set changes.
//
// There's one ContentWatcher per Dispatcher rather than per RenderView because
// WebFrames can move between RenderViews through adoptNode.
class ContentWatcher {
 public:
  explicit ContentWatcher(Dispatcher* dispatcher);
  ~ContentWatcher();

  // Returns the callback to call on a frame change.
  scoped_ptr<NativeHandler> MakeNatives(v8::Handle<v8::Context> v8_context);

  // Handler for ExtensionMsg_WatchPages.
  void OnWatchPages(const std::vector<std::string>& css_selectors);

  // Registers the MutationObserver to call back into this object whenever the
  // content of |frame| changes.
  void DidCreateDocumentElement(WebKit::WebFrame* frame);

  // Scans *frame for the current set of interesting CSS selectors, and if
  // they've changed sends ExtensionHostMsg_OnWatchedPageChange back to the
  // RenderViewHost that owns the frame.
  void ScanAndNotify(WebKit::WebFrame* frame);

 private:
  void EnsureWatchingMutations(WebKit::WebFrame* frame);

  ModuleSystem* GetModuleSystem(WebKit::WebFrame* frame) const;
  std::vector<base::StringPiece> FindMatchingSelectors(
      WebKit::WebFrame* frame) const;

  // Given that we saw a change in the CSS selectors that |changed_frame|
  // matched, tell the browser about the new set of matching selectors in its
  // top-level page.  We filter this so that if an extension were to be granted
  // activeTab permission on that top-level page, we only send CSS selectors for
  // frames that it could run on.
  void NotifyBrowserOfChange(WebKit::WebFrame* changed_frame) const;

  base::WeakPtrFactory<ContentWatcher> weak_ptr_factory_;
  Dispatcher* dispatcher_;

  // If any of these selectors match on a page, we need to send an
  // ExtensionHostMsg_OnWatchedPageChange back to the browser.
  std::vector<std::string> css_selectors_;

  // Maps live WebFrames to the set of CSS selectors they match.  This lets us
  // traverse a top-level frame's sub-frames without rescanning them all each
  // time any one changes.
  //
  // The StringPieces point into css_selectors_ above, so when it changes, they
  // all need to be regenerated.
  std::map<WebKit::WebFrame*,
           std::vector<base::StringPiece> > matching_selectors_;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CONTENT_WATCHER_H_
