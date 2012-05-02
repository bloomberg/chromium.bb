// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_DELEGATE_H_
#pragma once

#include <string>

namespace content {

// Clients that want to use default DevTools front-end implementation should
// implement this interface to provide access to the embedding browser from
// the front-end.
class DevToolsFrontendHostDelegate {
 public:
  virtual ~DevToolsFrontendHostDelegate() {}

  // Should bring DevTools window to front.
  virtual void ActivateWindow() = 0;

  // Closes DevTools front-end window.
  virtual void CloseWindow() = 0;

  // Moves DevTols front-end windo.
  virtual void MoveWindow(int x, int y) = 0;

  // Attaches DevTools front-end to the inspected page.
  virtual void DockWindow() = 0;

  // Detaches DevTools front-end from the inspected page and places it in its
  // own window.
  virtual void UndockWindow() = 0;

  // Specifies side for devtools to dock to.
  virtual void SetDockSide(const std::string& side) = 0;

  // Opens given |url| in a new contents.
  virtual void OpenInNewTab(const std::string& url) = 0;

  // Saves given |content| associated with the given |url|. Optionally showing
  // Save As dialog.
  virtual void SaveToFile(const std::string& url,
                          const std::string& content,
                          bool save_as) = 0;

  // Appends given |content| to the file that has been associated with the
  // given |url| by SaveToFile method.
  virtual void AppendToFile(const std::string& url,
                            const std::string& content) = 0;

  // This method is called when the contents inspected by this devtools frontend
  // is closing.
  virtual void InspectedContentsClosing() = 0;

  // This method is called when the contents inspected by this devtools frontend
  // is navigating to |url|.
  virtual void FrameNavigating(const std::string& url) = 0;

  // Invoked when the contents inspected by this devtools frontend is replaced
  // by another contents. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  virtual void ContentsReplaced(WebContents* new_contents) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_DELEGATE_H_
