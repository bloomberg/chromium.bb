// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

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

// Javascript where $1 will be replaced with the option value. Also fires a
// "change" event to trigger any listeners. Changing the index directly does not
// trigger this.
const char* const kSelectOptionScript =
    R"(function() {
    const value = '$1'.toUpperCase();
      var found = false;
      for (var i = 0; i < this.options.length; ++i) {
        const label = this.options[i].label.toUpperCase();
        if (label.length > 0 && label.startsWith(value)) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return { result: false };
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return { result: true };
    })";

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
    std::unique_ptr<FindElementResult> result) {
  if (result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to click.";
    OnResult(false, std::move(callback));
    return;
  }

  ClickObject(result->object_id, std::move(callback));
}

void WebController::ClickObject(const std::string& object_id,
                                base::OnceCallback<void(bool)> callback) {
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
    std::unique_ptr<FindElementResult> result) {
  OnResult(!result->object_id.empty(), std::move(callback));
}

void WebController::FindElement(const std::vector<std::string>& selectors,
                                FindElementCallback callback) {
  devtools_client_->GetDOM()->Enable();
  devtools_client_->GetDOM()->GetDocument(base::BindOnce(
      &WebController::OnGetDocument, weak_ptr_factory_.GetWeakPtr(), selectors,
      std::move(callback)));
}

void WebController::OnGetDocument(
    const std::vector<std::string>& selectors,
    FindElementCallback callback,
    std::unique_ptr<dom::GetDocumentResult> result) {
  std::unique_ptr<FindElementResult> element_result =
      std::make_unique<FindElementResult>();
  element_result->container_frame_host = web_contents_->GetMainFrame();
  element_result->container_frame_selector_index = 0;
  element_result->object_id = "";
  RecursiveFindElement(result->GetRoot()->GetNodeId(), 0, selectors,
                       std::move(element_result), std::move(callback));
}

void WebController::RecursiveFindElement(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback) {
  devtools_client_->GetDOM()->QuerySelectorAll(
      node_id, selectors[index],
      base::BindOnce(&WebController::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index, selectors,
                     std::move(element_result), std::move(callback)));
}

void WebController::OnQuerySelectorAll(
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::QuerySelectorAllResult> result) {
  if (!result || !result->GetNodeIds() || result->GetNodeIds()->empty()) {
    std::move(callback).Run(std::move(element_result));
    return;
  }

  if (result->GetNodeIds()->size() != 1) {
    DLOG(ERROR) << "Have " << result->GetNodeIds()->size()
                << " elements exist.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  // Resolve and return object id of the element.
  if (selectors.size() == index + 1) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetNodeId(result->GetNodeIds()->front())
            .Build(),
        base::BindOnce(&WebController::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(element_result), std::move(callback)));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetNodeId(result->GetNodeIds()->front())
          .Build(),
      base::BindOnce(&WebController::OnDescribeNode,
                     weak_ptr_factory_.GetWeakPtr(),
                     result->GetNodeIds()->front(), index, selectors,
                     std::move(element_result), std::move(callback)));
}

void WebController::OnResolveNode(
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() ||
      result->GetObject()->GetObjectId().empty()) {
    DLOG(ERROR) << "Failed to resolve object id from node id.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  element_result->object_id = result->GetObject()->GetObjectId();
  std::move(callback).Run(std::move(element_result));
}

