// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
const char* const kScrollIntoViewScript =
    "function(node) {\
    const rect = node.getBoundingClientRect();\
    if (rect.height < window.innerHeight) {\
      window.scrollBy({top: rect.top - window.innerHeight * 0.25, \
        behavior: 'smooth'});\
    } else {\
      window.scrollBy({top: rect.top, behavior: 'smooth'});\
    }\
    node.scrollIntoViewIfNeeded();\
  }";
}  // namespace

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      weak_ptr_factory_(this) {}

WebController::~WebController() {}

const GURL& WebController::GetUrl() {
  return web_contents_->GetLastCommittedURL();
}

void WebController::ClickElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, base::BindOnce(&WebController::OnFindElementForClick,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void WebController::OnFindElementForClick(
    base::OnceCallback<void(bool)> callback,
    std::string object_id) {
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to click.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnScrollIntoView,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     object_id));
}

void WebController::OnScrollIntoView(
    base::OnceCallback<void(bool)> callback,
    std::string object_id,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to scroll the element.";
    OnResult(false, std::move(callback));
    return;
  }

  devtools_client_->GetDOM()->GetBoxModel(
      dom::GetBoxModelParams::Builder().SetObjectId(object_id).Build(),
      base::BindOnce(&WebController::OnGetBoxModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetBoxModel(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::GetBoxModelResult> result) {
  if (!result || !result->GetModel() || !result->GetModel()->GetContent()) {
    DLOG(ERROR) << "Failed to get box model.";
    OnResult(false, std::move(callback));
    return;
  }

  // Click at the center of the element.
  const std::vector<double>* content_box = result->GetModel()->GetContent();
  DCHECK_EQ(content_box->size(), 8u);
  double x = ((*content_box)[0] + (*content_box)[2]) * 0.5;
  double y = ((*content_box)[3] + (*content_box)[5]) * 0.5;
  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchPressMoustEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), x,
                     y));
}

void WebController::OnDispatchPressMoustEvent(
    base::OnceCallback<void(bool)> callback,
    double x,
    double y,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchReleaseMoustEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMoustEvent(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  OnResult(true, std::move(callback));
}

void WebController::ElementExists(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, base::BindOnce(&WebController::OnFindElementForExist,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void WebController::OnFindElementForExist(
    base::OnceCallback<void(bool)> callback,
    std::string object_id) {
  OnResult(!object_id.empty(), std::move(callback));
}

void WebController::FindElement(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback) {
  devtools_client_->GetDOM()->Enable();
  devtools_client_->GetDOM()->GetDocument(base::BindOnce(
      &WebController::OnGetDocument, weak_ptr_factory_.GetWeakPtr(), selectors,
      std::move(callback)));
}

void WebController::OnGetDocument(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback,
    std::unique_ptr<dom::GetDocumentResult> result) {
  RecursiveFindElement(result->GetRoot()->GetNodeId(), 0, selectors,
                       std::move(callback));
}

void WebController::RecursiveFindElement(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback) {
  devtools_client_->GetDOM()->QuerySelectorAll(
      node_id, selectors[index],
      base::BindOnce(&WebController::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index, selectors,
                     std::move(callback)));
}

void WebController::OnQuerySelectorAll(
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback,
    std::unique_ptr<dom::QuerySelectorAllResult> result) {
  if (!result || !result->GetNodeIds() || result->GetNodeIds()->empty()) {
    std::move(callback).Run("");
    return;
  }

  if (result->GetNodeIds()->size() != 1) {
    DLOG(ERROR) << "Have " << result->GetNodeIds()->size()
                << " elements exist.";
    std::move(callback).Run("");
    return;
  }

  // Resolve and return object id of the element.
  if (selectors.size() == index + 1) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetNodeId(result->GetNodeIds()->front())
            .Build(),
        base::BindOnce(&WebController::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetNodeId(result->GetNodeIds()->front())
          .Build(),
      base::BindOnce(&WebController::OnDescribeNode,
                     weak_ptr_factory_.GetWeakPtr(),
                     result->GetNodeIds()->front(), index, selectors,
                     std::move(callback)));
}

void WebController::OnResolveNode(
    base::OnceCallback<void(std::string)> callback,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() ||
      result->GetObject()->GetObjectId().empty()) {
    DLOG(ERROR) << "Failed to resolve object id from node id.";
    std::move(callback).Run("");
    return;
  }

  std::move(callback).Run(result->GetObject()->GetObjectId());
}

void WebController::OnDescribeNode(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DLOG(ERROR) << "Failed to describe the node.";
    std::move(callback).Run("");
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
            base::BindOnce(&WebController::OnPushNodesByBackendIds,
                           weak_ptr_factory_.GetWeakPtr(), index, selectors,
                           std::move(callback)));
    return;
  }

  RecursiveFindElement(node_id, ++index, selectors, std::move(callback));
}

void WebController::OnPushNodesByBackendIds(
    size_t index,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(std::string)> callback,
    std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result) {
  DCHECK(result->GetNodeIds()->size() == 1);
  RecursiveFindElement(result->GetNodeIds()->front(), ++index, selectors,
                       std::move(callback));
}

void WebController::OnResult(bool result,
                             base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetDOM()->Disable();
  std::move(callback).Run(result);
}

void WebController::OnFindElementForFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::string object_id) {
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to focus on.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to focus on element.";
    OnResult(false, std::move(callback));
    return;
  }
  OnResult(true, std::move(callback));
}

void WebController::FillAddressForm(const std::string& guid,
                                    const std::vector<std::string>& selectors,
                                    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement fill address form operation.
  std::move(callback).Run(true);
}

void WebController::FillCardForm(const std::string& guid,
                                 const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement fill card form operation.
  std::move(callback).Run(true);
}

void WebController::SelectOption(const std::vector<std::string>& selectors,
                                 const std::string& selected_option,
                                 base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/806868): Implement select option operation.
  std::move(callback).Run(true);
}

void WebController::FocusElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(
      selectors,
      base::BindOnce(&WebController::OnFindElementForFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetFieldsValue(
    const std::vector<std::vector<std::string>>& selectors_list,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  // TODO(crbug.com/806868): Implement get fields value operation.
  std::vector<std::string> values;
  for (size_t i = 0; i < selectors_list.size(); i++) {
    values.emplace_back("");
  }
  std::move(callback).Run(values);
}

}  // namespace autofill_assistant.
