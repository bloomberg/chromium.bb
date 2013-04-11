// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_RENDERER_PASSWORD_GENERATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_RENDERER_PASSWORD_GENERATION_MANAGER_H_

#include <map>
#include <utility>
#include <vector>

#include "content/public/renderer/render_view_observer.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextFieldDecoratorClient.h"

namespace WebKit {
class WebCString;
class WebDocument;
}

namespace content {
struct PasswordForm;
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
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;

  // WebTextFieldDecoratorClient:
  virtual bool shouldAddDecorationTo(
      const WebKit::WebInputElement& element) OVERRIDE;
  virtual bool visibleByDefault() OVERRIDE;
  virtual WebKit::WebCString imageNameForNormalState() OVERRIDE;
  virtual WebKit::WebCString imageNameForDisabledState() OVERRIDE;
  virtual WebKit::WebCString imageNameForReadOnlyState() OVERRIDE;
  virtual WebKit::WebCString imageNameForHoverState() OVERRIDE;
  virtual void handleClick(WebKit::WebInputElement& element) OVERRIDE;
  virtual void willDetach(const WebKit::WebInputElement& element) OVERRIDE;

  // Message handlers.
  void OnFormNotBlacklisted(const content::PasswordForm& form);
  void OnPasswordAccepted(const base::string16& password);
  void OnPasswordGenerationEnabled(bool enabled);

  // Helper function to decide whether we should show password generation icon.
  void MaybeShowIcon();

  content::RenderView* render_view_;

  // True if password generation is enabled for the profile associated
  // with this renderer.
  bool enabled_;

  // Stores the origin of the account creation form we detected.
  GURL account_creation_form_origin_;

  // Stores the origins of the password forms confirmed not to be blacklisted
  // by the browser. A form can be blacklisted if a user chooses "never save
  // passwords for this site".
  std::vector<GURL> not_blacklisted_password_form_origins_;

  std::vector<WebKit::WebInputElement> passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_RENDERER_PASSWORD_GENERATION_MANAGER_H_