void WebController::OnDescribeNode(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DLOG(ERROR) << "Failed to describe the node.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->HasContentDocument()) {
    backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());

    element_result->container_frame_selector_index = index;

    // Find out the corresponding render frame host through document url and
    // name.
    // TODO(crbug.com/806868): Use more attributes to find out the render frame
    // host if name and document url are not enough to uniquely identify it.
    std::string frame_name;
    if (node->HasAttributes()) {
      const std::vector<std::string>* attributes = node->GetAttributes();
      for (size_t i = 0; i < attributes->size();) {
        if ((*attributes)[i] == "name") {
          frame_name = (*attributes)[i + 1];
          break;
        }
        // Jump two positions since attribute name and value are always paired.
        i = i + 2;
      }
    }
    element_result->container_frame_host = FindCorrespondingRenderFrameHost(
        frame_name, node->GetContentDocument()->GetDocumentURL());
    if (!element_result->container_frame_host) {
      DLOG(ERROR) << "Failed to find corresponding owner frame.";
      std::move(callback).Run(std::move(element_result));
      return;
    }
  } else if (node->HasFrameId()) {
    // TODO(crbug.com/806868): Support out-of-process iframe.
    DLOG(WARNING) << "The element is inside an OOPIF.";
    std::move(callback).Run(std::move(element_result));
    return;
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
                           std::move(element_result), std::move(callback)));
    return;
  }

  RecursiveFindElement(node_id, ++index, selectors, std::move(element_result),
                       std::move(callback));
}

content::RenderFrameHost* WebController::FindCorrespondingRenderFrameHost(
    std::string name,
    std::string document_url) {
  content::RenderFrameHost* ret_frame = nullptr;
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetFrameName() == name &&
        frame->GetLastCommittedURL().spec() == document_url) {
      DCHECK(!ret_frame);
      ret_frame = frame;
    }
  }

  return ret_frame;
}

void WebController::OnPushNodesByBackendIds(
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result) {
  DCHECK(result->GetNodeIds()->size() == 1);
  RecursiveFindElement(result->GetNodeIds()->front(), ++index, selectors,
                       std::move(element_result), std::move(callback));
}

void WebController::OnResult(bool result,
                             base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetDOM()->Disable();
  std::move(callback).Run(result);
}

void WebController::OnFindElementForFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to focus on.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetObjectId(element_result->object_id)
                            .Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
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
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(), guid, selectors,
                             std::move(callback)));
}

void WebController::OnFindElementForFillingForm(
    const std::string& autofill_data_guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element for filling the form.";
    OnResult(false, std::move(callback));
    return;
  }

  ClickObject(element_result->object_id,
              base::BindOnce(&WebController::OnClickObjectForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(), autofill_data_guid,
                             selectors, std::move(callback),
                             std::move(element_result)));
}

void WebController::OnClickObjectForFillingForm(
    const std::string& autofill_data_guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result,
    bool click_result) {
  if (!click_result) {
    DLOG(ERROR) << "Failed to click the element for filling form.";
    OnResult(false, std::move(callback));
  }

  std::vector<std::string> element_selectors = selectors;
  DCHECK(element_selectors.size() >
         element_result->container_frame_selector_index);
  for (size_t i = element_result->container_frame_selector_index; i > 0; i++) {
    element_selectors.erase(element_selectors.begin());
  }
  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      element_selectors,
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFillingForm,
                     weak_ptr_factory_.GetWeakPtr(), autofill_data_guid,
                     std::move(callback),
                     element_result->container_frame_host));
}

void WebController::OnGetFormAndFieldDataForFillingForm(
    const std::string& autofill_data_guid,
    base::OnceCallback<void(bool)> callback,
    content::RenderFrameHost* container_frame_host,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (form_data.fields.empty()) {
    DLOG(ERROR) << "Failed to get form data to fill form.";
    OnResult(false, std::move(callback));
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(container_frame_host);
  if (!driver) {
    DLOG(ERROR) << "Failed to get the autofill driver.";
    OnResult(false, std::move(callback));
    return;
  }
  driver->autofill_manager()->FillProfileForm(autofill_data_guid, form_data,
                                              form_field);
  OnResult(true, std::move(callback));
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
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForSelectOption,
                             weak_ptr_factory_.GetWeakPtr(), selected_option,
                             std::move(callback)));
}

void WebController::OnFindElementForSelectOption(
    const std::string& selected_option,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to select an option.";
    OnResult(false, std::move(callback));
    return;
  }

  std::string select_option_script = base::ReplaceStringPlaceholders(
      kSelectOptionScript, {selected_option}, nullptr);

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(select_option_script))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to select option.";
    OnResult(false, std::move(callback));
    return;
  }
  OnResult(true, std::move(callback));
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

}  // namespace autofill_assistant
