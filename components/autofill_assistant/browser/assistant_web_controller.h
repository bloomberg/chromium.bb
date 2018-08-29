// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/devtools/assistant_devtools_client.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"

namespace content {
class WebContents;
}

namespace autofill_assistant {
// Controller to interact with the web pages.
class AssistantWebController {
 public:
  // Create assistant web controller for a given |web_contents|.
  static std::unique_ptr<AssistantWebController> CreateForWebContents(
      content::WebContents* web_contents);

  explicit AssistantWebController(
      std::unique_ptr<AssistantDevtoolsClient> devtools_client);
  virtual ~AssistantWebController();

  // Perform a mouse left button click on the element given by |selectors| and
  // return the result through callback.
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

  // Check whether the element given by |selectors| exists on the web page.
  virtual void ElementExists(const std::vector<std::string>& selectors,
                             base::OnceCallback<void(bool)> callback);

  // Fill the address form given by |selectors| with the given address |guid| in
  // personal data manager.
  virtual void FillAddressForm(const std::string& guid,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback);

  // Fill the card form given by |selectors| with the given card |guid| in
  // personal data manager.
  virtual void FillCardForm(const std::string& guid,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback);

 private:
  void OnFindElementForClick(base::OnceCallback<void(bool)> callback,
                             std::string object_id);
  void OnScrollIntoView(base::OnceCallback<void(bool)> callback,
                        std::string object_id,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnGetBoxModel(base::OnceCallback<void(bool)> callback,
                     std::unique_ptr<dom::GetBoxModelResult> result);
  void OnDispatchPressMoustEvent(
      base::OnceCallback<void(bool)> callback,
      double x,
      double y,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMoustEvent(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnFindElementForExist(base::OnceCallback<void(bool)> callback,
                             std::string object_id);
  void FindElement(const std::vector<std::string>& selectors,
                   base::OnceCallback<void(std::string)> callback);
  void OnGetDocument(const std::vector<std::string>& selectors,
                     base::OnceCallback<void(std::string)> callback,
                     std::unique_ptr<dom::GetDocumentResult> result);
  void RecursiveFindElement(int node_id,
                            size_t index,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(std::string)> callback);
  void OnQuerySelectorAll(size_t index,
                          const std::vector<std::string>& selectors,
                          base::OnceCallback<void(std::string)> callback,
                          std::unique_ptr<dom::QuerySelectorAllResult> result);
  void OnResolveNode(base::OnceCallback<void(std::string)> callback,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  void OnDescribeNode(int node_id,
                      size_t index,
                      const std::vector<std::string>& selectors,
                      base::OnceCallback<void(std::string)> callback,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnPushNodesByBackendIds(
      size_t index,
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(std::string)> callback,
      std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result);
  void OnResult(bool result, base::OnceCallback<void(bool)> callback);

  std::unique_ptr<AssistantDevtoolsClient> devtools_client_;

  base::WeakPtrFactory<AssistantWebController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AssistantWebController);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_
