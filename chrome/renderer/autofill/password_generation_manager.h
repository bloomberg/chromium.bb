// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_MANAGER_H_
#define CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_MANAGER_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"

namespace autofill {

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (determines which fields are for account signup and
// fills in the generated passwords). Currently the WebKit part is not
// implemented.
class PasswordGenerationManager : public content::RenderViewObserver {
 public:
  explicit PasswordGenerationManager(content::RenderView* render_view);
  virtual ~PasswordGenerationManager();

 protected:
  // Returns true if this frame is one that we should consider analyzing.
  // Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeFrame(const WebKit::WebFrame& frame) const;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // RenderViewObserver:
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;

  // Message handlers.
  void OnPasswordAccepted(const string16& password);
  void OnPasswordGenerationEnabled(bool enabled);

  // True if password generation is enabled for the profile associated
  // with this renderer.
  bool enabled_;

  std::pair<WebKit::WebInputElement,
            std::vector<WebKit::WebInputElement> > account_creation_elements_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_MANAGER_H_
