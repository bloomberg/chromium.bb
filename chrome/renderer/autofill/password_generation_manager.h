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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextFieldDecoratorClient.h"

namespace WebKit {
class WebCString;
class WebDocument;
}

namespace autofill {

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationManager : public content::RenderViewObserver,
                                  public WebKit::WebTextFieldDecoratorClient {
 public:
  explicit PasswordGenerationManager(content::RenderView* render_view);
  virtual ~PasswordGenerationManager();

 protected:
  // Returns true if this document is one that we should consider analyzing.
  // Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeDocument(const WebKit::WebDocument& document) const;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // RenderViewObserver:
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;

  // WebTextFieldDecoratorClient:
  virtual bool shouldAddDecorationTo(
      const WebKit::WebInputElement& element) OVERRIDE;
  virtual bool visibleByDefault() OVERRIDE;
  virtual WebKit::WebCString imageNameForNormalState() OVERRIDE;
  virtual WebKit::WebCString imageNameForDisabledState() OVERRIDE;
  virtual WebKit::WebCString imageNameForReadOnlyState() OVERRIDE;
  virtual void handleClick(WebKit::WebInputElement& element) OVERRIDE;
  virtual void willDetach(const WebKit::WebInputElement& element) OVERRIDE;

  bool IsAccountCreationForm(const WebKit::WebFormElement& form,
                             std::vector<WebKit::WebInputElement>* passwords);

  // Message handlers.
  void OnPasswordAccepted(const string16& password);
  void OnPasswordGenerationEnabled(bool enabled);

  // True if password generation is enabled for the profile associated
  // with this renderer.
  bool enabled_;

  std::vector<WebKit::WebInputElement> passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_MANAGER_H_
