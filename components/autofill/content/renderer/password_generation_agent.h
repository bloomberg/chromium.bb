// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_

#include <map>
#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebPasswordGeneratorClient.h"
#include "url/gurl.h"

namespace blink {
class WebCString;
class WebDocument;
}

namespace autofill {

struct FormData;
struct PasswordForm;

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationAgent : public content::RenderViewObserver,
                                public blink::WebPasswordGeneratorClient {
 public:
  explicit PasswordGenerationAgent(content::RenderView* render_view);
  virtual ~PasswordGenerationAgent();

 protected:
  // Returns true if this document is one that we should consider analyzing.
  // Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeDocument(const blink::WebDocument& document) const;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // RenderViewObserver:
  virtual void DidFinishDocumentLoad(blink::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(blink::WebFrame* frame) OVERRIDE;

  // WebPasswordGeneratorClient:
  virtual void openPasswordGenerator(blink::WebInputElement& element) OVERRIDE;

  // Message handlers.
  void OnFormNotBlacklisted(const PasswordForm& form);
  void OnPasswordAccepted(const base::string16& password);
  void OnAccountCreationFormsDetected(
      const std::vector<autofill::FormData>& forms);

  // Helper function to decide whether we should show password generation icon.
  void MaybeShowIcon();

  content::RenderView* render_view_;

  // Stores the origin of the account creation form we detected.
  scoped_ptr<PasswordForm> possible_account_creation_form_;

  // Stores the origins of the password forms confirmed not to be blacklisted
  // by the browser. A form can be blacklisted if a user chooses "never save
  // passwords for this site".
  std::vector<GURL> not_blacklisted_password_form_origins_;

  // Stores each password form for which the Autofill server classifies one of
  // the form's fields as an ACCOUNT_CREATION_PASSWORD. These forms will
  // not be sent if the feature is disabled.
  std::vector<autofill::FormData> generation_enabled_forms_;

  std::vector<blink::WebInputElement> passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
