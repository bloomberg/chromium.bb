// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_web_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// static
std::unique_ptr<AssistantWebController>
AssistantWebController::CreateForWebContents(
    content::WebContents* web_contents) {
  return std::make_unique<AssistantWebController>(
      std::make_unique<AssistantDevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
}

AssistantWebController::AssistantWebController(
    std::unique_ptr<AssistantDevtoolsClient> devtools_client)
    : devtools_client_(std::move(devtools_client)), weak_ptr_factory_(this) {}

AssistantWebController::~AssistantWebController() {}

void AssistantWebController::ClickElement(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement click operation.
  std::move(callback).Run(true);
}

void AssistantWebController::ElementExists(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  devtools_client_->GetDOM()->Enable();
  devtools_client_->GetDOM()->GetDocument(base::BindOnce(
      &AssistantWebController::OnGetDocumentForElementExists,
      weak_ptr_factory_.GetWeakPtr(), selectors, std::move(callback)));
}

void AssistantWebController::OnGetDocumentForElementExists(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::GetDocumentResult> result) {
  RecursiveElementExists(result->GetRoot()->GetNodeId(), 0, selectors,
                         std::move(callback));
}

void AssistantWebController::RecursiveElementExists(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetDOM()->QuerySelectorAll(
      node_id, selectors[index],
      base::BindOnce(
          &AssistantWebController::OnQuerySelectorAllForElementExists,
          weak_ptr_factory_.GetWeakPtr(), index, selectors,
          std::move(callback)));
}

void AssistantWebController::OnQuerySelectorAllForElementExists(
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::QuerySelectorAllResult> result) {
  if (!result || !result->GetNodeIds() || result->GetNodeIds()->empty()) {
    OnResult(false, std::move(callback));
    return;
  }

  if (result->GetNodeIds()->size() != 1) {
    DLOG(WARNING) << "Have " << result->GetNodeIds()->size()
                  << " elements exist.";
    OnResult(false, std::move(callback));
    return;
  }

  if (selectors.size() == index + 1) {
    OnResult(true, std::move(callback));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetNodeId(result->GetNodeIds()->front())
          .Build(),
      base::BindOnce(&AssistantWebController::OnDescribeNodeForElementExists,
                     weak_ptr_factory_.GetWeakPtr(),
                     result->GetNodeIds()->front(), index, selectors,
                     std::move(callback)));
}

void AssistantWebController::OnDescribeNodeForElementExists(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DLOG(ERROR) << "Failed to describe the node.";
    OnResult(false, std::move(callback));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->HasContentDocument()) {
    backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()
        ->GetExperimental()
        ->PushNodesByBackendIdsToFrontend(
            dom::PushNodesByBackendIdsToFrontendParams::Builder()
                .SetBackendNodeIds(backend_ids)
                .Build(),
            base::BindOnce(&AssistantWebController::
                               OnPushNodesByBackendIdsForElementExists,
                           weak_ptr_factory_.GetWeakPtr(), index, selectors,
                           std::move(callback)));
    return;
  }

  RecursiveElementExists(node_id, ++index, selectors, std::move(callback));
}

void AssistantWebController::OnPushNodesByBackendIdsForElementExists(
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result) {
  DCHECK(result->GetNodeIds()->size() == 1);
  RecursiveElementExists(result->GetNodeIds()->front(), ++index, selectors,
                         std::move(callback));
}

void AssistantWebController::OnResult(bool result,
                                      base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetDOM()->Disable();
  std::move(callback).Run(result);
}

void AssistantWebController::FillAddressForm(
    const std::string& guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement fill address form operation.
  std::move(callback).Run(true);
}

void AssistantWebController::FillCardForm(
    const std::string& guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement fill card form operation.
  std::move(callback).Run(true);
}

}  // namespace autofill_assistant.
